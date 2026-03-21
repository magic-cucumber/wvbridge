#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>
#import <jni.h>

@interface URLChangeObserver : NSObject

- (instancetype)initWithJVM:(JavaVM *)jvm
                   listener:(jobject)listener
                   methodID:(jmethodID)methodID
                  useAccept:(BOOL)useAccept;

@end
