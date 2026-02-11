#import "progress.h"

@implementation ProgressObserver {
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

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey,id> *)change
                       context:(void *)context {

    if ([keyPath isEqualToString:@"estimatedProgress"]) {
        double progress = [[change objectForKey:NSKeyValueChangeNewKey] doubleValue];


        JNIEnv *env;
        bool needsDetach = false;

        jint getEnvStat = _jvm->GetEnv((void **)&env, JNI_VERSION_1_6);
        if (getEnvStat == JNI_EDETACHED) {
            if (_jvm->AttachCurrentThread((void **)&env, NULL) != 0) return;
            needsDetach = true;
        }

        jclass floatClass = env->FindClass("java/lang/Float");
        jmethodID floatConstructor = env->GetMethodID(floatClass, "<init>", "(F)V");
        jobject floatObj = env->NewObject(floatClass, floatConstructor, progress);

        env->CallVoidMethod(_listenerGlobalRef, _acceptMethod, floatObj);

        env->DeleteLocalRef(floatObj);
        env->DeleteLocalRef(floatClass);

        if (needsDetach) {
            _jvm->DetachCurrentThread();
        }
    }
}


- (void)dealloc {

    JNIEnv *env = nullptr;
    jint getEnvStat = _jvm->GetEnv((void **)&env, JNI_VERSION_1_6);

    bool needsDetach = false;
    if (getEnvStat == JNI_EDETACHED) {
        if (_jvm->AttachCurrentThread((void **)&env, nullptr) == 0) {
            needsDetach = true;
        }
    }

    if (env != nullptr) {
        env->DeleteGlobalRef(_listenerGlobalRef);
        _listenerGlobalRef = nullptr;
    }

    if (needsDetach) {
        _jvm->DetachCurrentThread();
    }

    [super dealloc];
}

@end