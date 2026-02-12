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

    gulong decide_policy_handler_id = 0;

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

// 调用 JVM handler(url) -> boolean
// - 若没有 handler，则返回 true（默认放行）
// - 若 JVM 调用失败/抛异常，则返回 false（安全起见拦截），并清理异常。
static bool call_handler_allow(NavigationState* st, const char* uri) {
    if (!st) return true;
    if (!st->handler_global || !st->mid_apply || !st->jvm) return true;

    bool did_attach = false;
    JNIEnv* env = get_env(st->jvm, &did_attach);
    if (!env) {
        detach_if_needed(st->jvm, did_attach);
        return false;
    }

    // Kotlin/Java 的 String 需要 UTF-8
    jstring juri = env->NewStringUTF(uri ? uri : "");
    if (!juri) {
        // OOM or exception
        if (env->ExceptionCheck()) env->ExceptionClear();
        detach_if_needed(st->jvm, did_attach);
        return false;
    }

    jobject ret = env->CallObjectMethod(st->handler_global, st->mid_apply, juri);
    env->DeleteLocalRef(juri);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        detach_if_needed(st->jvm, did_attach);
        return false;
    }

    bool allow = true;
    if (ret != nullptr) {
        jclass clsBoolean = env->FindClass("java/lang/Boolean");
        if (clsBoolean != nullptr && env->IsInstanceOf(ret, clsBoolean)) {
            jmethodID midBoolValue = env->GetMethodID(clsBoolean, "booleanValue", "()Z");
            if (midBoolValue) {
                allow = (env->CallBooleanMethod(ret, midBoolValue) == JNI_TRUE);
            } else {
                allow = false;
            }
            env->DeleteLocalRef(clsBoolean);
        } else {
            // 返回类型不对，当作拒绝
            if (clsBoolean) env->DeleteLocalRef(clsBoolean);
            allow = false;
        }
        env->DeleteLocalRef(ret);
    } else {
        // null 当作拒绝（更安全）
        allow = false;
    }

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        allow = false;
    }

    detach_if_needed(st->jvm, did_attach);
    return allow;
}

static gboolean decide_policy_cb(WebKitWebView* webview,
                                WebKitPolicyDecision* decision,
                                WebKitPolicyDecisionType type,
                                gpointer user_data) {
    (void)webview;

    auto* st = static_cast<NavigationState*>(user_data);
    if (!st) return G_SOURCE_CONTINUE;

    if (should_block_due_to_closing(st)) {
        webkit_policy_decision_ignore(decision);
        return TRUE;
    }

    // 仅拦截导航（点击链接、location 变更等）
    if (type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) {
        return FALSE;
    }

    auto* nav_decision = WEBKIT_NAVIGATION_POLICY_DECISION(decision);

    // 可按需仅拦截主 frame
    // if (!webkit_navigation_policy_decision_is_main_frame(nav_decision)) return FALSE;

    WebKitNavigationAction* action = webkit_navigation_policy_decision_get_navigation_action(nav_decision);
    if (!action) {
        return FALSE;
    }

    WebKitURIRequest* req = webkit_navigation_action_get_request(action);
    if (!req) {
        return FALSE;
    }

    const char* uri = webkit_uri_request_get_uri(req);
    if (!uri) uri = "";

    bool allow = call_handler_allow(st, uri);
    if (allow) {
        webkit_policy_decision_use(decision);
    } else {
        webkit_policy_decision_ignore(decision);
    }

    return TRUE;
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

    if (state->decide_policy_handler_id != 0) {
        // 已安装：先卸载避免重复连接
        g_signal_handler_disconnect(webview, state->decide_policy_handler_id);
        state->decide_policy_handler_id = 0;
    }

    state->decide_policy_handler_id = g_signal_connect(webview, "decide-policy", G_CALLBACK(decide_policy_cb), state);
}

void navigation_uninstall(WebKitWebView* webview, NavigationState* state) {
    if (!webview || !state) return;

    if (state->decide_policy_handler_id != 0) {
        g_signal_handler_disconnect(webview, state->decide_policy_handler_id);
        state->decide_policy_handler_id = 0;
    }
}

} // namespace wvbridge
