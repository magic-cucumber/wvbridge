#include "navigation.h"

#include <glib.h>

#include <string>

namespace wvbridge {

struct NavigationState {
    JavaVM* jvm = nullptr;

    // Kotlin Function1<String, Boolean> / Java Function<String, Boolean>
    // 这里不强依赖具体接口类型，只用反射调用 apply(Object) -> Object。
    jobject handler_global = nullptr;
    jmethodID mid_apply = nullptr; // Object apply(Object)

    gulong notify_uri_handler_id = 0;

    const std::atomic_bool* closing = nullptr; // borrowed
};

static JNIEnv* get_env(JavaVM* jvm, bool* did_attach) {
    if (did_attach) *did_attach = false;
    if (!jvm) return nullptr;

    JNIEnv* env = nullptr;
    jint r = jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (r == JNI_OK) return env;

    if (r == JNI_EDETACHED) {
        if (jvm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr) != JNI_OK) {
            return nullptr;
        }
        if (did_attach) *did_attach = true;
        return env;
    }

    return nullptr;
}

static void detach_if_needed(JavaVM* jvm, bool did_attach) {
    if (!jvm || !did_attach) return;
    jvm->DetachCurrentThread();
}

static bool should_block_due_to_closing(const NavigationState* st) {
    if (!st) return false;
    if (!st->closing) return false;
    return st->closing->load(std::memory_order_acquire);
}

// 调用 JVM handler(url)，忽略返回值。
static void notify_handler(NavigationState* st, const char* uri) {
    if (!st) return;
    if (!st->handler_global || !st->mid_apply || !st->jvm) return;

    bool did_attach = false;
    JNIEnv* env = get_env(st->jvm, &did_attach);
    if (!env) {
        detach_if_needed(st->jvm, did_attach);
        return;
    }

    jstring juri = env->NewStringUTF(uri ? uri : "");
    if (!juri) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        detach_if_needed(st->jvm, did_attach);
        return;
    }

    jobject ret = env->CallObjectMethod(st->handler_global, st->mid_apply, juri);
    env->DeleteLocalRef(juri);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        detach_if_needed(st->jvm, did_attach);
        return;
    }

    if (ret) env->DeleteLocalRef(ret);

    detach_if_needed(st->jvm, did_attach);
}

static void notify_uri_cb(WebKitWebView* webview, GParamSpec* pspec, gpointer user_data) {
    auto* st = static_cast<NavigationState*>(user_data);
    if (!st || !webview) return;
    if (pspec == nullptr) return;
    if (should_block_due_to_closing(st)) return;

    const char* uri = webkit_web_view_get_uri(webview);
    notify_handler(st, uri ? uri : "");
}

NavigationState* navigation_state_new(JNIEnv* env) {
    if (!env) return nullptr;

    auto* st = new NavigationState();

    JavaVM* jvm = nullptr;
    if (env->GetJavaVM(&jvm) != JNI_OK) {
        delete st;
        return nullptr;
    }

    st->jvm = jvm;
    return st;
}

void navigation_state_destroy(NavigationState* state) {
    if (!state) return;

    if (state->handler_global && state->jvm) {
        bool did_attach = false;
        JNIEnv* env = get_env(state->jvm, &did_attach);
        if (env) {
            env->DeleteGlobalRef(state->handler_global);
        }
        detach_if_needed(state->jvm, did_attach);
        state->handler_global = nullptr;
    }

    delete state;
}

void navigation_set_handler(JNIEnv* env, NavigationState* state, jobject handler) {
    if (!state) return;
    if (!env) return;

    // 释放旧 handler
    if (state->handler_global) {
        env->DeleteGlobalRef(state->handler_global);
        state->handler_global = nullptr;
        state->mid_apply = nullptr;
    }

    if (handler == nullptr) {
        return;
    }

    // 建立 GlobalRef
    state->handler_global = env->NewGlobalRef(handler);
    if (!state->handler_global) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return;
    }

    // 解析 apply(Object) -> Object
    jclass cls = env->GetObjectClass(handler);
    if (!cls) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteGlobalRef(state->handler_global);
        state->handler_global = nullptr;
        return;
    }

    // Kotlin Function1 与 java.util.function.Function 都有 apply(Object)
    jmethodID mid = env->GetMethodID(cls, "apply", "(Ljava/lang/Object;)Ljava/lang/Object;");
    env->DeleteLocalRef(cls);

    if (!mid) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteGlobalRef(state->handler_global);
        state->handler_global = nullptr;
        return;
    }

    state->mid_apply = mid;
}

void navigation_install(WebKitWebView* webview, NavigationState* state, const std::atomic_bool* closing_flag) {
    if (!webview || !state) return;

    state->closing = closing_flag;

    if (state->notify_uri_handler_id != 0) {
        g_signal_handler_disconnect(webview, state->notify_uri_handler_id);
        state->notify_uri_handler_id = 0;
    }

    state->notify_uri_handler_id = g_signal_connect(webview, "notify::uri", G_CALLBACK(notify_uri_cb), state);
}

void navigation_uninstall(WebKitWebView* webview, NavigationState* state) {
    if (!webview || !state) return;

    if (state->notify_uri_handler_id != 0) {
        g_signal_handler_disconnect(webview, state->notify_uri_handler_id);
        state->notify_uri_handler_id = 0;
    }
}

} // namespace wvbridge
