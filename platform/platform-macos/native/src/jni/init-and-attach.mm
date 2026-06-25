#include "libs_helpers.h"
#include <wvbridge/javascript.h>
#include <wvbridge/logger.h>

namespace {

NSString *string_from_script_message_body(id body) {
    LOGGER_V("string_from_script_message_body: entry");
    if (body == nil || body == [NSNull null]) {
        LOGGER_V("string_from_script_message_body: body is nil");
        return @"";
    }
    if ([body isKindOfClass:[NSString class]]) {
        LOGGER_V("string_from_script_message_body: body is NSString");
        return (NSString *) body;
    }
    if ([NSJSONSerialization isValidJSONObject:body]) {
        NSData *data = [NSJSONSerialization dataWithJSONObject:body options:0 error:nil];
        if (data) {
            NSString *json = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
            NSString *result = [json autorelease] ?: @"";
            LOGGER_V("string_from_script_message_body: json=\"%.*s\"", (int) MIN(100, [result length]), [result UTF8String]);
            return result;
        }
    }
    LOGGER_V("string_from_script_message_body: using description fallback");
    return [body description] ?: @"";
}

}

@interface WVBWebMessageHandler : NSObject <WKScriptMessageHandler>
@property(nonatomic, assign) WebViewContext *context;
@end

@implementation WVBWebMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message {
    LOGGER_V("WVBWebMessageHandler: didReceiveScriptMessage");
    (void) userContentController;
    if (!self.context) {
        LOGGER_W("WVBWebMessageHandler: context is nil, dropping message");
        return;
    }
    NSString *body = string_from_script_message_body(message.body);
    LOGGER_V("WVBWebMessageHandler: message body=\"%.*s\"", (int) MIN(100, [body length]), [body UTF8String]);
    wvbridge::dispatch_web_message_to_java(
        self.context->webMessageHandlersMutex,
        self.context->webMessageHandlers,
        [body UTF8String]
    );
    LOGGER_V("WVBWebMessageHandler: dispatched to Java");
}
@end

API_EXPORT(jlong, initAndAttach) {
    LOGGER_I("initAndAttach: entry");
    // JAWT surface layers
    JAWT awt;
    awt.version = JAWT_VERSION_1_4 | JAWT_MACOSX_USE_CALAYER;
    LOGGER_V("initAndAttach: requesting JAWT version=0x%x", (unsigned int) awt.version);
    if (JAWT_GetAWT(env, &awt) == JNI_FALSE) {
        LOGGER_E("initAndAttach: JAWT_GetAWT failed");
        return 0;
    }

    JAWT_DrawingSurface *ds = awt.GetDrawingSurface(env, thiz);
    if (!ds) {
        LOGGER_E("initAndAttach: GetDrawingSurface returned null");
        return 0;
    }
    LOGGER_V("initAndAttach: drawing surface obtained, ds=%p", (void *) ds);

    jint lock = ds->Lock(ds);
    if (lock & JAWT_LOCK_ERROR) {
        LOGGER_E("initAndAttach: JAWT lock error, lock=0x%x", (unsigned int) lock);
        awt.FreeDrawingSurface(ds);
        return 0;
    }
    LOGGER_V("initAndAttach: drawing surface locked successfully");

    JAWT_DrawingSurfaceInfo *dsi = ds->GetDrawingSurfaceInfo(ds);
    if (!dsi) {
        LOGGER_E("initAndAttach: GetDrawingSurfaceInfo returned null");
        ds->Unlock(ds);
        awt.FreeDrawingSurface(ds);
        return 0;
    }

    id <JAWT_SurfaceLayers> surfaceLayers = (__bridge id <JAWT_SurfaceLayers>) dsi->platformInfo;
    if (!surfaceLayers) {
        LOGGER_E("initAndAttach: surfaceLayers is null");
        ds->FreeDrawingSurfaceInfo(dsi);
        ds->Unlock(ds);
        awt.FreeDrawingSurface(ds);
        return 0;
    }
    LOGGER_V("initAndAttach: surfaceLayers obtained");

    WebViewContext *ctx = new WebViewContext();
    const jlong pointer = (jlong) (uintptr_t) ctx;
    LOGGER_V("initAndAttach: ctx=%p pointer=%lld", (void *) ctx, (long long) pointer);

    LOGGER_V("initAndAttach: setting up WebView on main thread");
    runOnMainSync(^{
        ctx->rootLayer = [[CALayer alloc] init];
        ctx->windowLayer = [surfaceLayers.windowLayer retain];
        ctx->documentStartHooks = [[NSMutableDictionary alloc] init];

        WKWebViewConfiguration *config = [[WKWebViewConfiguration alloc] init];
        config.defaultWebpagePreferences.allowsContentJavaScript = YES;
        config.preferences.javaScriptCanOpenWindowsAutomatically = YES;
        LOGGER_V("initAndAttach: setting up WKScriptMessageHandler");
        ctx->webMessageHandler = [[WVBWebMessageHandler alloc] init];
        ctx->webMessageHandler.context = ctx;
        [config.userContentController addScriptMessageHandler:ctx->webMessageHandler name:@"wvbridge"];
        LOGGER_V("initAndAttach: wvbridge script message handler registered");

        WKWebView *wv = [[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 0, 0)
                                           configuration:config];
        wv.customUserAgent = @"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/15.0 Safari/605.1.15";
        wv.hidden = NO;
        wv.UIDelegate = [[AllowAllUIDelegate alloc] init];
        ctx->webView = wv;
        ctx->events = [[WebViewEvents alloc] initWithWebView:wv pointer:pointer];
        LOGGER_V("initAndAttach: WebView created on main thread, wv=%p", (void *) wv);

        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        surfaceLayers.layer = ctx->rootLayer;
        [CATransaction commit];
        [CATransaction flush];
        LOGGER_V("initAndAttach: rootLayer bound to surfaceLayers");


        NSView *hostView = find_view_for_layer(ctx->windowLayer);
        ctx->hostWindow = hostView.window;
        ctx->hostView = ctx->hostWindow.contentView ?: hostView;
        [ctx->webView removeFromSuperview];
        [ctx->hostView addSubview:ctx->webView positioned:NSWindowAbove relativeTo:nil];
        LOGGER_V("initAndAttach: webView added to hostView subviews");
    });

    ds->FreeDrawingSurfaceInfo(dsi);
    ds->Unlock(ds);
    awt.FreeDrawingSurface(ds);
    LOGGER_V("initAndAttach: JAWT surface released, returning pointer=%lld", (long long) pointer);

    return (jlong) (uintptr_t) ctx;
}
