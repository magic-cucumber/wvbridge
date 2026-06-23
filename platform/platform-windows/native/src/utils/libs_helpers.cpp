#include "libs_helpers.h"

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

std::string format_hresult_code(HRESULT hr) {
    std::ostringstream oss;
    oss << "0x"
        << std::uppercase
        << std::hex
        << std::setw(8)
        << std::setfill('0')
        << static_cast<unsigned long>(static_cast<unsigned int>(hr));
    return oss.str();
}

const char *known_hresult_name(HRESULT hr) {
    switch (hr) {
        case S_OK:
            return "S_OK";
        case S_FALSE:
            return "S_FALSE";
        case E_ABORT:
            return "E_ABORT";
        case E_ACCESSDENIED:
            return "E_ACCESSDENIED";
        case E_FAIL:
            return "E_FAIL";
        case E_HANDLE:
            return "E_HANDLE";
        case E_INVALIDARG:
            return "E_INVALIDARG";
        case E_NOINTERFACE:
            return "E_NOINTERFACE";
        case E_NOTIMPL:
            return "E_NOTIMPL";
        case E_OUTOFMEMORY:
            return "E_OUTOFMEMORY";
        case E_POINTER:
            return "E_POINTER";
        case E_UNEXPECTED:
            return "E_UNEXPECTED";
        case HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND):
            return "ERROR_FILE_NOT_FOUND";
        case HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND):
            return "ERROR_PATH_NOT_FOUND";
        case HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND):
            return "ERROR_MOD_NOT_FOUND";
        case HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND):
            return "ERROR_PROC_NOT_FOUND";
        case HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS):
            return "ERROR_ALREADY_EXISTS";
        case HRESULT_FROM_WIN32(ERROR_FILE_EXISTS):
            return "ERROR_FILE_EXISTS";
        case HRESULT_FROM_WIN32(ERROR_BUSY):
            return "ERROR_BUSY";
        case HRESULT_FROM_WIN32(ERROR_OPERATION_ABORTED):
            return "ERROR_OPERATION_ABORTED";
        case HRESULT_FROM_WIN32(ERROR_CANCELLED):
            return "ERROR_CANCELLED";
        case HRESULT_FROM_WIN32(ERROR_TIMEOUT):
            return "ERROR_TIMEOUT";
        case HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED):
            return "ERROR_NOT_SUPPORTED";
        case HRESULT_FROM_WIN32(ERROR_NOT_FOUND):
            return "ERROR_NOT_FOUND";
        case HRESULT_FROM_WIN32(ERROR_INVALID_STATE):
            return "ERROR_INVALID_STATE";
        default:
            return nullptr;
    }
}

std::string format_hresult(HRESULT hr) {
    const char *name = known_hresult_name(hr);
    if (name) {
        return std::string(name) + " (" + format_hresult_code(hr) + ")";
    }

    if (HRESULT_FACILITY(hr) == FACILITY_WIN32) {
        std::ostringstream oss;
        oss << "HRESULT_FROM_WIN32(" << HRESULT_CODE(hr) << ") (" << format_hresult_code(hr) << ")";
        return oss.str();
    }

    return "HRESULT(" + format_hresult_code(hr) + ")";
}

std::string format_system_message(HRESULT hr) {
    LPWSTR raw = nullptr;
    DWORD message_id = static_cast<DWORD>(hr);
    if (HRESULT_FACILITY(hr) == FACILITY_WIN32) {
        message_id = HRESULT_CODE(hr);
    }
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD len = FormatMessageW(
        flags,
        nullptr,
        message_id,
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
    const std::string &extra
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

void destroy_ctx(WebViewContext *ctx) {
    if (!ctx) return;

    ctx->closing.store(true, std::memory_order_release);

    if (ctx->thread) {
        webview2_thread_run_sync(ctx->thread, [ctx] {
            webview_events_destroy(ctx->events);
            ctx->events = nullptr;

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
