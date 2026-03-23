#include <windows.h>
#include <Unknwn.h>

#include <jawt.h>
#include <jawt_md.h>

#include <atomic>
#include <filesystem>
#include <future>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "history_events.h"
#include "java_listener.h"
#include "page_loading_events.h"
#include "thread.h"
#include "url_events.h"
#include "utils.h"
#include "webview2_callback.h"
#include "webview_context.h"

using Microsoft::WRL::ComPtr;

namespace {

struct InitState {
    std::promise<HRESULT> promise;
    std::atomic_bool done{false};
    std::string error;
};

int clamp_dim(jint v) {
    return (v < 1) ? 1 : static_cast<int>(v);
}

std::wstring query_process_path() {
    std::vector<wchar_t> buffer(MAX_PATH);
    while (true) {
        DWORD len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (len == 0) return L"";
        if (len < buffer.size() - 1) {
            return std::wstring(buffer.data(), len);
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::wstring query_current_directory() {
    DWORD len = GetCurrentDirectoryW(0, nullptr);
    if (len == 0) return L"";

    std::vector<wchar_t> buffer(len);
    DWORD actual = GetCurrentDirectoryW(len, buffer.data());
    if (actual == 0 || actual >= len) return L"";
    return std::wstring(buffer.data(), actual);
}

std::wstring query_env_var(const wchar_t *name) {
    if (!name || name[0] == L'\0') return L"";

    DWORD len = GetEnvironmentVariableW(name, nullptr, 0);
    if (len == 0) return L"";

    std::vector<wchar_t> buffer(len);
    DWORD actual = GetEnvironmentVariableW(name, buffer.data(), len);
    if (actual == 0 || actual >= len) return L"";
    return std::wstring(buffer.data(), actual);
}

std::wstring query_temp_directory() {
    std::vector<wchar_t> buffer(MAX_PATH);
    while (true) {
        DWORD len = GetTempPathW(static_cast<DWORD>(buffer.size()), buffer.data());
        if (len == 0) return L"";
        if (len < buffer.size()) {
            return std::wstring(buffer.data(), len);
        }
        buffer.resize(len + 1);
    }
}

std::string wstring_to_utf8(const std::wstring &value) {
    if (value.empty()) return "";

    int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) return "";

    std::string out(static_cast<size_t>(required), '\0');
    int converted = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), required, nullptr, nullptr);
    if (converted <= 0) return "";
    return out;
}

std::string trim_message(std::wstring message) {
    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' ' || message.back() == L'\t')) {
        message.pop_back();
    }
    return wstring_to_utf8(message);
}

std::string format_hresult(HRESULT hr) {
    std::ostringstream oss;
    oss << "0x"
        << std::uppercase
        << std::hex
        << std::setw(8)
        << std::setfill('0')
        << static_cast<unsigned long>(static_cast<unsigned int>(hr));
    return oss.str();
}

std::string format_system_message(HRESULT hr) {
    LPWSTR raw = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD len = FormatMessageW(
        flags,
        nullptr,
        static_cast<DWORD>(hr),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&raw),
        0,
        nullptr
    );
    if (len == 0 || !raw) return "";

    std::wstring message(raw, len);
    LocalFree(raw);
    return trim_message(message);
}

uint64_t stable_hash(const std::wstring &value) {
    uint64_t hash = 1469598103934665603ull;
    for (wchar_t ch: value) {
        hash ^= static_cast<uint64_t>(ch);
        hash *= 1099511628211ull;
    }
    return hash;
}

std::wstring build_user_data_folder() {
    std::wstring root = query_env_var(L"LOCALAPPDATA");
    if (root.empty()) {
        root = query_temp_directory();
    }
    if (root.empty()) return L"";

    const std::wstring process_path = query_process_path();
    std::filesystem::path process_fs = process_path.empty() ? std::filesystem::path(L"java.exe") : std::filesystem::path(process_path);
    std::wstring process_name = process_fs.stem().wstring();
    if (process_name.empty()) {
        process_name = L"java";
    }

    std::wostringstream suffix;
    suffix << std::hex << std::uppercase << stable_hash(process_fs.wstring());

    return (std::filesystem::path(root) / L"wvbridge" / L"WebView2" / (process_name + L"-" + suffix.str())).wstring();
}

