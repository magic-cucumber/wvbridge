#import "utils.h"

#import <Foundation/Foundation.h>

#include <dispatch/dispatch.h>

void runOnMainSync(void (^block)(void)) {
    if ([NSThread isMainThread]) {
        block();
    } else {
        dispatch_sync(dispatch_get_main_queue(), block);
    }
}

void runOnMainAsync(void (^block)(void)) {
    if ([NSThread isMainThread]) {
        block();
    } else {
        dispatch_async(dispatch_get_main_queue(), block);
    }
}

void println0(JNIEnv *env, const char *message) {
    if (env == nullptr) return;

    jclass sysClass = env->FindClass("java/lang/System");
    if (sysClass == nullptr) return;

    jfieldID outID = env->GetStaticFieldID(sysClass, "out", "Ljava/io/PrintStream;");
    if (outID == nullptr) return;

    jobject outObj = env->GetStaticObjectField(sysClass, outID);
    if (outObj == nullptr) return;

    jclass psClass = env->FindClass("java/io/PrintStream");
    if (psClass == nullptr) return;

    jmethodID printlnID = env->GetMethodID(psClass, "println", "(Ljava/lang/String;)V");
    if (printlnID == nullptr) return;

    jstring jmsg = env->NewStringUTF(message);
    env->CallVoidMethod(outObj, printlnID, jmsg);

    env->DeleteLocalRef(jmsg);
    env->DeleteLocalRef(sysClass);
    if (psClass != nullptr) env->DeleteLocalRef(psClass);
    if (outObj != nullptr) env->DeleteLocalRef(outObj);
}

void throw_jni_exception(JNIEnv *env, const char *name, const char *message) {
    if (env == nullptr) return;

    // 若已存在 pending exception，则不要覆盖。
    if (env->ExceptionCheck()) return;

    const char *fallback = "java/lang/RuntimeException";

    std::string className = (name != nullptr && name[0] != '\0') ? name : fallback;
    for (auto &ch : className) {
        if (ch == '.') ch = '/';
    }

    jclass excClass = env->FindClass(className.c_str());
    if (excClass == nullptr) {
        // FindClass 失败会留下 ClassNotFoundException；这里清掉并回退到 RuntimeException。
        if (env->ExceptionCheck()) env->ExceptionClear();
        excClass = env->FindClass(fallback);
        if (excClass == nullptr) {
            // 连 RuntimeException 都找不到：避免留下非预期异常。
            if (env->ExceptionCheck()) env->ExceptionClear();
            return;
        }
    }

    const char *msg = (message != nullptr) ? message : "";
    env->ThrowNew(excClass, msg);
    env->DeleteLocalRef(excClass);
}
