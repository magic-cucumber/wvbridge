#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import <WebKit/WebKit.h>

#import <jni.h>
#import <jawt.h>
#import <jawt_md.h>

#include <algorithm>
#include <cstdint>

#import "history_listener.h"
#import "page_loading.h"
#import "utils.h"
#import "url_listener.h"
#import "ui_delegate.h"

struct WebViewContext {
    WKWebView *webView = nil;
    NSWindow *hostWindow = nil;
    NSView *hostView = nil;
    CALayer *rootLayer = nil;
    CALayer *windowLayer = nil;
    PageLoadingObserver *pageLoadingObserver = nil;
    HistoryChangeObserver *historyChangeObserver = nil;
    URLChangeObserver *urlChangeObserver = nil;
};

static NSView *find_view_for_layer(CALayer *layer) {
    for (CALayer *cursor = layer; cursor != nil; cursor = cursor.superlayer) {
        id delegate = cursor.delegate;
        if ([delegate isKindOfClass:[NSView class]]) {
            return (NSView *) delegate;
        }
    }
    return nil;
}

static bool ensure_host_view_attached(WebViewContext *ctx) {
    if (!ctx || !ctx->webView) return false;

    NSView *hostView = ctx->hostView;
    if (!hostView || ctx->webView.superview != hostView || hostView.window == nil) {
        hostView = find_view_for_layer(ctx->windowLayer);
        if (!hostView && ctx->rootLayer) {
            hostView = find_view_for_layer(ctx->rootLayer.superlayer);
        }
        if (!hostView) return false;

        ctx->hostWindow = hostView.window;
        if (ctx->hostWindow.contentView) {
            hostView = ctx->hostWindow.contentView;
        }
        ctx->hostView = hostView;
        [ctx->webView removeFromSuperview];
        [hostView addSubview:ctx->webView positioned:NSWindowAbove relativeTo:nil];
    } else {
        ctx->hostWindow = hostView.window;
    }

    return ctx->hostView != nil && ctx->hostWindow != nil;
}

API_EXPORT(jlong, initAndAttach) {
    // JAWT surface layers
    JAWT awt;
    awt.version = JAWT_VERSION_1_4 | JAWT_MACOSX_USE_CALAYER;
    if (JAWT_GetAWT(env, &awt) == JNI_FALSE) return 0;

    JAWT_DrawingSurface *ds = awt.GetDrawingSurface(env, thiz);
    if (!ds) return 0;

    jint lock = ds->Lock(ds);
    if (lock & JAWT_LOCK_ERROR) {
        awt.FreeDrawingSurface(ds);
        return 0;
    }

    JAWT_DrawingSurfaceInfo *dsi = ds->GetDrawingSurfaceInfo(ds);
    if (!dsi) {
        ds->Unlock(ds);
        awt.FreeDrawingSurface(ds);
        return 0;
    }

    id <JAWT_SurfaceLayers> surfaceLayers = (__bridge id <JAWT_SurfaceLayers>) dsi->platformInfo;
    if (!surfaceLayers) {
        ds->FreeDrawingSurfaceInfo(dsi);
        ds->Unlock(ds);
        awt.FreeDrawingSurface(ds);
        return 0;
    }
    JavaVM *jvm = nil;
    if (env->GetJavaVM(&jvm) != JNI_OK) {
        ds->FreeDrawingSurfaceInfo(dsi);
        ds->Unlock(ds);
        awt.FreeDrawingSurface(ds);
        return 0;
    }
    WebViewContext *ctx = new WebViewContext();

    runOnMainSync(^{
        ctx->rootLayer = [[CALayer alloc] init];
        ctx->windowLayer = [surfaceLayers.windowLayer retain];

        WKWebViewConfiguration *config = [[WKWebViewConfiguration alloc] init];
        config.defaultWebpagePreferences.allowsContentJavaScript = YES;
        config.preferences.javaScriptCanOpenWindowsAutomatically = YES;

        WKWebView *wv = [[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 0, 0)
                                           configuration:config];
        wv.customUserAgent = @"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/15.0 Safari/605.1.15";
        wv.hidden = NO;
        wv.UIDelegate = [[AllowAllUIDelegate alloc] init];
        ctx->pageLoadingObserver = [[PageLoadingObserver alloc] initWithJVM:jvm];
        ctx->historyChangeObserver = [[HistoryChangeObserver alloc] initWithJVM:jvm];
        ctx->urlChangeObserver = [[URLChangeObserver alloc] initWithJVM:jvm];
        wv.navigationDelegate = ctx->pageLoadingObserver;
        [wv addObserver:ctx->pageLoadingObserver
             forKeyPath:@"estimatedProgress"
                options:NSKeyValueObservingOptionNew
                context:nil];
        [wv addObserver:ctx->urlChangeObserver
             forKeyPath:@"URL"
                options:NSKeyValueObservingOptionNew
                context:nil];
        [wv addObserver:ctx->historyChangeObserver
             forKeyPath:@"canGoBack"
                options:NSKeyValueObservingOptionNew
                context:nil];
        [wv addObserver:ctx->historyChangeObserver
             forKeyPath:@"canGoForward"
                options:NSKeyValueObservingOptionNew
                context:nil];

        ctx->webView = wv;

        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        surfaceLayers.layer = ctx->rootLayer;
        [CATransaction commit];
        [CATransaction flush];

        ensure_host_view_attached(ctx);
    });

    ds->FreeDrawingSurfaceInfo(dsi);
    ds->Unlock(ds);
    awt.FreeDrawingSurface(ds);

    return (jlong) (uintptr_t) ctx;
}

