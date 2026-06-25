#include "libs_helpers.h"
#include <wvbridge/logger.h>

API_EXPORT(void, close0, jlong handle) {
    LOGGER_I("close0: handle=%lld", (long long) handle);
    if (handle == 0) {
        LOGGER_W("close0: handle is null, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    LOGGER_V("close0: ctx=%p", (void *) ctx);
    if (!ctx) {
        LOGGER_W("close0: ctx is null after cast, aborting");
        return;
    }

    // AWT 相关清理（解绑 surfaceLayers.layer）只有在"正确上下文"下才执行。
    // 这里的"正确上下文"= 组件可用（displayable）且能拿到 SurfaceLayers，并且其 layer 正是 ctx->rootLayer。
    LOGGER_V("close0: entering AWT surface cleanup do-while block");
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

        LOGGER_V("close0: unbinding rootLayer from surfaceLayers");
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
    LOGGER_V("close0: AWT surface cleanup done, starting UI cleanup on main thread");

    // UI 清理放在主线程。
    runOnMainSync(^{
        LOGGER_V("close0: executing UI cleanup on main thread, ctx=%p", (void *) ctx);
        if (!ctx) return;

        if (ctx->webView) {
            [ctx->events release];
            ctx->events = nil;

            [ctx->documentStartHooks release];
            ctx->documentStartHooks = nil;

            [ctx->webView.UIDelegate release];
            ctx->webView.UIDelegate = nil;
            [ctx->webView removeFromSuperview];
            ctx->webView = nil;
        }

        ctx->hostView = nil;
        ctx->hostWindow = nil;
        [ctx->windowLayer release];
        ctx->windowLayer = nil;
        ctx->rootLayer = nil;

    });

    LOGGER_V("close0: UI cleanup done, deleting ctx=%p", (void *) ctx);
    delete ctx;
}