HRESULT ensure_directory_exists(const std::wstring &path, std::string *detail) {
    if (path.empty()) {
        if (detail) *detail = "No writable base directory was found for WebView2 userDataFolder";
        return E_FAIL;
    }

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path), ec);
    if (ec) {
        if (detail) {
            std::ostringstream oss;
            oss << "create_directories failed: " << ec.message() << " (" << ec.value() << ")";
            *detail = oss.str();
        }
        return HRESULT_FROM_WIN32(ec.value());
    }
    return S_OK;
}

std::string query_runtime_version(const std::wstring &browser_executable_folder) {
    LPWSTR version = nullptr;
    HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(
        browser_executable_folder.empty() ? nullptr : browser_executable_folder.c_str(),
        &version
    );

    if (SUCCEEDED(hr) && version) {
        std::string value = wstring_to_utf8(version);
        CoTaskMemFree(version);
        return value.empty() ? "<empty>" : value;
    }

    if (version) {
        CoTaskMemFree(version);
    }

    std::ostringstream oss;
    oss << "<unavailable>";
    if (FAILED(hr)) {
        oss << " (" << format_hresult(hr);
        const std::string message = format_system_message(hr);
        if (!message.empty()) {
            oss << ", " << message;
        }
        oss << ")";
    }
    return oss.str();
}

std::string hint_for_hresult(HRESULT hr) {
    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
        return "No compatible WebView2 Runtime was found. Install Evergreen WebView2 Runtime or provide browserExecutableFolder.";
    }
    if (hr == E_ACCESSDENIED) {
        return "WebView2 could not access its user data folder. Use a writable userDataFolder and preserve its ACLs.";
    }
    if (hr == E_FAIL) {
        return "WebView2 Runtime failed to start. Check runtime installation, antivirus policy, and userDataFolder permissions.";
    }
    return "";
}

std::string build_init_error(
    const char *stage,
    HRESULT hr,
    const std::wstring &browser_executable_folder,
    const std::wstring &user_data_folder,
    const std::string &extra = ""
) {
    const std::wstring process_path = query_process_path();
    const std::wstring default_udf = process_path.empty() ? L"" : (process_path + L".WebView2");

    std::ostringstream oss;
    oss << (stage ? stage : "WebView2 initialization failed")
        << " [HRESULT=" << format_hresult(hr);

    const std::string system_message = format_system_message(hr);
    if (!system_message.empty()) {
        oss << ", message=" << system_message;
    }
    oss << "]";

    oss << ", processPath=" << (process_path.empty() ? "<unknown>" : wstring_to_utf8(process_path))
        << ", workingDirectory=" << wstring_to_utf8(query_current_directory())
        << ", defaultUserDataFolder=" << (default_udf.empty() ? "<unknown>" : wstring_to_utf8(default_udf))
        << ", configuredUserDataFolder=" << (user_data_folder.empty() ? "<default>" : wstring_to_utf8(user_data_folder))
        << ", browserExecutableFolder=" << (browser_executable_folder.empty() ? "<default>" : wstring_to_utf8(browser_executable_folder))
        << ", availableRuntimeVersion=" << query_runtime_version(browser_executable_folder);

    const std::string hint = hint_for_hresult(hr);
    if (!hint.empty()) {
        oss << ", hint=" << hint;
    }
    if (!extra.empty()) {
        oss << ", detail=" << extra;
    }
    return oss.str();
}

