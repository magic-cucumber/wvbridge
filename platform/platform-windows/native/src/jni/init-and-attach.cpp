#include "libs_helpers.h"
#include "javascript-helpers.h"
#include <wvbridge/logger.h>
#include <wvbridge/webview-platform-settings.h>

API_EXPORT(jlong, initAndAttach, jobject platformSetting) {
    LOGGER_I("initAndAttach: called");
    WvBridgeWindowsWebViewPlatformSetting setting;
    if (!parse_webview_platform_settings(env, platformSetting, &setting)) {
        LOGGER_E("initAndAttach: failed to parse Windows platform setting");
        return 0;
    }

    HWND parent_hwnd = nullptr;

    JAWT awt{};
    awt.version = JAWT_VERSION_1_4;
    if (JAWT_GetAWT(env, &awt) == JNI_FALSE) {
        LOGGER_E("initAndAttach: JAWT_GetAWT failed, JNI exception will be set");
        throw_jni_exception(env, "java/lang/RuntimeException", "JAWT_GetAWT failed");
        return 0;
    }
    LOGGER_V("initAndAttach: JAWT_GetAWT succeeded");

    JAWT_DrawingSurface *ds = awt.GetDrawingSurface(env, thiz);
    if (!ds) {
        LOGGER_E("initAndAttach: GetDrawingSurface returned null, JNI exception will be set");
        throw_jni_exception(env, "java/lang/RuntimeException", "GetDrawingSurface failed");
        return 0;
    }
    LOGGER_V("initAndAttach: GetDrawingSurface succeeded, ds=%p", ds);

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
        LOGGER_E("initAndAttach: DrawingSurface lock failed, JNI exception will be set");
        free_ds();
        throw_jni_exception(env, "java/lang/RuntimeException", "DrawingSurface lock failed");
        return 0;
    }
    locked = true;
    LOGGER_V("initAndAttach: DrawingSurface locked successfully");

    dsi = ds->GetDrawingSurfaceInfo(ds);
    if (!dsi) {
        LOGGER_E("initAndAttach: GetDrawingSurfaceInfo returned null, JNI exception will be set");
        unlock();
        free_ds();
        throw_jni_exception(env, "java/lang/RuntimeException", "GetDrawingSurfaceInfo failed");
        return 0;
    }
    LOGGER_V("initAndAttach: GetDrawingSurfaceInfo succeeded");

    auto *win_info = reinterpret_cast<JAWT_Win32DrawingSurfaceInfo *>(dsi->platformInfo);
    if (win_info) parent_hwnd = win_info->hwnd;
    LOGGER_V("initAndAttach: parent_hwnd=%p", parent_hwnd);

    free_dsi();
    unlock();
    free_ds();
    LOGGER_V("initAndAttach: released DS resources");

    if (!parent_hwnd || !IsWindow(parent_hwnd)) {
        LOGGER_E("initAndAttach: AWT HWND is invalid, parent_hwnd=%p", parent_hwnd);
        throw_jni_exception(env, "java/lang/RuntimeException", "AWT HWND is invalid");
        return 0;
    }

    auto *ctx = new WebViewContext();
    ctx->parent_hwnd = parent_hwnd;
    ctx->thread = webview2_thread_create();
    LOGGER_V("initAndAttach: created context=%p thread=%p", ctx, ctx->thread);

    const std::wstring browser_executable_folder;
    const std::wstring user_data_folder = utf8_to_wstring(setting.data_dir.c_str());
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
        LOGGER_E("initAndAttach: ensure_directory_exists failed, hr=0x%lx", (unsigned long)user_data_hr);
        destroy_ctx(ctx);
        throw_jni_exception(env, "java/lang/RuntimeException", error.c_str());
        return 0;
    }
    LOGGER_V("initAndAttach: user data folder ready");

    auto state = std::make_shared<InitState>();
    auto future = state->promise.get_future();

    LOGGER_V("initAndAttach: dispatching WebView2 init to webview thread");
    webview2_thread_run_sync(ctx->thread, [ctx, state, browser_executable_folder, user_data_folder, setting] {
        LOGGER_V("initAndAttach: creating child window via CreateWindowExW");
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
            LOGGER_V("initAndAttach: CreateWindowExW failed, GetLastError=%lu", (unsigned long)last_error);
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
        LOGGER_V("initAndAttach: child window created, child_hwnd=%p", ctx->child_hwnd);

        LOGGER_V("initAndAttach: calling CreateCoreWebView2EnvironmentWithOptions");
        HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
            browser_executable_folder.empty() ? nullptr : browser_executable_folder.c_str(),
            user_data_folder.empty() ? nullptr : user_data_folder.c_str(),
            nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [ctx, state, browser_executable_folder, user_data_folder, setting](HRESULT result, ICoreWebView2Environment *created_env) -> HRESULT {
                    if (FAILED(result) || !created_env) {
                        const HRESULT failure = FAILED(result) ? result : E_POINTER;
                        LOGGER_V("initAndAttach: CreateCoreWebView2EnvironmentWithOptions callback failed, hr=0x%lx", (unsigned long)failure);
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

                    LOGGER_V("initAndAttach: environment created, calling CreateCoreWebView2Controller");
                    ctx->env = created_env;
                    return created_env->CreateCoreWebView2Controller(
                        ctx->child_hwnd,
                        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [ctx, state, browser_executable_folder, user_data_folder, setting](HRESULT result, ICoreWebView2Controller *created_controller) -> HRESULT {
                                if (FAILED(result) || !created_controller) {
                                    const HRESULT failure = FAILED(result) ? result : E_POINTER;
                                    LOGGER_V("initAndAttach: CreateCoreWebView2Controller callback failed, hr=0x%lx", (unsigned long)failure);
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

                                LOGGER_V("initAndAttach: controller created, getting CoreWebView2");
                                ctx->controller = created_controller;
                                if (FAILED(ctx->controller->get_CoreWebView2(&ctx->webview)) || !ctx->webview) {
                                    LOGGER_V("initAndAttach: get_CoreWebView2 failed or returned null");
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
                                    if (!setting.user_agent.empty()) {
                                        ComPtr<ICoreWebView2Settings2> settings2;
                                        if (SUCCEEDED(settings.As(&settings2)) && settings2) {
                                            const std::wstring user_agent = utf8_to_wstring(setting.user_agent.c_str());
                                            settings2->put_UserAgent(user_agent.c_str());
                                        }
                                    }
                                    LOGGER_V("initAndAttach: settings configured");
                                }

                                HRESULT web_message_hr = ensure_web_message_registered(ctx);
                                if (FAILED(web_message_hr)) {
                                    LOGGER_V("initAndAttach: add_WebMessageReceived failed, hr=0x%lx", (unsigned long)web_message_hr);
                                    complete_once(
                                        state,
                                        web_message_hr,
                                        build_init_error(
                                            "add_WebMessageReceived failed",
                                            web_message_hr,
                                            browser_executable_folder,
                                            user_data_folder
                                        )
                                    );
                                    return S_OK;
                                }
                                LOGGER_V("initAndAttach: web message handler registered");

                                ctx->events = webview_events_create(ctx);
                                LOGGER_V("initAndAttach: webview events created");

                                complete_once(state, S_OK, "");
                                LOGGER_V("initAndAttach: init complete, state set to S_OK");
                                return S_OK;
                            }
                        ).Get()
                    );
                }
            ).Get()
        );

        if (FAILED(hr)) {
            LOGGER_V("initAndAttach: CreateCoreWebView2EnvironmentWithOptions dispatch failed, hr=0x%lx", (unsigned long)hr);
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
        LOGGER_E("initAndAttach: WebView2 init failed, hr=0x%lx error=%s", (unsigned long)result, state->error.c_str());
        destroy_ctx(ctx);
        throw_jni_exception(env, "java/lang/RuntimeException", state->error.c_str());
        return 0;
    }

    LOGGER_I("initAndAttach: success, returning context=%p", ctx);
    return reinterpret_cast<jlong>(ctx);
}