API_EXPORT(void, update, jlong handle, jint w, jint h, jint x, jint y) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    if (!ctx) return;

    runOnMainAsync(^{
        if (!ctx) return;
        if (w <= 0 || h <= 0) return;

        CGRect layerFrame = CGRectMake((CGFloat) x, (CGFloat) y, (CGFloat) w, (CGFloat) h);
        if (ctx->windowLayer) {
            layerFrame.origin.y = ctx->windowLayer.bounds.size.height - layerFrame.origin.y - layerFrame.size.height;
        }
        ctx->rootLayer.frame = layerFrame;

        if (!ctx->webView) return;

        NSRect target = NSMakeRect(
            (CGFloat) x,
            ctx->hostView.isFlipped
                ? (CGFloat) y
                : ctx->hostView.bounds.size.height - (CGFloat) y - (CGFloat) h,
            (CGFloat) w,
            (CGFloat) h
        );

        ctx->webView.frame = target;
        [ctx->webView setNeedsLayout:YES];
        [ctx->webView layoutSubtreeIfNeeded];
    });
}

API_EXPORT(void, close0, jlong handle) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    if (!ctx) return;

    // AWT 相关清理（解绑 surfaceLayers.layer）只有在“正确上下文”下才执行。
    // 这里的“正确上下文”= 组件可用（displayable）且能拿到 SurfaceLayers，并且其 layer 正是 ctx->rootLayer。
    do {
        if (!env || !thiz) break;
        if (!ctx->rootLayer) break;

        jclass compCls = env->FindClass("java/awt/Component");
        if (!compCls) break;
        if (!env->IsInstanceOf(thiz, compCls)) {
            env->DeleteLocalRef(compCls);
            break;
        }
        jmethodID isDisplayableMID = env->GetMethodID(compCls, "isDisplayable", "()Z");
        env->DeleteLocalRef(compCls);
        if (!isDisplayableMID) break;

        jboolean displayable = env->CallBooleanMethod(thiz, isDisplayableMID);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            break;
        }
        if (displayable == JNI_FALSE) break;

        JAWT awt;
        awt.version = JAWT_VERSION_1_4 | JAWT_MACOSX_USE_CALAYER;
        if (JAWT_GetAWT(env, &awt) == JNI_FALSE) break;

        JAWT_DrawingSurface *ds = awt.GetDrawingSurface(env, thiz);
        if (!ds) break;

        jint lock = ds->Lock(ds);
        if (lock & JAWT_LOCK_ERROR) {
            awt.FreeDrawingSurface(ds);
            break;
        }

        JAWT_DrawingSurfaceInfo *dsi = ds->GetDrawingSurfaceInfo(ds);
        if (!dsi) {
            ds->Unlock(ds);
            awt.FreeDrawingSurface(ds);
            break;
        }

        id <JAWT_SurfaceLayers> surfaceLayers = (__bridge id <JAWT_SurfaceLayers>) dsi->platformInfo;
        if (!surfaceLayers) {
            ds->FreeDrawingSurfaceInfo(dsi);
            ds->Unlock(ds);
            awt.FreeDrawingSurface(ds);
            break;
        }

        if (surfaceLayers.layer != ctx->rootLayer) {
            ds->FreeDrawingSurfaceInfo(dsi);
            ds->Unlock(ds);
            awt.FreeDrawingSurface(ds);
            break;
        }

        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        [ctx->rootLayer release];
        ctx->rootLayer = nil;
        [CATransaction commit];
        [CATransaction flush];

        ds->FreeDrawingSurfaceInfo(dsi);
        ds->Unlock(ds);
        awt.FreeDrawingSurface(ds);
    } while (false);

    // UI 清理放在主线程。
    runOnMainSync(^{
        if (!ctx) return;

        if (ctx->webView) {
            if (ctx->urlChangeObserver != nil) {
                [ctx->webView removeObserver:ctx->urlChangeObserver forKeyPath:@"URL"];
            }
            if (ctx->pageLoadingObserver != nil) {
                [ctx->webView removeObserver:ctx->pageLoadingObserver forKeyPath:@"estimatedProgress"];
            }
            if (ctx->historyChangeObserver != nil) {
                [ctx->webView removeObserver:ctx->historyChangeObserver forKeyPath:@"canGoBack"];
                [ctx->webView removeObserver:ctx->historyChangeObserver forKeyPath:@"canGoForward"];
            }

            [ctx->webView.UIDelegate release];
            ctx->webView.UIDelegate = nil;
            ctx->webView.navigationDelegate = nil;
            [ctx->webView removeFromSuperview];
            ctx->webView = nil;
        }

        ctx->hostView = nil;
        ctx->hostWindow = nil;
        [ctx->windowLayer release];
        ctx->windowLayer = nil;
        ctx->rootLayer = nil;

        [ctx->pageLoadingObserver release];
        ctx->pageLoadingObserver = nil;
        [ctx->historyChangeObserver release];
        ctx->historyChangeObserver = nil;
        [ctx->urlChangeObserver release];
        ctx->urlChangeObserver = nil;
    });

    delete ctx;

}

