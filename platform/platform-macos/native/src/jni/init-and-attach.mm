#include "libs_helpers.h"

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
    const jlong pointer = (jlong) (uintptr_t) ctx;

    runOnMainSync(^{
        ctx->rootLayer = [[CALayer alloc] init];
        ctx->windowLayer = [surfaceLayers.windowLayer retain];
        ctx->documentStartHooks = [[NSMutableDictionary alloc] init];

        WKWebViewConfiguration *config = [[WKWebViewConfiguration alloc] init];
        config.defaultWebpagePreferences.allowsContentJavaScript = YES;
        config.preferences.javaScriptCanOpenWindowsAutomatically = YES;

        WKWebView *wv = [[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 0, 0)
                                           configuration:config];
        wv.customUserAgent = @"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/15.0 Safari/605.1.15";
        wv.hidden = NO;
        wv.UIDelegate = [[AllowAllUIDelegate alloc] init];
        ctx->webView = wv;
        ctx->events = [[WebViewEvents alloc] initWithWebView:wv pointer:pointer];

        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        surfaceLayers.layer = ctx->rootLayer;
        [CATransaction commit];
        [CATransaction flush];


        NSView *hostView = find_view_for_layer(ctx->windowLayer);
        ctx->hostWindow = hostView.window;
        ctx->hostView = ctx->hostWindow.contentView ?: hostView;
        [ctx->webView removeFromSuperview];
        [ctx->hostView addSubview:ctx->webView positioned:NSWindowAbove relativeTo:nil];
    });

    ds->FreeDrawingSurfaceInfo(dsi);
    ds->Unlock(ds);
    awt.FreeDrawingSurface(ds);

    return (jlong) (uintptr_t) ctx;
}
