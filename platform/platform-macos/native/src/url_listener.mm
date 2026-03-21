#import "url_listener.h"

@implementation URLChangeObserver {
    JavaVM *_jvm;
    jobject _listenerGlobalRef;
    jmethodID _callMethod;
    BOOL _useAccept;
}

- (instancetype)initWithJVM:(JavaVM *)jvm
                   listener:(jobject)listener
                   methodID:(jmethodID)methodID
                  useAccept:(BOOL)useAccept {
    self = [super init];
    if (self) {
        _jvm = jvm;
        _listenerGlobalRef = listener;
        _callMethod = methodID;
        _useAccept = useAccept;
    }
    return self;
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id> *)change
                       context:(void *)context {
    (void) change;
    (void) context;

    if (![keyPath isEqualToString:@"URL"]) return;
    if (_jvm == nullptr || _listenerGlobalRef == nullptr || _callMethod == nullptr) return;

    WKWebView *webView = [object isKindOfClass:[WKWebView class]] ? (WKWebView *) object : nil;
    NSString *urlStr = webView.URL.absoluteString ?: @"";

    JNIEnv *env = nullptr;
    bool needsDetach = false;

    jint getEnvStat = _jvm->GetEnv((void **) &env, JNI_VERSION_1_6);
    if (getEnvStat == JNI_EDETACHED) {
        if (_jvm->AttachCurrentThread((void **) &env, nullptr) != 0) {
            return;
        }
        needsDetach = true;
    }

    if (env == nullptr) return;

    const char *cUrl = [urlStr UTF8String];
    jstring jUrl = env->NewStringUTF(cUrl ? cUrl : "");
    if (jUrl == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (needsDetach) _jvm->DetachCurrentThread();
        return;
    }

    if (_useAccept) {
        env->CallVoidMethod(_listenerGlobalRef, _callMethod, jUrl);
    } else {
        jobject resultObj = env->CallObjectMethod(_listenerGlobalRef, _callMethod, jUrl);
        if (resultObj != nullptr) {
            env->DeleteLocalRef(resultObj);
        }
    }
    env->DeleteLocalRef(jUrl);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    if (needsDetach) {
        _jvm->DetachCurrentThread();
    }
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
