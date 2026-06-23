#include "libs_helpers.h"

NSView *find_view_for_layer(CALayer *layer) {
    for (CALayer *cursor = layer; cursor != nil; cursor = cursor.superlayer) {
        id delegate = cursor.delegate;
        if ([delegate isKindOfClass:[NSView class]]) {
            return (NSView *) delegate;
        }
    }
    return nil;
}

