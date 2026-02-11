#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>
#import <jni.h>

@interface NavigationGateway : NSObject <WKNavigationDelegate>

- (instancetype)initWithJVM:(JavaVM *)jvm
                   listener:(jobject)listener
                   methodID:(jmethodID)methodID;

@end