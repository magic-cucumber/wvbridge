#include "url_listener.h"

#include <glib.h>

#include <string>

namespace wvbridge {

struct URLListenerState {
    JavaVM* jvm = nullptr;

    // Kotlin Function1<String, Unit> or java.util.function.Consumer<String>
    jobject listener_global = nullptr;
    jmethodID mid_call = nullptr;
    bool use_accept = true; // true => accept(Object), false => invoke(Object)

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

static bool should_skip_due_to_closing(const URLListenerState* st) {
    if (!st) return false;
    if (!st->closing) return false;
    return st->closing->load(std::memory_order_acquire);
}

static void notify_listener(URLListenerState* st, const char* uri) {
    if (!st) return;
    if (!st->listener_global || !st->mid_call || !st->jvm) return;

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

    if (st->use_accept) {
        env->CallVoidMethod(st->listener_global, st->mid_call, juri);
    } else {
        jobject ret = env->CallObjectMethod(st->listener_global, st->mid_call, juri);
        if (ret) env->DeleteLocalRef(ret);
    }
    env->DeleteLocalRef(juri);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    detach_if_needed(st->jvm, did_attach);
}

static void notify_uri_cb(WebKitWebView* webview, GParamSpec* pspec, gpointer user_data) {
    auto* st = static_cast<URLListenerState*>(user_data);
    if (!st || !webview) return;
    if (pspec == nullptr) return;
    if (should_skip_due_to_closing(st)) return;

    const char* uri = webkit_web_view_get_uri(webview);
    notify_listener(st, uri ? uri : "");
}

URLListenerState* url_listener_state_new(JNIEnv* env) {
    if (!env) return nullptr;

    auto* st = new URLListenerState();

    JavaVM* jvm = nullptr;
    if (env->GetJavaVM(&jvm) != JNI_OK) {
        delete st;
        return nullptr;
    }

    st->jvm = jvm;
    return st;
}

void url_listener_state_destroy(URLListenerState* state) {
    if (!state) return;

    if (state->listener_global && state->jvm) {
        bool did_attach = false;
        JNIEnv* env = get_env(state->jvm, &did_attach);
        if (env) {
            env->DeleteGlobalRef(state->listener_global);
        }
        detach_if_needed(state->jvm, did_attach);
        state->listener_global = nullptr;
    }

    delete state;
}

void url_listener_set_listener(JNIEnv* env, URLListenerState* state, jobject listener) {
    if (!state || !env) return;

    if (state->listener_global) {
        env->DeleteGlobalRef(state->listener_global);
        state->listener_global = nullptr;
        state->mid_call = nullptr;
    }

    if (listener == nullptr) {
        return;
    }

    state->listener_global = env->NewGlobalRef(listener);
    if (!state->listener_global) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return;
    }

    jclass cls = env->GetObjectClass(listener);
    if (!cls) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteGlobalRef(state->listener_global);
        state->listener_global = nullptr;
        return;
    }

    jmethodID mid = env->GetMethodID(cls, "accept", "(Ljava/lang/Object;)V");
    if (mid) {
        state->mid_call = mid;
        state->use_accept = true;
        env->DeleteLocalRef(cls);
        return;
    }

    if (env->ExceptionCheck()) env->ExceptionClear();

    mid = env->GetMethodID(cls, "invoke", "(Ljava/lang/Object;)Ljava/lang/Object;");
    env->DeleteLocalRef(cls);

    if (mid) {
        state->mid_call = mid;
        state->use_accept = false;
        return;
    }

    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteGlobalRef(state->listener_global);
    state->listener_global = nullptr;
    state->mid_call = nullptr;
}

void url_listener_install(WebKitWebView* webview, URLListenerState* state, const std::atomic_bool* closing_flag) {
    if (!webview || !state) return;

    state->closing = closing_flag;

    if (state->notify_uri_handler_id != 0) {
        g_signal_handler_disconnect(webview, state->notify_uri_handler_id);
        state->notify_uri_handler_id = 0;
    }

    state->notify_uri_handler_id = g_signal_connect(webview, "notify::uri", G_CALLBACK(notify_uri_cb), state);
}

void url_listener_uninstall(WebKitWebView* webview, URLListenerState* state) {
    if (!webview || !state) return;

    if (state->notify_uri_handler_id != 0) {
        g_signal_handler_disconnect(webview, state->notify_uri_handler_id);
        state->notify_uri_handler_id = 0;
    }
}

} // namespace wvbridge
