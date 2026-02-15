#include <windows.h>

#include <jawt.h>
#include <jawt_md.h>

#include <WebView2.h>
#include <wrl.h>

#include <atomic>
#include <future>
#include <memory>
#include <sstream>
#include <string>

#include "thread.h"
#include "utils.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

struct WebViewContext {
    WebView2Environment *environment = nullptr;
    HWND parentHwnd = nullptr;
    HWND childHwnd = nullptr;

    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
};

namespace {

    struct InitState {
        std::promise<HRESULT> promise;
        std::atomic_bool done{false};
        std::string error;
    };

    std::string hex_hresult(HRESULT hr) {
        std::ostringstream oss;
        oss << "0x" << std::hex << std::uppercase << static_cast<unsigned long>(hr);
        return oss.str();
    }

    std::string hresult_to_readable_string(HRESULT hr) {
        LPWSTR wideBuffer = nullptr;
        const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
                            FORMAT_MESSAGE_FROM_SYSTEM |
                            FORMAT_MESSAGE_IGNORE_INSERTS;

        const DWORD length = FormatMessageW(
            flags,
            nullptr,
            static_cast<DWORD>(hr),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPWSTR>(&wideBuffer),
            0,
            nullptr
        );

        if (length == 0 || wideBuffer == nullptr) {
            return "Unknown error";
        }

        const int utf8Length = WideCharToMultiByte(
            CP_UTF8,
            0,
            wideBuffer,
            static_cast<int>(length),
            nullptr,
            0,
            nullptr,
            nullptr
        );

        if (utf8Length <= 0) {
            LocalFree(wideBuffer);
            return "Unknown error";
        }

        std::string message(static_cast<size_t>(utf8Length), '\0');
        char *outBuffer = message.empty() ? nullptr : &message[0];
        WideCharToMultiByte(
            CP_UTF8,
            0,
            wideBuffer,
            static_cast<int>(length),
            outBuffer,
            utf8Length,
            nullptr,
            nullptr
        );

        LocalFree(wideBuffer);

        while (!message.empty() &&
               (message.back() == '\r' || message.back() == '\n' || message.back() == ' ' || message.back() == '.')) {
            message.pop_back();
        }

        if (message.empty()) {
            return "Unknown error";
        }

        return message;
    }

    std::string stage_error(const char *stage, HRESULT hr) {
        std::ostringstream oss;
        oss << stage << " failed: " << hresult_to_readable_string(hr)
            << " (HRESULT=" << hex_hresult(hr) << ")";
        return oss.str();
    }

    void cleanup_context(WebViewContext *ctx) {
        if (!ctx) return;

        if (ctx->environment) {
            webview2_environment_run_sync(ctx->environment, [ctx] {
                ctx->webview.Reset();
                ctx->controller.Reset();

                if (ctx->childHwnd != nullptr && IsWindow(ctx->childHwnd)) {
                    DestroyWindow(ctx->childHwnd);
                    ctx->childHwnd = nullptr;
                }
            });

            webview2_environment_destroy(ctx->environment);
            ctx->environment = nullptr;
        }

        delete ctx;
    }

} // namespace

