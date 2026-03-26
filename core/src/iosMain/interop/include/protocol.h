#pragma once

#import <Foundation/Foundation.h>

@protocol WVBKVOObserverProtocol <NSObject>

- (void)observeValueForKeyPath:(NSString * _Nullable)keyPath
                      ofObject:(id _Nullable)object
                        change:(NSDictionary * _Nullable)change
                       context:(void * _Nullable)context;

@end
