#include "wvbridge/java_runtime.h"
#include "wvbridge/logger.h"

static JavaVM* g_java_vm = NULL;

void listener_support_on_load(JNIEnv* env);
void javascript_on_load(JNIEnv* env);

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void) reserved;
    g_java_vm = vm;

    JNIEnv* env = NULL;
    if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    listener_support_on_load(env);
    javascript_on_load(env);
    logger_on_load();

    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    (void) vm;
    (void) reserved;
    logger_on_unload();
    g_java_vm = NULL;
}

JavaVM* java_runtime_get_vm(void) {
    return g_java_vm;
}

JNIEnv* java_runtime_get_env(int* attached) {
    if (attached != NULL) {
        *attached = 0;
    }
    if (g_java_vm == NULL) {
        return NULL;
    }

    JNIEnv* env = NULL;
    const jint status = (*g_java_vm)->GetEnv(g_java_vm, (void**) &env, JNI_VERSION_1_6);
    if (status == JNI_OK) {
        return env;
    }
    if (status != JNI_EDETACHED) {
        return NULL;
    }

    if ((*g_java_vm)->AttachCurrentThread(g_java_vm, (void**) &env, NULL) != JNI_OK) {
        return NULL;
    }
    if (attached != NULL) {
        *attached = 1;
    }
    return env;
}

void java_runtime_detach_env(int attached) {
    if (attached == 0 || g_java_vm == NULL) {
        return;
    }
    (*g_java_vm)->DetachCurrentThread(g_java_vm);
}
