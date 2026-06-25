#include "libs_helpers.h"
#include <wvbridge/logger.h>

NSView *find_view_for_layer(CALayer *layer) {
    LOGGER_I("find_view_for_layer");
    for (CALayer *cursor = layer; cursor != nil; cursor = cursor.superlayer) {
        id delegate = cursor.delegate;
        LOGGER_V("find_view_for_layer: checking delegate %p", (__bridge void *)delegate);
        if ([delegate isKindOfClass:[NSView class]]) {
            LOGGER_V("find_view_for_layer: found NSView delegate at %p", (__bridge void *)delegate);
            return (NSView *) delegate;
        }
    }
    LOGGER_V("find_view_for_layer: no NSView found in layer hierarchy");
    return nil;
}

