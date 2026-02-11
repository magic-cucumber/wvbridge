#import "navigation.h"

@implementation NavigationGateway {
    JavaVM *_jvm;
    jobject _listenerGlobalRef;
    jmethodID _acceptMethod;
}

- (instancetype)initWithJVM:(JavaVM *)jvm
                   listener:(jobject)listener
                   methodID:(jmethodID)methodID {
    self = [super init];
    if (self) {
        _jvm = jvm;
        _listenerGlobalRef = listener;
        _acceptMethod = methodID;
    }
    return self;
}

- (void)                webView:(WKWebView *)webView
 decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction
                 decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler {
    (void) webView;

    if (decisionHandler == nil) return;
    if (_jvm == nullptr) {
        decisionHandler(WKNavigationActionPolicyAllow);
        return;
    }

    NSURL *url = navigationAction.request.URL;
    if (url == nil) {
        decisionHandler(WKNavigationActionPolicyAllow);
        return;
    }

    NSString *urlStr = url.absoluteString;
    if (urlStr == nil) {
        decisionHandler(WKNavigationActionPolicyAllow);
        return;
    }

    JNIEnv *env = nullptr;
    bool needsDetach = false;

    auto finish = [&](WKNavigationActionPolicy policy) {
        if (needsDetach) {
            _jvm->DetachCurrentThread();
        }
        decisionHandler(policy);
    };

    jint getEnvStat = _jvm->GetEnv((void **) &env, JNI_VERSION_1_6);
    if (getEnvStat == JNI_EDETACHED) {
        if (_jvm->AttachCurrentThread((void **) &env, nullptr) != 0) {
            decisionHandler(WKNavigationActionPolicyAllow);
            return;
        }
        needsDetach = true;
    }

    if (env == nullptr) {
        finish(WKNavigationActionPolicyAllow);
        return;
    }

    if (_listenerGlobalRef == nullptr || _acceptMethod == nullptr) {
        finish(WKNavigationActionPolicyAllow);
        return;
    }

    const char *cUrl = [urlStr UTF8String];
    jstring jUrl = env->NewStringUTF(cUrl ? cUrl : "");
    if (jUrl == nullptr) {
        finish(WKNavigationActionPolicyAllow);
        return;
    }

    jobject resultObj = env->CallObjectMethod(_listenerGlobalRef, _acceptMethod, jUrl);
    env->DeleteLocalRef(jUrl);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        finish(WKNavigationActionPolicyAllow);
        return;
    }

    if (resultObj == nullptr) {
        finish(WKNavigationActionPolicyAllow);
        return;
    }

    jclass booleanClass = env->FindClass("java/lang/Boolean");
    if (booleanClass == nullptr) {
        env->DeleteLocalRef(resultObj);
        finish(WKNavigationActionPolicyAllow);
        return;
    }

    jmethodID booleanValueMID = env->GetMethodID(booleanClass, "booleanValue", "()Z");
    if (booleanValueMID == nullptr) {
        env->DeleteLocalRef(booleanClass);
        env->DeleteLocalRef(resultObj);
        finish(WKNavigationActionPolicyAllow);
        return;
    }

    jboolean jAllow = env->CallBooleanMethod(resultObj, booleanValueMID);

    env->DeleteLocalRef(booleanClass);
    env->DeleteLocalRef(resultObj);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        finish(WKNavigationActionPolicyAllow);
        return;
    }

    finish((jAllow == JNI_TRUE) ? WKNavigationActionPolicyAllow : WKNavigationActionPolicyCancel);
}

- (void)dealloc {
    JNIEnv *env = nullptr;
    jint getEnvStat = _jvm->GetEnv((void **) &env, JNI_VERSION_1_6);

    bool needsDetach = false;
    if (getEnvStat == JNI_EDETACHED) {
        if (_jvm->AttachCurrentThread((void **) &env, nullptr) == 0) {
            needsDetach = true;
        }
    }

    if (env != nullptr && _listenerGlobalRef != nullptr) {
        env->DeleteGlobalRef(_listenerGlobalRef);
        _listenerGlobalRef = nullptr;
    }

    if (needsDetach) {
        _jvm->DetachCurrentThread();
    }

    [super dealloc];
}

@end