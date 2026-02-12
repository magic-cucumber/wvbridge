#include "progress.h"

#include <glib.h>

#include <algorithm>

namespace wvbridge {

struct ProgressState {
    JavaVM* jvm = nullptr;

    // Kotlin Function1<Float, Unit> or java.util.function.Consumer<Float>
    jobject listener_global = nullptr;

    // Consumer.accept(Object) or Function1.invoke(Object)
    jmethodID mid_call = nullptr;

    gulong notify_handler_id = 0;

    const std::atomic_bool* closing = nullptr; // borrowed

    bool use_accept = true; // true => accept(Object), false => invoke(Object)
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

static bool should_skip_due_to_closing(const ProgressState* st) {
    if (!st) return true;
    if (!st->closing) return false;
    return st->closing->load(std::memory_order_acquire);
}

static float clamp01(double v) {
    // WebKit 的 estimated-load-progress 理论上就是 0..1，但这里做防御，且转成 float
    if (v != v) return 0.0f; // NaN
    if (v < 0.0) return 0.0f;
    if (v > 1.0) return 1.0f;
    return static_cast<float>(v);
}

static void call_listener(ProgressState* st, float progress) {
    if (!st || !st->listener_global || !st->mid_call || !st->jvm) return;

    bool did_attach = false;
    JNIEnv* env = get_env(st->jvm, &did_attach);
    if (!env) {
        detach_if_needed(st->jvm, did_attach);
        return;
    }

    // Float.valueOf(float)
    jclass clsFloat = env->FindClass("java/lang/Float");
    if (!clsFloat) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        detach_if_needed(st->jvm, did_attach);
        return;
    }

    jmethodID midValueOf = env->GetStaticMethodID(clsFloat, "valueOf", "(F)Ljava/lang/Float;");
    if (!midValueOf) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(clsFloat);
        detach_if_needed(st->jvm, did_attach);
        return;
    }

    jobject jprogress = env->CallStaticObjectMethod(clsFloat, midValueOf, progress);
    env->DeleteLocalRef(clsFloat);

    if (env->ExceptionCheck() || !jprogress) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        detach_if_needed(st->jvm, did_attach);
        return;
    }

    env->CallVoidMethod(st->listener_global, st->mid_call, jprogress);
    env->DeleteLocalRef(jprogress);

    if (env->ExceptionCheck()) {
        // 不让异常逃逸到 native 层导致后续 JNI 调用异常
        env->ExceptionClear();
    }

    detach_if_needed(st->jvm, did_attach);
}

static void notify_progress_cb(GObject* obj, GParamSpec* pspec, gpointer user_data) {
    (void)pspec;

    auto* st = static_cast<ProgressState*>(user_data);
    if (!st) return;
    if (should_skip_due_to_closing(st)) return;

    auto* webview = WEBKIT_WEB_VIEW(obj);
    double p = webkit_web_view_get_estimated_load_progress(webview);

    float pf = clamp01(p);
    call_listener(st, pf);
}

ProgressState* progress_state_new(JNIEnv* env) {
    if (!env) return nullptr;

    auto* st = new ProgressState();

    JavaVM* jvm = nullptr;
    if (env->GetJavaVM(&jvm) != JNI_OK) {
        delete st;
        return nullptr;
    }

    st->jvm = jvm;
    return st;
}

void progress_state_destroy(ProgressState* state) {
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

void progress_set_listener(JNIEnv* env, ProgressState* state, jobject listener) {
    if (!env || !state) return;

    // 释放旧 listener
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

    // 优先 accept(Object)
    jmethodID mid = env->GetMethodID(cls, "accept", "(Ljava/lang/Object;)V");
    if (mid) {
        state->mid_call = mid;
        state->use_accept = true;
        env->DeleteLocalRef(cls);
        return;
    }

    // 清掉 accept 查找失败的异常（如果有）
    if (env->ExceptionCheck()) env->ExceptionClear();

    // 再尝试 Kotlin Function1.invoke(Object)
    mid = env->GetMethodID(cls, "invoke", "(Ljava/lang/Object;)Ljava/lang/Object;");
    if (mid) {
        state->mid_call = mid;
        state->use_accept = false;
        env->DeleteLocalRef(cls);
        return;
    }

    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(cls);

    env->DeleteGlobalRef(state->listener_global);
    state->listener_global = nullptr;
    state->mid_call = nullptr;
}

void progress_install(WebKitWebView* webview, ProgressState* state, const std::atomic_bool* closing_flag) {
    if (!webview || !state) return;

    state->closing = closing_flag;

    if (state->notify_handler_id != 0) {
        g_signal_handler_disconnect(webview, state->notify_handler_id);
        state->notify_handler_id = 0;
    }

    // 监听属性变化：notify::estimated-load-progress
    state->notify_handler_id = g_signal_connect(webview, "notify::estimated-load-progress", G_CALLBACK(notify_progress_cb), state);
}

void progress_uninstall(WebKitWebView* webview, ProgressState* state) {
    if (!webview || !state) return;

    if (state->notify_handler_id != 0) {
        g_signal_handler_disconnect(webview, state->notify_handler_id);
        state->notify_handler_id = 0;
    }
}

} // namespace wvbridge