void destroy_ctx(JNIEnv *env, WebViewContext *ctx) {
    if (!ctx) return;

    ctx->closing.store(true, std::memory_order_release);

    if (ctx->thread) {
        webview2_thread_run_sync(ctx->thread, [ctx] {
            unregister_url_events(ctx);
            unregister_page_loading_events(ctx);
            unregister_history_events(ctx);

            if (ctx->new_window_requested_registered) {
                ctx->webview->remove_NewWindowRequested(ctx->token_new_window_requested);
                ctx->new_window_requested_registered = false;
            }

            if (ctx->controller) {
                ctx->controller->Close();
                ctx->controller.Reset();
            }
            ctx->webview.Reset();
            ctx->env.Reset();

            if (ctx->child_hwnd && IsWindow(ctx->child_hwnd)) {
                DestroyWindow(ctx->child_hwnd);
                ctx->child_hwnd = nullptr;
            }
        });
    }

    clear_listener(env, ctx->url_listener);
    clear_listener(env, ctx->page_loading_start_listener);
    clear_listener(env, ctx->page_loading_progress_listener);
    clear_listener(env, ctx->page_loading_end_listener);
    clear_listener(env, ctx->can_go_back_change_listener);
    clear_listener(env, ctx->can_go_forward_change_listener);

    if (ctx->thread) {
        webview2_thread_destroy(ctx->thread);
        ctx->thread = nullptr;
    }

    delete ctx;
}

void complete_once(const std::shared_ptr<InitState> &state, HRESULT hr, std::string error) {
    bool expected = false;
    if (!state->done.compare_exchange_strong(expected, true)) return;
    state->error = std::move(error);
    state->promise.set_value(hr);
}

} // namespace