API_EXPORT(jlong, initAndAttach) {
    HWND parentHwnd = nullptr;

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

    bool dsLocked = false;
    JAWT_DrawingSurfaceInfo *dsi = nullptr;

    auto releaseDsi = [&] {
        if (dsi) {
            ds->FreeDrawingSurfaceInfo(dsi);
            dsi = nullptr;
        }
    };
    auto unlockDs = [&] {
        if (ds && dsLocked) {
            ds->Unlock(ds);
            dsLocked = false;
        }
    };
    auto freeDs = [&] {
        if (ds) {
            awt.FreeDrawingSurface(ds);
            ds = nullptr;
        }
    };

    jint lock = ds->Lock(ds);
    if (lock & JAWT_LOCK_ERROR) {
        freeDs();
        throw_jni_exception(env, "java/lang/RuntimeException", "DrawingSurface lock failed");
        return 0;
    }
    dsLocked = true;

    dsi = ds->GetDrawingSurfaceInfo(ds);
    if (!dsi) {
        unlockDs();
        freeDs();
        throw_jni_exception(env, "java/lang/RuntimeException", "GetDrawingSurfaceInfo failed");
        return 0;
    }

    auto *winInfo = reinterpret_cast<JAWT_Win32DrawingSurfaceInfo *>(dsi->platformInfo);
    if (winInfo) {
        parentHwnd = winInfo->hwnd;
    }

    releaseDsi();
    unlockDs();
    freeDs();

    if (parentHwnd == nullptr || !IsWindow(parentHwnd)) {
        throw_jni_exception(env, "java/lang/RuntimeException", "AWT HWND is invalid");
        return 0;
    }

    auto *ctx = new WebViewContext();
    ctx->parentHwnd = parentHwnd;
    ctx->environment = webview2_environment_init();
    if (!ctx->environment) {
        delete ctx;
        throw_jni_exception(env, "java/lang/RuntimeException", "webview2_environment_init failed");
        return 0;
    }

    auto state = std::make_shared<InitState>();
    auto future = state->promise.get_future();

    auto complete = [state](HRESULT hr, const std::string &message) {
        bool expected = false;
        if (!state->done.compare_exchange_strong(expected, true)) {
            return;
        }
        state->error = message;
        state->promise.set_value(hr);
    };

    webview2_environment_run_async(ctx->environment, [ctx, complete] {
        if (!IsWindow(ctx->parentHwnd)) {
            complete(E_HANDLE, "Parent HWND is not valid");
            return;
        }

        ctx->childHwnd = CreateWindowExW(
            0,
            L"STATIC",
            L"",
            WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            0,
            0,
            1,
            1,
            ctx->parentHwnd,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr
        );

        if (ctx->childHwnd == nullptr) {
            complete(HRESULT_FROM_WIN32(GetLastError()), "Create child HWND failed");
            return;
        }

        ShowWindow(ctx->childHwnd, SW_HIDE);

        HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
            nullptr,
            nullptr,
            nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [ctx, complete](HRESULT result, ICoreWebView2Environment *webviewEnv) -> HRESULT {
                    if (FAILED(result) || webviewEnv == nullptr) {
                        const HRESULT errorCode = FAILED(result) ? result : E_POINTER;
                        complete(errorCode, stage_error("CreateCoreWebView2EnvironmentWithOptions", errorCode));
                        return S_OK;
                    }

                    return webviewEnv->CreateCoreWebView2Controller(
                        ctx->childHwnd,
                        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [ctx, complete](HRESULT result, ICoreWebView2Controller *controller) -> HRESULT {
                                if (FAILED(result) || controller == nullptr) {
                                    const HRESULT errorCode = FAILED(result) ? result : E_POINTER;
                                    complete(errorCode, stage_error("CreateCoreWebView2Controller", errorCode));
                                    return S_OK;
                                }

                                ctx->controller = controller;

                                HRESULT hrGetWebView = ctx->controller->get_CoreWebView2(&ctx->webview);
                                if (FAILED(hrGetWebView) || !ctx->webview) {
                                    complete(hrGetWebView, stage_error("ICoreWebView2Controller::get_CoreWebView2", hrGetWebView));
                                    return S_OK;
                                }

                                ComPtr<ICoreWebView2Settings> settings;
                                if (SUCCEEDED(ctx->webview->get_Settings(&settings)) && settings) {
                                    settings->put_IsScriptEnabled(TRUE);
                                    settings->put_AreDefaultScriptDialogsEnabled(TRUE);
                                    settings->put_IsWebMessageEnabled(TRUE);
                                }

                                RECT bounds{0, 0, 1, 1};
                                ctx->controller->put_Bounds(bounds);
                                ctx->controller->put_IsVisible(TRUE);

                                complete(S_OK, "");
                                return S_OK;
                            }
                        ).Get()
                    );
                }
            ).Get()
        );

        if (FAILED(hr)) {
            complete(hr, stage_error("CreateCoreWebView2EnvironmentWithOptions(start)", hr));
            return;
        }
    });

    HRESULT initHr = future.get();
    if (FAILED(initHr)) {
        std::string msg = state->error.empty() ? stage_error("initAndAttach", initHr) : state->error;
        cleanup_context(ctx);
        throw_jni_exception(env, "java/lang/RuntimeException", msg.c_str());
        return 0;
    }

    return static_cast<jlong>(reinterpret_cast<uintptr_t>(ctx));
}

API_EXPORT(void, update, jlong handle, jint w, jint h, jint x, jint y) {
    (void) env;
    (void) thiz;
    (void) x;
    (void) y;

    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }

    auto *ctx = reinterpret_cast<WebViewContext *>(static_cast<uintptr_t>(handle));
    if (!ctx) {
        return;
    }

    if (!ctx->environment) {
        throw_jni_exception(env, "java/lang/RuntimeException", "webview environment is null");
        return;
    }

    const int width = (w > 0) ? static_cast<int>(w) : 1;
    const int height = (h > 0) ? static_cast<int>(h) : 1;

    webview2_environment_run_sync(ctx->environment, [ctx, width, height] {
        if (!ctx) return;
        if (ctx->childHwnd == nullptr || !IsWindow(ctx->childHwnd)) return;

        SetWindowPos(
            ctx->childHwnd,
            nullptr,
            0,
            0,
            width,
            height,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW
        );

        if (!ctx->controller) return;

        RECT bounds{0, 0, width, height};
        ctx->controller->put_Bounds(bounds);
        ctx->controller->put_IsVisible(TRUE);
    });
}

API_EXPORT(void, close0, jlong handle) {
    (void) env;
    (void) thiz;
    (void) handle;
}

API_EXPORT(void, loadUrl, jlong handle, jstring url) {
    (void) env;
    (void) thiz;
    (void) handle;
    (void) url;
}

API_EXPORT(void, setProgressListener, jlong handle, jobject listener) {
    (void) env;
    (void) thiz;
    (void) handle;
    (void) listener;
}

API_EXPORT(void, setNavigationHandler, jlong handle, jobject handler) {
    (void) env;
    (void) thiz;
    (void) handle;
    (void) handler;
}
