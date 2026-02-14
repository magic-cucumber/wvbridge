#include <windows.h>

#include <atomic>
#include <string>
#include <cstring>

#include <jni.h>
#include <jawt.h>
#include <jawt_md.h>

#include <wrl.h>

#include <WebView2.h>

#include "utils.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace {

    struct WebViewContext {
        HWND parent_hwnd = nullptr; // AWT 宿主窗口
        HWND host_hwnd = nullptr;   // WebView2 Controller 宿主子窗口

        ComPtr<ICoreWebView2Environment> env;
        ComPtr<ICoreWebView2Controller> controller;
        ComPtr<ICoreWebView2> webview;

        std::atomic_bool closing{false};
        bool com_inited = false;
    };

    std::wstring utf8_to_wide(const char *s) {
        if (!s) return L"";
        const int len = (int) strlen(s);
        if (len <= 0) return L"";

        int wlen = MultiByteToWideChar(CP_UTF8, 0, s, len, nullptr, 0);
        if (wlen <= 0) return L"";

        std::wstring out;
        out.resize(wlen);

        // 注意：在 C++17 之前 std::wstring::data() 返回 const wchar_t*，
        // 不能作为 MultiByteToWideChar 的输出缓冲区。
        MultiByteToWideChar(CP_UTF8, 0, s, len, &out[0], wlen);
        return out;
    }

    HWND get_parent_hwnd_from_awt(JNIEnv *env, jobject thiz) {
        JAWT awt;
        awt.version = JAWT_VERSION_1_4;
        if (JAWT_GetAWT(env, &awt) == JNI_FALSE) {
            throw_jni_exception(env, "java/lang/RuntimeException", "JAWT_GetAWT failed");
            return nullptr;
        }

        JAWT_DrawingSurface *ds = awt.GetDrawingSurface(env, thiz);
        if (!ds) {
            throw_jni_exception(env, "java/lang/RuntimeException", "GetDrawingSurface failed");
            return nullptr;
        }

        bool ds_locked = false;
        JAWT_DrawingSurfaceInfo *dsi = nullptr;

        auto free_dsi = [&] {
            if (dsi) {
                ds->FreeDrawingSurfaceInfo(dsi);
                dsi = nullptr;
            }
        };

        auto unlock_ds = [&] {
            if (ds && ds_locked) {
                ds->Unlock(ds);
                ds_locked = false;
            }
        };

        auto free_ds = [&] {
            if (ds) {
                awt.FreeDrawingSurface(ds);
                ds = nullptr;
            }
        };

        const jint lock = ds->Lock(ds);
        if (lock & JAWT_LOCK_ERROR) {
            free_ds();
            throw_jni_exception(env, "java/lang/RuntimeException", "DrawingSurface lock failed");
            return nullptr;
        }
        ds_locked = true;

        dsi = ds->GetDrawingSurfaceInfo(ds);
        if (!dsi) {
            unlock_ds();
            free_ds();
            throw_jni_exception(env, "java/lang/RuntimeException", "GetDrawingSurfaceInfo failed");
            return nullptr;
        }

        HWND hwnd = nullptr;
        auto *winInfo = static_cast<JAWT_Win32DrawingSurfaceInfo *>(dsi->platformInfo);
        if (winInfo) {
            hwnd = winInfo->hwnd;
        }

        free_dsi();
        unlock_ds();
        free_ds();

        if (!hwnd) {
            throw_jni_exception(env, "java/lang/RuntimeException", "AWT hwnd is null");
            return nullptr;
        }

        return hwnd;
    }

    // 注册一个用于承载 WebView2 的子窗口类（一次性）。
    ATOM ensure_host_window_class() {
        static ATOM atom = 0;
        if (atom) return atom;

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"WVBridgeWebViewHost";

        atom = RegisterClassExW(&wc);
        return atom;
    }

    HWND create_host_child_window(HWND parent) {
        ensure_host_window_class();

        RECT rc{};
        GetClientRect(parent, &rc);
        const int w = (rc.right - rc.left);
        const int h = (rc.bottom - rc.top);

        HWND host = CreateWindowExW(
            0,
            L"WVBridgeWebViewHost",
            L"",
            WS_CHILD | WS_VISIBLE,
            0,
            0,
            (w > 0 ? w : 1),
            (h > 0 ? h : 1),
            parent,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

        return host;
    }

    bool create_webview2_sync(WebViewContext *ctx, int timeout_ms) {
        if (!ctx || !ctx->host_hwnd) return false;

        HANDLE done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!done) return false;

        HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
            nullptr,
            nullptr,
            nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [ctx, done](HRESULT result, ICoreWebView2Environment *env) -> HRESULT {
                    if (SUCCEEDED(result) && env) {
                        ctx->env = env;
                    }
                    SetEvent(done);
                    return S_OK;
                })
                .Get());

        if (FAILED(hr)) {
            CloseHandle(done);
            return false;
        }

        WaitForSingleObject(done, (DWORD) timeout_ms);
        CloseHandle(done);

        if (!ctx->env) return false;

        // 2) Create controller
        HANDLE done2 = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!done2) return false;

        hr = ctx->env->CreateCoreWebView2Controller(
            ctx->host_hwnd,
            Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                [ctx, done2](HRESULT result, ICoreWebView2Controller *controller) -> HRESULT {
                    if (SUCCEEDED(result) && controller) {
                        ctx->controller = controller;
                        controller->get_CoreWebView2(&ctx->webview);
                    }
                    SetEvent(done2);
                    return S_OK;
                })
                .Get());

        if (FAILED(hr)) {
            CloseHandle(done2);
            return false;
        }

        WaitForSingleObject(done2, (DWORD) timeout_ms);
        CloseHandle(done2);

        if (!ctx->controller || !ctx->webview) return false;

        // 初始 bounds
        RECT rc{};
        GetClientRect(ctx->host_hwnd, &rc);
        ctx->controller->put_Bounds(rc);
        ctx->controller->put_IsVisible(TRUE);

        // 默认加载空白页
        ctx->webview->Navigate(L"about:blank");
        return true;
    }

} // namespace