API_EXPORT(void, requestFocus0, jlong handle) {
    (void) env;
    (void) thiz;
    (void) handle;
}

API_EXPORT(void, loadUrl, jlong handle, jstring url) {
    const char *nativeString = env->GetStringUTFChars(url, nullptr);
    if (nativeString == nullptr) {
        return;
    }

    NSString *result = [NSString stringWithUTF8String:nativeString];
    env->ReleaseStringUTFChars(url, nativeString);

    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    if (!ctx) return;

    runOnMainAsync(^{
        if (!ctx) return;

        [ctx->webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:result]]];
    });
}

API_EXPORT(void, refresh, jlong handle) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    if (!ctx) return;

    __block bool ok = true;
    runOnMainSync(^{
        if (!ctx || !ctx->webView) {
            ok = false;
            return;
        }

        [ctx->webView reload];
    });
    if (!ok) {
        throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
    }
}

API_EXPORT(void, stop, jlong handle) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    if (!ctx) return;

    __block bool ok = true;
    runOnMainSync(^{
        if (!ctx || !ctx->webView) {
            ok = false;
            return;
        }

        [ctx->webView stopLoading];
    });
    if (!ok) {
        throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
    }
}

API_EXPORT(jboolean, goBack, jlong handle) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return JNI_FALSE;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    if (!ctx) return JNI_FALSE;

    __block BOOL can_go_back_after = NO;
    __block bool ok = true;
    __block bool cannot_go_back = false;
    runOnMainSync(^{
        if (!ctx || !ctx->webView) {
            ok = false;
            return;
        }
        if (![ctx->webView canGoBack]) {
            ok = false;
            cannot_go_back = true;
            return;
        }

        [ctx->webView goBack];
        can_go_back_after = [ctx->webView canGoBack];
    });
    if (!ok) {
        throw_jni_exception(env, cannot_go_back ? "java/lang/IllegalStateException" : "java/lang/RuntimeException",
                            cannot_go_back ? "webview cannot go back" : "webview is not available");
        return JNI_FALSE;
    }
    return can_go_back_after ? JNI_TRUE : JNI_FALSE;
}

API_EXPORT(jboolean, goForward, jlong handle) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return JNI_FALSE;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    if (!ctx) return JNI_FALSE;

    __block BOOL can_go_forward_after = NO;
    __block bool ok = true;
    __block bool cannot_go_forward = false;
    runOnMainSync(^{
        if (!ctx || !ctx->webView) {
            ok = false;
            return;
        }
        if (![ctx->webView canGoForward]) {
            ok = false;
            cannot_go_forward = true;
            return;
        }

        [ctx->webView goForward];
        can_go_forward_after = [ctx->webView canGoForward];
    });
    if (!ok) {
        throw_jni_exception(env, cannot_go_forward ? "java/lang/IllegalStateException" : "java/lang/RuntimeException",
                            cannot_go_forward ? "webview cannot go forward" : "webview is not available");
        return JNI_FALSE;
    }
    return can_go_forward_after ? JNI_TRUE : JNI_FALSE;
}

API_EXPORT(void, setPageLoadingStartListener, jlong handle, jobject listener) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    if (!ctx) return;
    [ctx->pageLoadingObserver updateStartListener:env listener:listener];
}

API_EXPORT(void, setPageLoadingProgressListener, jlong handle, jobject listener) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    if (!ctx) return;
    [ctx->pageLoadingObserver updateProgressListener:env listener:listener];
}

API_EXPORT(void, setPageLoadingEndListener, jlong handle, jobject listener) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    if (!ctx) return;
    [ctx->pageLoadingObserver updateEndListener:env listener:listener];
}

API_EXPORT(void, setURLChangeListener, jlong handle, jobject handler) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    if (!ctx) return;
    [ctx->urlChangeObserver updateListener:env listener:handler];
}

API_EXPORT(void, setCanGoBackChangeListener, jlong handle, jobject handler) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    if (!ctx) return;

    [ctx->historyChangeObserver updateCanGoBackListener:env listener:handler];
    runOnMainSync(^{
        [ctx->historyChangeObserver emitCurrentState:ctx->webView];
    });
}

API_EXPORT(void, setCanGoForwardChangeListener, jlong handle, jobject handler) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    if (!ctx) return;

    [ctx->historyChangeObserver updateCanGoForwardListener:env listener:handler];
    runOnMainSync(^{
        [ctx->historyChangeObserver emitCurrentState:ctx->webView];
    });
}
