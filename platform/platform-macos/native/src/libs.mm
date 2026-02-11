#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import <WebKit/WebKit.h>

#import <jni.h>
#import <jawt.h>
#import <jawt_md.h>

#include <algorithm>
#include <cstdint>

#import "utils.h"
#import "progress.h"
#import "navigation.h"
#import "ui_delegate.h"

struct WebViewContext {
    WKWebView *webView = nil;
    NSWindow *hostWindow = nil;
    NSView *hostView = nil;
    CALayer *rootLayer = nil;
    ProgressObserver *progressObserver = nil;
    NavigationGateway *navigationGateway = nil;
};

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
    WebViewContext *ctx = new WebViewContext();

    runOnMainSync(^{
        ctx->rootLayer = [CALayer layer];

        WKWebViewConfiguration *config = [[WKWebViewConfiguration alloc] init];
        config.defaultWebpagePreferences.allowsContentJavaScript = YES;
        config.preferences.javaScriptCanOpenWindowsAutomatically = YES;

        WKWebView *wv = [[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 0, 0)
                                           configuration:config];
        wv.customUserAgent = @"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/15.0 Safari/605.1.15";
        wv.hidden = NO;
        wv.UIDelegate = [[AllowAllUIDelegate alloc] init];

        ctx->webView = wv;


        if (!ctx || !ctx->webView) return;

        if (ctx->hostView && ctx->webView.superview == ctx->hostView) return;

        NSWindow *win = [NSApp keyWindow];
        if (!win || !win.contentView) return;

        ctx->hostWindow = win;
        ctx->hostView = win.contentView;

        if (ctx->webView.superview != ctx->hostView) {
            [ctx->webView removeFromSuperview];
            [ctx->hostView addSubview:ctx->webView positioned:NSWindowAbove relativeTo:nil];
        }

        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        surfaceLayers.layer = ctx->rootLayer;
        [CATransaction commit];
        [CATransaction flush];
    });

    ds->FreeDrawingSurfaceInfo(dsi);
    ds->Unlock(ds);
    awt.FreeDrawingSurface(ds);

    return (jlong) (uintptr_t) ctx;
}

API_EXPORT(void, update, jlong handle, jint w, jint h, jint x, jint y) {
    (void) env;
    (void) thiz;

    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    if (!ctx) return;

    runOnMainAsync(^{
        if (!ctx) return;
        if (w <= 0 || h <= 0) return;

        CGRect bounds = CGRectMake(0, 0, w, h);
        ctx->rootLayer.bounds = bounds;
        ctx->rootLayer.position = CGPointMake(bounds.size.width / 2.0, bounds.size.height / 2.0);

        if (!ctx->webView) return;
        if (!ctx->hostWindow || !ctx->hostView) return;


        CGFloat primaryH = NSScreen.mainScreen.frame.size.height;
        NSRect cocoaScreenRect = NSMakeRect((CGFloat) x,
                                            primaryH - (CGFloat) y - (CGFloat) h,
                                            (CGFloat) w,
                                            (CGFloat) h);;
        NSRect windowRect = [ctx->hostWindow convertRectFromScreen:cocoaScreenRect];
        NSRect target = [ctx->hostView convertRect:windowRect fromView:nil];

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
        surfaceLayers.layer = nil;
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
            ctx->webView.navigationDelegate = nil;
            [ctx->navigationGateway release];
            ctx->navigationGateway = nil;

            [ctx->webView.UIDelegate release];
            ctx->webView.UIDelegate = nil;

            [ctx->webView removeObserver:ctx->progressObserver forKeyPath:@"estimatedProgress"];
            [ctx->webView removeFromSuperview];
            ctx->webView = nil;
        }

        ctx->hostView = nil;
        ctx->hostWindow = nil;
        ctx->rootLayer = nil;

        [ctx->progressObserver release];
        ctx->progressObserver = nil;
    });

    delete ctx;

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

//private external fun setProgressListener(webview: Long, consumer: Consumer<Float>)
API_EXPORT(void, setProgressListener, jlong handle, jobject listener) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    if (!ctx) return;

    if (ctx->progressObserver != nil) {
        [ctx->webView removeObserver:ctx->progressObserver forKeyPath:@"estimatedProgress"];
        [ctx->progressObserver release];
        ctx->progressObserver = nil;
    }

    if (listener == nullptr) return;

    JavaVM *jvm = nil;
    if (env->GetJavaVM(&jvm) != JNI_OK) return;

    jclass listenerClass = env->GetObjectClass(listener);
    jmethodID acceptMID = env->GetMethodID(listenerClass,
                                           "accept",
                                           "(Ljava/lang/Object;)V");
    env->DeleteLocalRef(listenerClass);

    ProgressObserver *observer = [[ProgressObserver alloc] initWithJVM:jvm
                                                              listener:env->NewGlobalRef(listener)
                                                              methodID:acceptMID];

    [ctx->webView addObserver:observer
                   forKeyPath:@"estimatedProgress"
                      options:NSKeyValueObservingOptionNew
                      context:nil];

    ctx->progressObserver = observer;
}

//private external fun setNavigationHandler(webview: Long, handler: Function<String, Boolean>)
API_EXPORT(void, setNavigationHandler, jlong handle, jobject handler) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    if (!ctx) return;

    if (ctx->navigationGateway != nil) {
        ctx->webView.navigationDelegate = nil;
        [ctx->navigationGateway release];
        ctx->navigationGateway = nil;
    }

    if (handler == nullptr) return;

    JavaVM *jvm = nil;
    if (env->GetJavaVM(&jvm) != JNI_OK) return;

    jclass handlerClass = env->GetObjectClass(handler);
    jmethodID applyMID = env->GetMethodID(handlerClass,
                                          "apply",
                                          "(Ljava/lang/Object;)Ljava/lang/Object;");
    env->DeleteLocalRef(handlerClass);

    NavigationGateway *gateway = [[NavigationGateway alloc] initWithJVM:jvm
                                                               listener:env->NewGlobalRef(handler)
                                                               methodID:applyMID];
    ctx->webView.navigationDelegate = gateway;
    ctx->navigationGateway = gateway;
}