// 示例 DLL 导出函数：用于验证 DLL 符号导出正常（与 JNI 导出无关）。
// 该函数名会以 "DLL_Export" 形式导出（无 C++ name mangling）。
extern "C" __declspec(dllexport) const char *DLL_Export() {
    return "wvbridge/windows: DLL_Export example";
}

// 示例：Windows 下使用 JAWT 取得 AWT HWND，并创建 WebView2。
API_EXPORT(jlong, initAndAttach) {
    HWND parent = get_parent_hwnd_from_awt(env, thiz);
    if (!parent) return 0;

    auto *ctx = new WebViewContext();
    ctx->parent_hwnd = parent;

    // WebView2 依赖 COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr)) {
        ctx->com_inited = true;
    }

    ctx->host_hwnd = create_host_child_window(parent);
    if (!ctx->host_hwnd) {
        if (ctx->com_inited) CoUninitialize();
        delete ctx;
        throw_jni_exception(env, "java/lang/RuntimeException", "Create host window failed");
        return 0;
    }

    if (!create_webview2_sync(ctx, 10 * 1000)) {
        DestroyWindow(ctx->host_hwnd);
        ctx->host_hwnd = nullptr;
        if (ctx->com_inited) CoUninitialize();
        delete ctx;
        throw_jni_exception(env, "java/lang/RuntimeException", "Create WebView2 failed");
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
    if (!ctx || ctx->closing.load(std::memory_order_acquire)) return;

    const int cw = (w < 1) ? 1 : (int) w;
    const int ch = (h < 1) ? 1 : (int) h;

    if (ctx->host_hwnd) {
        MoveWindow(ctx->host_hwnd, 0, 0, cw, ch, TRUE);
    }

    if (ctx->controller) {
        RECT bounds{0, 0, cw, ch};
        ctx->controller->put_Bounds(bounds);
    }
}

API_EXPORT(void, close0, jlong handle) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    if (!ctx) return;

    ctx->closing.store(true, std::memory_order_release);

    if (ctx->controller) {
        ctx->controller->Close();
    }

    ctx->webview.Reset();
    ctx->controller.Reset();
    ctx->env.Reset();

    if (ctx->host_hwnd) {
        DestroyWindow(ctx->host_hwnd);
        ctx->host_hwnd = nullptr;
    }

    if (ctx->com_inited) {
        CoUninitialize();
        ctx->com_inited = false;
    }

    delete ctx;
}

API_EXPORT(void, loadUrl, jlong handle, jstring url) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    if (url == nullptr) {
        throw_jni_exception(env, "java/lang/NullPointerException", "url is null");
        return;
    }

    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    if (!ctx || !ctx->webview) return;

    const char *nativeString = env->GetStringUTFChars(url, nullptr);
    if (!nativeString) return;

    std::wstring wurl = utf8_to_wide(nativeString);
    env->ReleaseStringUTFChars(url, nativeString);

    if (wurl.empty()) wurl = L"about:blank";

    ctx->webview->Navigate(wurl.c_str());
}

// Windows 示例中先留空：仅保证 JNI 符号存在，便于 Java/Kotlin 侧链接。
API_EXPORT(void, setProgressListener, jlong handle, jobject listener) {
    (void) handle;
    (void) listener;
}

API_EXPORT(void, setNavigationHandler, jlong handle, jobject handler) {
    (void) handle;
    (void) handler;
}
