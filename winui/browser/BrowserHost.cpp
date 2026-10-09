#include <unknwn.h>
#include "BrowserHost.h"
#include "WebLifecycle.h"
#include "../../src/WebViewZoom.h"
#include "../../src/ConfigStore.h"
#include <winrt/Microsoft.UI.Dispatching.h>
#include <wil/cppwinrt_helpers.h>
#include <WebView2EnvironmentOptions.h>
#include <wrl.h>
#include <vector>
using Microsoft::WRL::Callback;
namespace miniwindow {
namespace {
struct Creation {
    wil::unique_handle event{CreateEventW(nullptr,TRUE,FALSE,nullptr)};
    HRESULT result=E_PENDING;
    void Finish(HRESULT hr) {result=hr;SetEvent(event.get());}
};
std::wstring BrowserError(HRESULT hr) {return L"WebView2 操作失败（"+std::to_wstring(static_cast<unsigned long>(hr))+L"）。请检查 Runtime，或重试。";}
bool Supported(const std::wstring& url) {
    return url.starts_with(L"https://")||url.starts_with(L"http://")||url.starts_with(L"file:///")||url==L"about:blank";
}
}
struct BrowserHost::State {
    HWND parent=nullptr;
    std::filesystem::path profile;
    winrt::Microsoft::UI::Dispatching::DispatcherQueue queue{nullptr};
    WebLifecycle life;
    wil::com_ptr<ICoreWebView2Environment> environment;
    wil::com_ptr<ICoreWebView2Controller> controller;
    wil::com_ptr<ICoreWebView2> view;
    xiaochuang::WebViewZoom zoom;
    AppSettings settings;
    RECT bounds{};
    bool ready=false,loading=false,visible=true,waiting=false,events=false;
    UINT64 navigationId=0;
    uint64_t generation=0;
    std::wstring pendingUrl;
    std::shared_ptr<Creation> creation;
    std::function<void(bool,bool,std::wstring)> status;
    std::function<void(PageSignal)> signal;
    std::function<void(std::wstring,std::wstring)> document;
    std::function<void()> cancelGesture,closeTab;
    std::function<void(std::wstring)> newTab;
    EventRegistrationToken starting{},content{},completed{},source{},title{},message{},fullscreen{},newWindow{},closeWindow{},failed{};
    void Report(std::wstring error={}) {if(status) status(ready,loading,std::move(error));}
    void UpdateDocument() {
        if(waiting||!view||!document) return;
        wil::unique_cotaskmem_string url,name;
        if(SUCCEEDED(view->get_Source(&url))&&SUCCEEDED(view->get_DocumentTitle(&name))) document(url?url.get():L"",name?name.get():L"");
    }
};
BrowserHost::BrowserHost(HWND parent,std::filesystem::path profile,std::function<void(bool,bool,std::wstring)> status):state_(std::make_shared<State>()) {
    state_->parent=parent;state_->profile=std::move(profile);state_->status=std::move(status);
    state_->queue=winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
}
BrowserHost::~BrowserHost() {Close();}
winrt::Windows::Foundation::IAsyncAction BrowserHost::InitializeAsync() {
    auto state=state_;
    const auto generation=state->life.Begin();state->generation=generation;
    state->ready=false;state->loading=false;
    auto creation=std::make_shared<Creation>();
    if(!creation->event) {state->Report(L"无法创建浏览器初始化事件。");co_return;}
    state->creation=creation;
    auto options=Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    if(state->settings.renderMode==xiaochuang::RenderMode::SoftwareCompatibility) options->put_AdditionalBrowserArguments(L"--disable-gpu");
    std::weak_ptr<State> weak=state;
    const auto hr=CreateCoreWebView2EnvironmentWithOptions(nullptr,state->profile.c_str(),options.Get(),
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>([weak,generation,creation](HRESULT result,ICoreWebView2Environment* environment)->HRESULT {
            auto s=weak.lock();
            if(!s||!s->life.IsCurrent(generation)) {creation->Finish(E_ABORT);return S_OK;}
            if(FAILED(result)||!environment) {creation->Finish(FAILED(result)?result:E_FAIL);return S_OK;}
            s->environment=environment;
            const auto controllerResult=environment->CreateCoreWebView2Controller(s->parent,
                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>([weak,generation,creation](HRESULT result,ICoreWebView2Controller* controller)->HRESULT {
                    auto s=weak.lock();
                    if(!s||!s->life.IsCurrent(generation)) {if(controller)controller->Close();creation->Finish(E_ABORT);return S_OK;}
                    if(FAILED(result)||!controller) {creation->Finish(FAILED(result)?result:E_FAIL);return S_OK;}
                    s->controller=controller;
                    result=controller->get_CoreWebView2(&s->view);
                    if(FAILED(result)) {creation->Finish(result);return S_OK;}
                    result=s->zoom.Attach(controller,s->view.get());
                    if(FAILED(result)) {creation->Finish(result);return S_OK;}
                    result=s->zoom.Configure(s->settings);
                    if(FAILED(result)) {creation->Finish(result);return S_OK;}
                    controller->put_Bounds(s->bounds);
                    controller->put_IsVisible(s->visible);
                    wil::com_ptr<ICoreWebView2Settings> settings;
                    if(SUCCEEDED(s->view->get_Settings(&settings))) settings->put_IsStatusBarEnabled(FALSE);
                    try {ConfigureEvents(s,generation);} catch(const winrt::hresult_error& error) {creation->Finish(error.code());return S_OK;}
                    const auto scriptResult=s->view->AddScriptToExecuteOnDocumentCreated(xiaochuang::MediaBridge::BootstrapScript(),
                        Callback<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>([creation](HRESULT result,PCWSTR)->HRESULT {creation->Finish(result);return S_OK;}).Get());
                    if(FAILED(scriptResult)) creation->Finish(scriptResult);
                    return S_OK;
                }).Get());
            if(FAILED(controllerResult)) creation->Finish(controllerResult);
            return S_OK;
        }).Get());
    if(FAILED(hr)) creation->Finish(hr);
    co_await winrt::resume_on_signal(creation->event.get());
    co_await wil::resume_foreground(state->queue);
    if(!state->life.IsCurrent(generation)) co_return;
    if(FAILED(creation->result)) {state->Report(BrowserError(creation->result));co_return;}
    state->ready=true;
    state->Report();
    if(!state->pendingUrl.empty()) {
        state->waiting=true;
        const auto navigate=state->view->Navigate(state->pendingUrl.c_str());
        if(FAILED(navigate)) {state->waiting=false;state->Report(BrowserError(navigate));}
    }
}
void BrowserHost::ConfigureEvents(const std::shared_ptr<State>& s,uint64_t generation) {
    std::weak_ptr<State> weak=s;
    auto current=[weak,generation](){auto p=weak.lock();return p&&p->life.IsCurrent(generation)?p:std::shared_ptr<State>{};};
    s->events=true;
    winrt::check_hresult(s->view->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>([current](ICoreWebView2*,ICoreWebView2NavigationStartingEventArgs* args)->HRESULT {
        if(auto s=current()) {s->loading=true;s->waiting=true;if(args)args->get_NavigationId(&s->navigationId);if(s->cancelGesture)s->cancelGesture();if(s->signal)s->signal({PageSignal::Typing,false});s->Report();}return S_OK;
    }).Get(),&s->starting));
    winrt::check_hresult(s->view->add_ContentLoading(Callback<ICoreWebView2ContentLoadingEventHandler>([current](ICoreWebView2*,ICoreWebView2ContentLoadingEventArgs* args)->HRESULT {
        if(auto s=current()) {UINT64 id=0;if(args)args->get_NavigationId(&id);if(id==s->navigationId){s->waiting=false;s->controller->put_IsVisible(s->visible);}}return S_OK;
    }).Get(),&s->content));
    winrt::check_hresult(s->view->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>([current](ICoreWebView2*,ICoreWebView2NavigationCompletedEventArgs* args)->HRESULT {
        if(auto s=current()) {
            UINT64 id=0;BOOL ok=FALSE;if(args){args->get_NavigationId(&id);args->get_IsSuccess(&ok);}
            if(id!=s->navigationId)return S_OK;
            s->waiting=false;s->loading=false;s->controller->put_IsVisible(s->visible);s->UpdateDocument();s->Report(ok?L"":L"网页加载失败，请重试或更换网址。");
        }return S_OK;
    }).Get(),&s->completed));
    winrt::check_hresult(s->view->add_SourceChanged(Callback<ICoreWebView2SourceChangedEventHandler>([current](ICoreWebView2*,ICoreWebView2SourceChangedEventArgs*)->HRESULT {if(auto s=current())s->UpdateDocument();return S_OK;}).Get(),&s->source));
    winrt::check_hresult(s->view->add_DocumentTitleChanged(Callback<ICoreWebView2DocumentTitleChangedEventHandler>([current](ICoreWebView2*,IUnknown*)->HRESULT {if(auto s=current())s->UpdateDocument();return S_OK;}).Get(),&s->title));
    winrt::check_hresult(s->view->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>([current](ICoreWebView2*,ICoreWebView2WebMessageReceivedEventArgs* args)->HRESULT {
        if(auto s=current()) {wil::unique_cotaskmem_string text;if(args&&SUCCEEDED(args->TryGetWebMessageAsString(&text))&&text&&s->signal){if(auto value=ParsePageSignal(text.get()))s->signal(*value);}}return S_OK;
    }).Get(),&s->message));
    winrt::check_hresult(s->view->add_ContainsFullScreenElementChanged(Callback<ICoreWebView2ContainsFullScreenElementChangedEventHandler>([current,generation,weak](ICoreWebView2*,IUnknown*)->HRESULT {
        if(auto s=current()) {
            BOOL full=FALSE;s->view->get_ContainsFullScreenElement(&full);if(s->signal)s->signal({PageSignal::Fullscreen,full!=FALSE});
            if(full) s->view->ExecuteScript(L"(()=>{let e=document.fullscreenElement;let v=e&&(e.matches('video')?e:e.querySelector('video'));return v&&v.videoHeight?v.videoWidth/v.videoHeight:0})()",
                Callback<ICoreWebView2ExecuteScriptCompletedHandler>([weak,generation](HRESULT hr,PCWSTR value)->HRESULT {
                    if(auto s=weak.lock();s&&s->life.IsCurrent(generation)&&SUCCEEDED(hr)&&value&&s->signal) {if(auto signal=ParsePageSignal(L"{\"kind\":\"aspect\",\"aspect\":"+std::wstring(value)+L"}"))s->signal(*signal);}return S_OK;
                }).Get());
        }return S_OK;
    }).Get(),&s->fullscreen));
    winrt::check_hresult(s->view->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>([current](ICoreWebView2*,ICoreWebView2NewWindowRequestedEventArgs* args)->HRESULT {
        if(auto s=current();s&&args) {args->put_Handled(TRUE);wil::unique_cotaskmem_string url;if(SUCCEEDED(args->get_Uri(&url))&&url&&Supported(url.get())&&s->newTab){auto queue=s->queue;auto action=s->newTab;auto address=std::wstring(url.get());queue.TryEnqueue([current,action,address]{if(current())action(address);});}}return S_OK;
    }).Get(),&s->newWindow));
    winrt::check_hresult(s->view->add_WindowCloseRequested(Callback<ICoreWebView2WindowCloseRequestedEventHandler>([current](ICoreWebView2*,IUnknown*)->HRESULT {if(auto s=current();s&&s->closeTab){auto action=s->closeTab;s->queue.TryEnqueue([current,action]{if(current())action();});}return S_OK;}).Get(),&s->closeWindow));
    winrt::check_hresult(s->view->add_ProcessFailed(Callback<ICoreWebView2ProcessFailedEventHandler>([current](ICoreWebView2*,ICoreWebView2ProcessFailedEventArgs*)->HRESULT {
        if(auto s=current()) {s->ready=false;s->loading=false;if(s->cancelGesture)s->cancelGesture();s->Report(L"浏览器进程异常。请恢复普通模式后重试。");}return S_OK;
    }).Get(),&s->failed));
    s->events=true;
}
OperationResult BrowserHost::Navigate(const std::wstring& url) {
    if(!Supported(url))return {false,L"仅支持 http、https、本地文件和 about:blank。"};
    auto s=state_;s->pendingUrl=url;
    if(!s->ready)return {true,{}};
    if(s->cancelGesture)s->cancelGesture();
    s->waiting=true;s->navigationId=0;s->controller->put_IsVisible(FALSE);
    const auto result=s->view->Navigate(url.c_str());
    if(FAILED(result)){s->waiting=false;s->controller->put_IsVisible(s->visible);return {false,BrowserError(result)};}
    return {};
}
void BrowserHost::SetVisible(bool visible) {
    auto s=state_;s->visible=visible||s->settings.backgroundMediaHotkeys;
    if(!visible&&s->cancelGesture)s->cancelGesture();
    if(!visible&&s->settings.autoPauseOnHide&&s->view) s->view->ExecuteScript(L"(()=>{const v=document.querySelector('video');if(v&&!v.paused)v.pause()})()",nullptr);
    if(s->controller)s->controller->put_IsVisible(s->visible&&!s->waiting);
}
void BrowserHost::ExecuteMedia(MediaCommand command,int rate) {if(state_->ready)state_->view->ExecuteScript(xiaochuang::MediaBridge::CommandScript(command,rate).c_str(),nullptr);}
void BrowserHost::Close() {
    auto s=state_;s->life.Close();
    if(s->cancelGesture)s->cancelGesture();
    s->ready=false;
    s->status={};s->signal={};s->document={};s->cancelGesture={};s->newTab={};s->closeTab={};
    if(s->creation&&s->creation->result==E_PENDING)s->creation->Finish(E_ABORT);
    s->zoom.Detach();
    if(s->view&&s->events) {
        s->view->remove_NavigationStarting(s->starting);s->view->remove_ContentLoading(s->content);s->view->remove_NavigationCompleted(s->completed);
        s->view->remove_SourceChanged(s->source);s->view->remove_DocumentTitleChanged(s->title);s->view->remove_WebMessageReceived(s->message);
        s->view->remove_ContainsFullScreenElementChanged(s->fullscreen);s->view->remove_NewWindowRequested(s->newWindow);
        s->view->remove_WindowCloseRequested(s->closeWindow);s->view->remove_ProcessFailed(s->failed);
    }
    s->events=false;
    if(s->controller)s->controller->Close();
    s->view.reset();s->controller.reset();s->environment.reset();
}
void BrowserHost::SetFixedZoom(bool enabled,int percent) {state_->settings.fixedWebZoomEnabled=enabled;state_->settings.fixedWebZoomPercent=percent;if(state_->controller){auto result=state_->zoom.Configure(state_->settings);if(FAILED(result))state_->Report(BrowserError(result));}}
void BrowserHost::Configure(const AppSettings& settings) {state_->settings=settings;SetFixedZoom(settings.fixedWebZoomEnabled,settings.fixedWebZoomPercent);}
void BrowserHost::SetBounds(RECT bounds) {state_->bounds=bounds;if(state_->controller)state_->controller->put_Bounds(bounds);}
void BrowserHost::NotifyParentMoved() {if(state_->controller)state_->controller->NotifyParentWindowPositionChanged();}
void BrowserHost::SetPageSignalHandler(std::function<void(PageSignal)> handler) {state_->signal=std::move(handler);}
void BrowserHost::SetDocumentHandler(std::function<void(std::wstring,std::wstring)> handler) {state_->document=std::move(handler);}
void BrowserHost::SetNavigationHandlers(std::function<void()> cancel,std::function<void(std::wstring)> newTab,std::function<void()> closeTab) {state_->cancelGesture=std::move(cancel);state_->newTab=std::move(newTab);state_->closeTab=std::move(closeTab);}
void BrowserHost::GoBack() {if(CanGoBack()){if(state_->cancelGesture)state_->cancelGesture();state_->view->GoBack();}}
bool BrowserHost::CanGoBack() const {BOOL can=FALSE;if(state_->view)state_->view->get_CanGoBack(&can);return can!=FALSE;}
void BrowserHost::RefreshOrStop() {if(state_->view){if(state_->loading)state_->view->Stop();else state_->view->Reload();}}
void BrowserHost::RequestDiagnostics(std::function<void(std::wstring)> completion) {
    auto s=state_;if(!s->ready){completion(L"浏览器尚未就绪。");return;}
    const auto generation=s->generation;
    s->view->ExecuteScript(xiaochuang::MediaBridge::DiagnosticScript().c_str(),Callback<ICoreWebView2ExecuteScriptCompletedHandler>([weak=std::weak_ptr<State>(s),completion,generation](HRESULT hr,PCWSTR value)->HRESULT {if(auto s=weak.lock();s&&s->life.IsCurrent(generation)&&s->ready)completion(SUCCEEDED(hr)&&value?value:L"媒体诊断失败。");return S_OK;}).Get());
}
}