API_EXPORT(jlong, initAndAttach) {
    HWND parent_hwnd = nullptr;

    JAWT awt{};
    awt.version = JAWT_VERSION_1_4;
    if (JAWT_GetAWT(env, &awt) == JNI_FALSE) {
        throw_jni_exception(env, "java/lang/RuntimeException", "JAWT_GetAWT failed");
        return 0;
    }

    JAWT_DrawingSurface *ds = awt.GetDrawingSurface(env, thiz);
    if (!ds) {
        throw_jni_exception(env, "java/lang/RuntimeException", "GetDrawingSurface failed");
        return 0;
    }

    bool locked = false;
    JAWT_DrawingSurfaceInfo *dsi = nullptr;

    auto free_dsi = [&] {
        if (dsi) {
            ds->FreeDrawingSurfaceInfo(dsi);
            dsi = nullptr;
        }
    };
    auto unlock = [&] {
        if (locked) {
            ds->Unlock(ds);
            locked = false;
        }
    };
    auto free_ds = [&] {
        if (ds) {
            awt.FreeDrawingSurface(ds);
            ds = nullptr;
        }
    };

    jint lock = ds->Lock(ds);
    if (lock & JAWT_LOCK_ERROR) {
        free_ds();
        throw_jni_exception(env, "java/lang/RuntimeException", "DrawingSurface lock failed");
        return 0;
    }
    locked = true;

    dsi = ds->GetDrawingSurfaceInfo(ds);
    if (!dsi) {
        unlock();
        free_ds();
        throw_jni_exception(env, "java/lang/RuntimeException", "GetDrawingSurfaceInfo failed");
        return 0;
    }

    auto *win_info = reinterpret_cast<JAWT_Win32DrawingSurfaceInfo *>(dsi->platformInfo);
    if (win_info) parent_hwnd = win_info->hwnd;

    free_dsi();
    unlock();
    free_ds();

    if (!parent_hwnd || !IsWindow(parent_hwnd)) {
        throw_jni_exception(env, "java/lang/RuntimeException", "AWT HWND is invalid");
        return 0;
    }

    JavaVM *jvm = nullptr;
    if (env->GetJavaVM(&jvm) != JNI_OK || !jvm) {
        throw_jni_exception(env, "java/lang/RuntimeException", "GetJavaVM failed");
        return 0;
    }

    auto *ctx = new WebViewContext();
    ctx->jvm = jvm;
    ctx->parent_hwnd = parent_hwnd;
    ctx->thread = webview2_thread_create();

    const std::wstring browser_executable_folder;
    const std::wstring user_data_folder = build_user_data_folder();
    std::string user_data_detail;
    HRESULT user_data_hr = ensure_directory_exists(user_data_folder, &user_data_detail);
    if (FAILED(user_data_hr)) {
        const std::string error = build_init_error(
            "Failed to prepare WebView2 userDataFolder",
            user_data_hr,
            browser_executable_folder,
            user_data_folder,
            user_data_detail
        );
        destroy_ctx(env, ctx);
        throw_jni_exception(env, "java/lang/RuntimeException", error.c_str());
        return 0;
    }

    auto state = std::make_shared<InitState>();
    auto future = state->promise.get_future();

    webview2_thread_run_sync(ctx->thread, [ctx, state, browser_executable_folder, user_data_folder] {
        ctx->child_hwnd = CreateWindowExW(
            0,
            L"STATIC",
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            0,
            0,
            1,
            1,
            ctx->parent_hwnd,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr
        );

        if (!ctx->child_hwnd) {
            const DWORD last_error = GetLastError();
            complete_once(
                state,
                HRESULT_FROM_WIN32(last_error),
                build_init_error(
                    "CreateWindowExW failed",
                    HRESULT_FROM_WIN32(last_error),
                    browser_executable_folder,
                    user_data_folder
                )
            );
            return;
        }

        HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
            browser_executable_folder.empty() ? nullptr : browser_executable_folder.c_str(),
            user_data_folder.empty() ? nullptr : user_data_folder.c_str(),
            nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [ctx, state, browser_executable_folder, user_data_folder](HRESULT result, ICoreWebView2Environment *created_env) -> HRESULT {
                    if (FAILED(result) || !created_env) {
                        const HRESULT failure = FAILED(result) ? result : E_POINTER;
                        complete_once(
                            state,
                            failure,
                            build_init_error(
                                "CreateCoreWebView2EnvironmentWithOptions failed",
                                failure,
                                browser_executable_folder,
                                user_data_folder
                            )
                        );
                        return S_OK;
                    }

                    ctx->env = created_env;
                    return created_env->CreateCoreWebView2Controller(
                        ctx->child_hwnd,
                        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [ctx, state, browser_executable_folder, user_data_folder](HRESULT result, ICoreWebView2Controller *created_controller) -> HRESULT {
                                if (FAILED(result) || !created_controller) {
                                    const HRESULT failure = FAILED(result) ? result : E_POINTER;
                                    complete_once(
                                        state,
                                        failure,
                                        build_init_error(
                                            "CreateCoreWebView2Controller failed",
                                            failure,
                                            browser_executable_folder,
                                            user_data_folder
                                        )
                                    );
                                    return S_OK;
                                }

                                ctx->controller = created_controller;
                                if (FAILED(ctx->controller->get_CoreWebView2(&ctx->webview)) || !ctx->webview) {
                                    complete_once(
                                        state,
                                        E_FAIL,
                                        build_init_error(
                                            "ICoreWebView2Controller::get_CoreWebView2 failed",
                                            E_FAIL,
                                            browser_executable_folder,
                                            user_data_folder
                                        )
                                    );
                                    return S_OK;
                                }

                                RECT bounds{0, 0, 1, 1};
                                ctx->controller->put_Bounds(bounds);
                                ctx->controller->put_IsVisible(TRUE);

                                ComPtr<ICoreWebView2Settings> settings;
                                if (SUCCEEDED(ctx->webview->get_Settings(&settings)) && settings) {
                                    settings->put_IsScriptEnabled(TRUE);
                                    settings->put_AreDefaultScriptDialogsEnabled(TRUE);
                                    settings->put_IsWebMessageEnabled(TRUE);
                                }

                                register_url_events(ctx);
                                register_page_loading_events(ctx);
                                register_history_events(ctx);

                                ctx->webview->add_NewWindowRequested(
                                    Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                                        [ctx](ICoreWebView2 *, ICoreWebView2NewWindowRequestedEventArgs *args) -> HRESULT {
                                            if (!ctx || !ctx->webview || !args) return S_OK;
                                            LPWSTR uri = nullptr;
                                            if (SUCCEEDED(args->get_Uri(&uri)) && uri) {
                                                ctx->webview->Navigate(uri);
                                                CoTaskMemFree(uri);
                                            }
                                            args->put_Handled(TRUE);
                                            return S_OK;
                                        }
                                    ).Get(),
                                    &ctx->token_new_window_requested
                                );
                                ctx->new_window_requested_registered = true;

                                complete_once(state, S_OK, "");
                                return S_OK;
                            }
                        ).Get()
                    );
                }
            ).Get()
        );

        if (FAILED(hr)) {
            complete_once(
                state,
                hr,
                build_init_error(
                    "CreateCoreWebView2EnvironmentWithOptions dispatch failed",
                    hr,
                    browser_executable_folder,
                    user_data_folder
                )
            );
        }
    });

    HRESULT result = future.get();
    if (FAILED(result)) {
        destroy_ctx(env, ctx);
        throw_jni_exception(env, "java/lang/RuntimeException", state->error.c_str());
        return 0;
    }

    return reinterpret_cast<jlong>(ctx);
}

