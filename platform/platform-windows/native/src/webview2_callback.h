#pragma once

#include <Unknwn.h>

#include <WebView2.h>
#include <wrl.h>

#include <atomic>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

template<typename Interface>
const IID &webview2_handler_iid();

template<>
inline const IID &webview2_handler_iid<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>() {
    return IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler;
}

template<>
inline const IID &webview2_handler_iid<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>() {
    return IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler;
}

template<>
inline const IID &webview2_handler_iid<ICoreWebView2SourceChangedEventHandler>() {
    return IID_ICoreWebView2SourceChangedEventHandler;
}

template<>
inline const IID &webview2_handler_iid<ICoreWebView2NavigationStartingEventHandler>() {
    return IID_ICoreWebView2NavigationStartingEventHandler;
}

template<>
inline const IID &webview2_handler_iid<ICoreWebView2ContentLoadingEventHandler>() {
    return IID_ICoreWebView2ContentLoadingEventHandler;
}

template<>
inline const IID &webview2_handler_iid<ICoreWebView2NavigationCompletedEventHandler>() {
    return IID_ICoreWebView2NavigationCompletedEventHandler;
}

template<>
inline const IID &webview2_handler_iid<ICoreWebView2HistoryChangedEventHandler>() {
    return IID_ICoreWebView2HistoryChangedEventHandler;
}

template<>
inline const IID &webview2_handler_iid<ICoreWebView2NewWindowRequestedEventHandler>() {
    return IID_ICoreWebView2NewWindowRequestedEventHandler;
}

template<typename Interface, typename Fn, typename Signature>
class LambdaComCallbackImpl;

template<typename Interface, typename Fn, typename Ret, typename... Args>
class LambdaComCallbackImpl<Interface, Fn, Ret(STDMETHODCALLTYPE Interface::*)(Args...)> final : public Interface {
public:
    explicit LambdaComCallbackImpl(Fn fn) : fn_(std::move(fn)) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override {
        if (!ppvObject) return E_POINTER;
        *ppvObject = nullptr;
        if (riid == IID_IUnknown || riid == webview2_handler_iid<Interface>()) {
            *ppvObject = static_cast<Interface *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(ref_count_.fetch_add(1, std::memory_order_relaxed) + 1);
    }

    ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = static_cast<ULONG>(ref_count_.fetch_sub(1, std::memory_order_acq_rel) - 1);
        if (count == 0) delete this;
        return count;
    }

    Ret STDMETHODCALLTYPE Invoke(Args... args) override {
        return fn_(args...);
    }

private:
    std::atomic_ulong ref_count_{1};
    Fn fn_;
};

template<typename Interface, typename Fn>
Microsoft::WRL::ComPtr<Interface> Callback(Fn &&fn) {
    using FnType = std::decay_t<Fn>;
    using Impl = LambdaComCallbackImpl<Interface, FnType, decltype(&Interface::Invoke)>;
    Microsoft::WRL::ComPtr<Interface> callback;
    callback.Attach(new (std::nothrow) Impl(FnType(std::forward<Fn>(fn))));
    return callback;
}
