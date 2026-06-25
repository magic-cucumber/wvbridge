#import "utils.h"

#import <Foundation/Foundation.h>

#include <dispatch/dispatch.h>
#include <wvbridge/logger.h>

void runOnMainSync(void (^block)(void)) {
    LOGGER_I("runOnMainSync");
    if ([NSThread isMainThread]) {
        LOGGER_V("runOnMainSync: already on main thread, executing directly");
        block();
    } else {
        LOGGER_V("runOnMainSync: dispatching sync to main queue");
        dispatch_sync(dispatch_get_main_queue(), block);
    }
}

void runOnMainAsync(void (^block)(void)) {
    LOGGER_I("runOnMainAsync");
    if ([NSThread isMainThread]) {
        LOGGER_V("runOnMainAsync: already on main thread, executing directly");
        block();
    } else {
        LOGGER_V("runOnMainAsync: dispatching async to main queue");
        dispatch_async(dispatch_get_main_queue(), block);
    }
}

void println0(JNIEnv *env, const char *message) {
    LOGGER_I("println0: message=%s", message ? message : "(null)");
    if (env == nullptr) {
        LOGGER_W("println0: env is null, aborting");
        return;
    }

    jclass sysClass = env->FindClass("java/lang/System");
    if (sysClass == nullptr) {
        LOGGER_W("println0: sysClass is null, aborting");
        return;
    }

    jfieldID outID = env->GetStaticFieldID(sysClass, "out", "Ljava/io/PrintStream;");
    if (outID == nullptr) {
        LOGGER_W("println0: outID is null, aborting");
        return;
    }

    jobject outObj = env->GetStaticObjectField(sysClass, outID);
    if (outObj == nullptr) {
        LOGGER_W("println0: outObj is null, aborting");
        return;
    }

    jclass psClass = env->FindClass("java/io/PrintStream");
    if (psClass == nullptr) {
        LOGGER_W("println0: psClass is null, aborting");
        return;
    }

    jmethodID printlnID = env->GetMethodID(psClass, "println", "(Ljava/lang/String;)V");
    if (printlnID == nullptr) {
        LOGGER_W("println0: printlnID is null, aborting");
        return;
    }

    LOGGER_V("println0: calling println");
    jstring jmsg = env->NewStringUTF(message);
    env->CallVoidMethod(outObj, printlnID, jmsg);

    env->DeleteLocalRef(jmsg);
    env->DeleteLocalRef(sysClass);
    if (psClass != nullptr) env->DeleteLocalRef(psClass);
    if (outObj != nullptr) env->DeleteLocalRef(outObj);
}

void throw_jni_exception(JNIEnv *env, const char *name, const char *message) {
    LOGGER_I("throw_jni_exception: name=%s message=%s", name ? name : "(null)", message ? message : "(null)");
    if (env == nullptr) {
        LOGGER_W("throw_jni_exception: env is null, aborting");
        return;
    }

    // 若已存在 pending exception，则不要覆盖。
    if (env->ExceptionCheck()) {
        LOGGER_W("throw_jni_exception: pending exception exists, not overwriting");
        return;
    }

    const char *fallback = "java/lang/RuntimeException";

    std::string className = (name != nullptr && name[0] != '\0') ? name : fallback;
    for (auto &ch : className) {
        if (ch == '.') ch = '/';
    }
    LOGGER_V("throw_jni_exception: resolved class=%s", className.c_str());

    jclass excClass = env->FindClass(className.c_str());
    if (excClass == nullptr) {
        // FindClass 失败会留下 ClassNotFoundException；这里清掉并回退到 RuntimeException。
        LOGGER_V("throw_jni_exception: original class not found, falling back to RuntimeException");
        if (env->ExceptionCheck()) env->ExceptionClear();
        excClass = env->FindClass(fallback);
        if (excClass == nullptr) {
            // 连 RuntimeException 都找不到：避免留下非预期异常。
            LOGGER_E("throw_jni_exception: RuntimeException not found, aborting");
            if (env->ExceptionCheck()) env->ExceptionClear();
            return;
        }
    }

    const char *msg = (message != nullptr) ? message : "";
    LOGGER_V("throw_jni_exception: throwing with msg=%s", msg);
    env->ThrowNew(excClass, msg);
    env->DeleteLocalRef(excClass);
}