API_EXPORT(void, update, jlong handle, jint w, jint h, jint x, jint y) {
    (void) thiz;
    (void) x;
    (void) y;

    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }

    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    if (!ctx || !ctx->thread) return;

    const int width = clamp_dim(w);
    const int height = clamp_dim(h);

    webview2_thread_run_sync(ctx->thread, [ctx, width, height] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire)) return;
        if (!ctx->child_hwnd || !IsWindow(ctx->child_hwnd)) return;

        SetWindowPos(
            ctx->child_hwnd,
            nullptr,
            0,
            0,
            width,
            height,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW
        );

        if (ctx->controller) {
            RECT bounds{0, 0, width, height};
            ctx->controller->put_Bounds(bounds);
            ctx->controller->put_IsVisible(TRUE);
        }
    });
}

API_EXPORT(void, close0, jlong handle) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    if (!ctx) return;
    destroy_ctx(env, ctx);
}

API_EXPORT(void, requestFocus0, jlong handle) {
    (void) thiz;

    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    if (!ctx || !ctx->thread) return;

    webview2_thread_run_sync(ctx->thread, [ctx] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire)) return;
        if (ctx->child_hwnd && IsWindow(ctx->child_hwnd)) {
            SetFocus(ctx->child_hwnd);
        }
        if (ctx->controller) {
            ctx->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        }
    });
}

API_EXPORT(void, loadUrl, jlong handle, jstring url) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    if (!url) {
        throw_jni_exception(env, "java/lang/NullPointerException", "url is null");
        return;
    }

    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    if (!ctx || !ctx->thread) return;

    const char *utf8 = env->GetStringUTFChars(url, nullptr);
    if (!utf8) return;
    std::wstring wurl = utf8_to_wstring(utf8);
    env->ReleaseStringUTFChars(url, utf8);

    if (wurl.empty()) {
        wurl = L"about:blank";
    }

    webview2_thread_run_sync(ctx->thread, [ctx, wurl] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire) || !ctx->webview) return;
        ctx->webview->Navigate(wurl.c_str());
    });
}

API_EXPORT(void, refresh, jlong handle) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }

    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    if (!ctx || !ctx->thread) return;

    bool ok = true;
    std::string error;
    webview2_thread_run_sync(ctx->thread, [ctx, &ok, &error] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire) || !ctx->webview) {
            ok = false;
            error = "webview is not available";
            return;
        }

        HRESULT hr = ctx->webview->Reload();
        if (FAILED(hr)) {
            ok = false;
            error = "ICoreWebView2::Reload failed (HRESULT=" + format_hresult(hr) + ")";
        }
    });

    if (!ok) {
        throw_jni_exception(env, "java/lang/RuntimeException", error.c_str());
    }
}

