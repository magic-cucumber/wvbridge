#pragma once

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

#include "utils.h"
#include "thread.h"
#include "webview2_callback.h"
#include "webview_context.h"
#include "webview_events.h"

using Microsoft::WRL::ComPtr;

struct InitState {
    std::promise<HRESULT> promise;
    std::atomic_bool done{false};
    std::string error;
};

int clamp_dim(jint v);
std::string format_hresult(HRESULT hr);
HRESULT ensure_directory_exists(const std::wstring &path, std::string *detail);
std::string build_init_error(
    const char *stage,
    HRESULT hr,
    const std::wstring &browser_executable_folder,
    const std::wstring &user_data_folder,
    const std::string &extra = ""
);
void destroy_ctx(WebViewContext *ctx);
void complete_once(const std::shared_ptr<InitState> &state, HRESULT hr, std::string error);
