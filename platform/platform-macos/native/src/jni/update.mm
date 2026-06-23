#include "libs_helpers.h"

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
