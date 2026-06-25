#include "libs_helpers.h"
#include <wvbridge/logger.h>

API_EXPORT(void, update, jlong handle, jint w, jint h, jint x, jint y) {
    LOGGER_I("update: handle=%lld w=%d h=%d x=%d y=%d", (long long) handle, (int) w, (int) h, (int) x, (int) y);
    if (handle == 0) {
        LOGGER_W("update: handle is null, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    LOGGER_V("update: ctx=%p", (void *) ctx);
    if (!ctx) {
        LOGGER_W("update: ctx is null after cast, aborting");
        return;
    }

    runOnMainAsync(^{
        LOGGER_V("update: executing on main thread, w=%d h=%d x=%d y=%d", (int) w, (int) h, (int) x, (int) y);
        if (!ctx) return;
        if (w <= 0 || h <= 0) return;

        CGRect layerFrame = CGRectMake((CGFloat) x, (CGFloat) y, (CGFloat) w, (CGFloat) h);
        if (ctx->windowLayer) {
            layerFrame.origin.y = ctx->windowLayer.bounds.size.height - layerFrame.origin.y - layerFrame.size.height;
            LOGGER_V("update: adjusted layerFrame origin.y for flipped coords");
        }
        ctx->rootLayer.frame = layerFrame;

        if (!ctx->webView) {
            LOGGER_V("update: webView is nil, skipping frame update");
            return;
        }

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
        LOGGER_V("update: webView frame set and layout triggered");
    });
}