API_EXPORT(jboolean, goBack, jlong handle) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return JNI_FALSE;
    }

    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    if (!ctx || !ctx->thread) return JNI_FALSE;

    BOOL can_go_back = FALSE;
    BOOL can_go_back_after = FALSE;
    bool ok = true;
    bool cannot_go_back = false;
    std::string error;
    webview2_thread_run_sync(ctx->thread, [ctx, &can_go_back, &can_go_back_after, &ok, &cannot_go_back, &error] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire) || !ctx->webview) {
            ok = false;
            error = "webview is not available";
            return;
        }

        HRESULT hr = ctx->webview->get_CanGoBack(&can_go_back);
        if (FAILED(hr)) {
            ok = false;
            error = "ICoreWebView2::get_CanGoBack failed (HRESULT=" + format_hresult(hr) + ")";
            return;
        }
        if (!can_go_back) {
            ok = false;
            cannot_go_back = true;
            error = "webview cannot go back";
            return;
        }

        hr = ctx->webview->GoBack();
        if (FAILED(hr)) {
            ok = false;
            error = "ICoreWebView2::GoBack failed (HRESULT=" + format_hresult(hr) + ")";
            return;
        }

        hr = ctx->webview->get_CanGoBack(&can_go_back_after);
        if (FAILED(hr)) {
            ok = false;
            error = "ICoreWebView2::get_CanGoBack failed after GoBack (HRESULT=" + format_hresult(hr) + ")";
        }
    });

    if (!ok) {
        const char *exception = cannot_go_back ? "java/lang/IllegalStateException" : "java/lang/RuntimeException";
        throw_jni_exception(env, exception, error.c_str());
        return JNI_FALSE;
    }
    return can_go_back_after ? JNI_TRUE : JNI_FALSE;
}

API_EXPORT(jboolean, goForward, jlong handle) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return JNI_FALSE;
    }

    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    if (!ctx || !ctx->thread) return JNI_FALSE;

    BOOL can_go_forward = FALSE;
    BOOL can_go_forward_after = FALSE;
    bool ok = true;
    bool cannot_go_forward = false;
    std::string error;
    webview2_thread_run_sync(ctx->thread, [ctx, &can_go_forward, &can_go_forward_after, &ok, &cannot_go_forward, &error] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire) || !ctx->webview) {
            ok = false;
            error = "webview is not available";
            return;
        }

        HRESULT hr = ctx->webview->get_CanGoForward(&can_go_forward);
        if (FAILED(hr)) {
            ok = false;
            error = "ICoreWebView2::get_CanGoForward failed (HRESULT=" + format_hresult(hr) + ")";
            return;
        }
        if (!can_go_forward) {
            ok = false;
            cannot_go_forward = true;
            error = "webview cannot go forward";
            return;
        }

        hr = ctx->webview->GoForward();
        if (FAILED(hr)) {
            ok = false;
            error = "ICoreWebView2::GoForward failed (HRESULT=" + format_hresult(hr) + ")";
            return;
        }

        hr = ctx->webview->get_CanGoForward(&can_go_forward_after);
        if (FAILED(hr)) {
            ok = false;
            error = "ICoreWebView2::get_CanGoForward failed after GoForward (HRESULT=" + format_hresult(hr) + ")";
        }
    });

    if (!ok) {
        const char *exception = cannot_go_forward ? "java/lang/IllegalStateException" : "java/lang/RuntimeException";
        throw_jni_exception(env, exception, error.c_str());
        return JNI_FALSE;
    }
    return can_go_forward_after ? JNI_TRUE : JNI_FALSE;
}

API_EXPORT(void, setPageLoadingStartListener, jlong handle, jobject listener) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    if (!ctx) return;
    replace_listener(env, ctx->page_loading_start_listener, listener);
}

API_EXPORT(void, setPageLoadingProgressListener, jlong handle, jobject listener) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    if (!ctx) return;
    replace_listener(env, ctx->page_loading_progress_listener, listener);
}

API_EXPORT(void, setPageLoadingEndListener, jlong handle, jobject listener) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    if (!ctx) return;
    replace_listener(env, ctx->page_loading_end_listener, listener);
}

API_EXPORT(void, setURLChangeListener, jlong handle, jobject handler) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    if (!ctx) return;
    replace_listener(env, ctx->url_listener, handler);
}

API_EXPORT(void, setCanGoBackChangeListener, jlong handle, jobject handler) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    if (!ctx) return;
    replace_listener(env, ctx->can_go_back_change_listener, handler);
    if (ctx->thread) {
        webview2_thread_run_sync(ctx->thread, [ctx] {
            emit_history_change_events(ctx);
        });
    }
}

API_EXPORT(void, setCanGoForwardChangeListener, jlong handle, jobject handler) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    if (!ctx) return;
    replace_listener(env, ctx->can_go_forward_change_listener, handler);
    if (ctx->thread) {
        webview2_thread_run_sync(ctx->thread, [ctx] {
            emit_history_change_events(ctx);
        });
    }
}
