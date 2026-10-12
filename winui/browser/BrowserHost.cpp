#include <unknwn.h>
#include "BrowserHost.h"
#include "WebLifecycle.h"
#include "VideoFullscreenScript.h"
#include "../../src/WebViewZoom.h"
#include "../../src/ConfigStore.h"
#include <winrt/Microsoft.UI.Dispatching.h>
#include <wil/cppwinrt_helpers.h>
#include <WebView2EnvironmentOptions.h>
#include <wrl.h>
#include <vector>
#include <dcomp.h>
#include <windowsx.h>
#pragma comment(lib,"dcomp.lib")
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
    HWND surface=nullptr;
    UINT nativeButtons=0;POINT nativePoint{};
    std::filesystem::path profile;
    winrt::Microsoft::UI::Dispatching::DispatcherQueue queue{nullptr};
    WebLifecycle life;
    wil::com_ptr<ICoreWebView2Environment> environment;
    wil::com_ptr<ICoreWebView2Controller> controller;
    wil::com_ptr<ICoreWebView2CompositionController> composition;
    wil::com_ptr<IDCompositionDevice> device;
    wil::com_ptr<IDCompositionTarget> target;
    wil::com_ptr<IDCompositionVisual> visual;
    wil::com_ptr<ICoreWebView2> view;
    xiaochuang::WebViewZoom zoom;
    AppSettings settings;
    RECT bounds{};
    bool ready=false,loading=false,visible=true,occluded=false,waiting=false,events=false;
    UINT64 navigationId=0;
    uint64_t generation=0;
    std::wstring pendingUrl;
    std::shared_ptr<Creation> creation;
    std::function<void(bool,bool,std::wstring)> status;
    std::function<void(PageSignal)> signal;
    std::function<void(bool)> focus;
    std::function<void(std::wstring,std::wstring)> document;
    std::function<void()> cancelGesture,closeTab;
    std::function<void(std::wstring)> newTab;
    EventRegistrationToken starting{},content{},completed{},source{},title{},message{},newWindow{},closeWindow{},failed{},gotFocus{},lostFocus{};
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
    // Native hit testing owns the visible browser surface. Route real HWND
    // mouse messages to WebView, including RDP, instead of expecting them to
    // fall through the child airspace into the covered XAML WebSlot.
    if(!state->surface) {
        WNDCLASSW windowClass{};windowClass.lpfnWndProc=SurfaceProcedure;
        windowClass.style=CS_DBLCLKS;windowClass.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        windowClass.hInstance=GetModuleHandleW(nullptr);windowClass.lpszClassName=L"MiniWindow.BrowserCompositionSurface";
        if(!RegisterClassW(&windowClass)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS){state->Report(BrowserError(HRESULT_FROM_WIN32(GetLastError())));co_return;}
        state->surface=CreateWindowExW(0,windowClass.lpszClassName,L"",WS_CHILD|(state->occluded?0:WS_VISIBLE)|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
            state->bounds.left,state->bounds.top,state->bounds.right-state->bounds.left,state->bounds.bottom-state->bounds.top,state->parent,nullptr,windowClass.hInstance,state.get());
        if(!state->surface){state->Report(BrowserError(HRESULT_FROM_WIN32(GetLastError())));co_return;}
        SetWindowPos(state->surface,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    }
    winrt::check_hresult(DCompositionCreateDevice(nullptr,IID_PPV_ARGS(state->device.put())));
    winrt::check_hresult(state->device->CreateTargetForHwnd(state->surface,TRUE,&state->target));
    winrt::check_hresult(state->device->CreateVisual(&state->visual));
    winrt::check_hresult(state->target->SetRoot(state->visual.get()));
    winrt::check_hresult(state->device->Commit());
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
            auto compositionEnvironment=s->environment.try_query<ICoreWebView2Environment3>();
            if(!compositionEnvironment){creation->Finish(E_NOINTERFACE);return S_OK;}
            const auto controllerResult=compositionEnvironment->CreateCoreWebView2CompositionController(s->surface,
                Callback<ICoreWebView2CreateCoreWebView2CompositionControllerCompletedHandler>([weak,generation,creation](HRESULT result,ICoreWebView2CompositionController* composition)->HRESULT {
                    wil::com_ptr<ICoreWebView2Controller> controller;
                    if(composition)composition->QueryInterface(IID_PPV_ARGS(controller.put()));
                    auto s=weak.lock();
                    if(!s||!s->life.IsCurrent(generation)) {if(controller)controller->Close();creation->Finish(E_ABORT);return S_OK;}
                    if(FAILED(result)||!controller) {creation->Finish(FAILED(result)?result:E_FAIL);return S_OK;}
                    s->controller=controller;s->composition=composition;
                    result=composition->put_RootVisualTarget(s->visual.get());
                    if(SUCCEEDED(result))result=s->device->Commit();
                    if(FAILED(result)){creation->Finish(result);return S_OK;}
                    result=controller->get_CoreWebView2(&s->view);
                    if(FAILED(result)) {creation->Finish(result);return S_OK;}
                    result=s->zoom.Attach(controller.get(),s->view.get());
                    if(FAILED(result)) {creation->Finish(result);return S_OK;}
                    result=s->zoom.Configure(s->settings);
                    if(FAILED(result)) {creation->Finish(result);return S_OK;}
                    controller->put_Bounds({0,0,s->bounds.right-s->bounds.left,s->bounds.bottom-s->bounds.top});
                    controller->put_IsVisible(s->visible&&!s->occluded);
                    wil::com_ptr<ICoreWebView2Settings> settings;
                    if(SUCCEEDED(s->view->get_Settings(&settings))) settings->put_IsStatusBarEnabled(FALSE);
                    try {ConfigureEvents(s,generation);} catch(const winrt::hresult_error& error) {creation->Finish(error.code());return S_OK;}
                    const auto scriptResult=s->view->AddScriptToExecuteOnDocumentCreated((std::wstring(xiaochuang::MediaBridge::BootstrapScript())+VideoFullscreenScript).c_str(),
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
    winrt::check_hresult(s->controller->add_GotFocus(Callback<ICoreWebView2FocusChangedEventHandler>([current](ICoreWebView2Controller*,IUnknown*)->HRESULT{if(auto s=current();s&&s->focus)s->focus(true);return S_OK;}).Get(),&s->gotFocus));
    winrt::check_hresult(s->controller->add_LostFocus(Callback<ICoreWebView2FocusChangedEventHandler>([current](ICoreWebView2Controller*,IUnknown*)->HRESULT{if(auto s=current();s&&s->focus)s->focus(false);return S_OK;}).Get(),&s->lostFocus));
    winrt::check_hresult(s->view->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>([current](ICoreWebView2*,ICoreWebView2NavigationStartingEventArgs* args)->HRESULT {
        if(auto s=current()) {CancelNativeMouse(*s);if(s->signal)s->signal({PageSignal::Fullscreen,false});s->loading=true;s->waiting=true;if(args)args->get_NavigationId(&s->navigationId);if(s->cancelGesture)s->cancelGesture();s->Report();}return S_OK;
    }).Get(),&s->starting));
    winrt::check_hresult(s->view->add_ContentLoading(Callback<ICoreWebView2ContentLoadingEventHandler>([current](ICoreWebView2*,ICoreWebView2ContentLoadingEventArgs* args)->HRESULT {
        if(auto s=current()) {UINT64 id=0;if(args)args->get_NavigationId(&id);if(id==s->navigationId){s->waiting=false;s->controller->put_IsVisible(s->visible&&!s->occluded);}}return S_OK;
    }).Get(),&s->content));
    winrt::check_hresult(s->view->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>([current](ICoreWebView2*,ICoreWebView2NavigationCompletedEventArgs* args)->HRESULT {
        if(auto s=current()) {
            UINT64 id=0;if(args)args->get_NavigationId(&id);
            if(id!=s->navigationId)return S_OK;
            s->waiting=false;s->loading=false;s->controller->put_IsVisible(s->visible&&!s->occluded);s->UpdateDocument();s->Report();
        }return S_OK;
    }).Get(),&s->completed));
    winrt::check_hresult(s->view->add_SourceChanged(Callback<ICoreWebView2SourceChangedEventHandler>([current](ICoreWebView2*,ICoreWebView2SourceChangedEventArgs*)->HRESULT {if(auto s=current())s->UpdateDocument();return S_OK;}).Get(),&s->source));
    winrt::check_hresult(s->view->add_DocumentTitleChanged(Callback<ICoreWebView2DocumentTitleChangedEventHandler>([current](ICoreWebView2*,IUnknown*)->HRESULT {if(auto s=current())s->UpdateDocument();return S_OK;}).Get(),&s->title));
    winrt::check_hresult(s->view->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>([current](ICoreWebView2*,ICoreWebView2WebMessageReceivedEventArgs* args)->HRESULT {
        if(auto s=current()) {wil::unique_cotaskmem_string text;if(args&&SUCCEEDED(args->TryGetWebMessageAsString(&text))&&text&&s->signal){if(auto value=ParsePageSignal(text.get()))s->signal(*value);}}return S_OK;
    }).Get(),&s->message));
    winrt::check_hresult(s->view->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>([current](ICoreWebView2*,ICoreWebView2NewWindowRequestedEventArgs* args)->HRESULT {
        if(auto s=current();s&&args) {args->put_Handled(TRUE);wil::unique_cotaskmem_string url;if(SUCCEEDED(args->get_Uri(&url))&&url&&Supported(url.get())&&s->newTab){auto queue=s->queue;auto action=s->newTab;auto address=std::wstring(url.get());queue.TryEnqueue([current,action,address]{if(current())action(address);});}}return S_OK;
    }).Get(),&s->newWindow));
    winrt::check_hresult(s->view->add_WindowCloseRequested(Callback<ICoreWebView2WindowCloseRequestedEventHandler>([current](ICoreWebView2*,IUnknown*)->HRESULT {if(auto s=current();s&&s->closeTab){auto action=s->closeTab;s->queue.TryEnqueue([current,action]{if(current())action();});}return S_OK;}).Get(),&s->closeWindow));
    winrt::check_hresult(s->view->add_ProcessFailed(Callback<ICoreWebView2ProcessFailedEventHandler>([current](ICoreWebView2*,ICoreWebView2ProcessFailedEventArgs*)->HRESULT {
        if(auto s=current()) {CancelNativeMouse(*s);s->ready=false;s->loading=false;if(s->cancelGesture)s->cancelGesture();s->Report(L"浏览器进程异常。请恢复普通模式后重试。");}return S_OK;
    }).Get(),&s->failed));
    s->events=true;
}
OperationResult BrowserHost::Navigate(const std::wstring& url) {
    if(!Supported(url))return {false,L"仅支持 http、https、本地文件和 about:blank。"};
    auto s=state_;s->pendingUrl=url;
    if(!s->ready)return {true,{}};
    if(s->cancelGesture)s->cancelGesture();
    CancelNativeMouse(*s);s->waiting=true;s->navigationId=0;s->controller->put_IsVisible(FALSE);
    const auto result=s->view->Navigate(url.c_str());
    if(FAILED(result)){s->waiting=false;s->controller->put_IsVisible(s->visible&&!s->occluded);return {false,BrowserError(result)};}
    return {};
}
void BrowserHost::SetVisible(bool visible) {
    auto s=state_;if(!visible)CancelNativeMouse(*s);s->visible=visible||s->settings.backgroundMediaHotkeys;
    if(!visible&&s->cancelGesture)s->cancelGesture();
    if(!visible&&s->settings.autoPauseOnHide&&s->view) s->view->ExecuteScript(L"(()=>{const v=document.querySelector('video');if(v&&!v.paused)v.pause()})()",nullptr);
    if(s->controller)s->controller->put_IsVisible(s->visible&&!s->occluded&&!s->waiting);
}
void BrowserHost::ExecuteMedia(MediaCommand command,int rate) {if(state_->ready)state_->view->ExecuteScript(xiaochuang::MediaBridge::CommandScript(command,rate).c_str(),nullptr);}
void BrowserHost::SetOccluded(bool occluded){auto s=state_;if(occluded)CancelNativeMouse(*s);s->occluded=occluded;if(s->surface)ShowWindow(s->surface,occluded?SW_HIDE:SW_SHOWNA);if(s->controller)s->controller->put_IsVisible(s->visible&&!occluded&&!s->waiting);}
void BrowserHost::Close() {
    auto s=state_;CancelNativeMouse(*s);s->life.Close();
    if(s->cancelGesture)s->cancelGesture();
    s->ready=false;
    s->status={};s->signal={};s->document={};s->focus={};s->cancelGesture={};s->newTab={};s->closeTab={};
    if(s->creation&&s->creation->result==E_PENDING)s->creation->Finish(E_ABORT);
    s->zoom.Detach();
    if(s->view&&s->events) {
        s->view->remove_NavigationStarting(s->starting);s->view->remove_ContentLoading(s->content);s->view->remove_NavigationCompleted(s->completed);
        s->view->remove_SourceChanged(s->source);s->view->remove_DocumentTitleChanged(s->title);s->view->remove_WebMessageReceived(s->message);
        s->view->remove_NewWindowRequested(s->newWindow);
        s->view->remove_WindowCloseRequested(s->closeWindow);s->view->remove_ProcessFailed(s->failed);
    }
    if(s->controller&&s->events){s->controller->remove_GotFocus(s->gotFocus);s->controller->remove_LostFocus(s->lostFocus);}
    s->events=false;
    if(s->composition)s->composition->put_RootVisualTarget(nullptr);
    if(s->controller)s->controller->Close();
    s->view.reset();s->controller.reset();s->composition.reset();s->environment.reset();
    s->visual.reset();s->target.reset();s->device.reset();
    if(s->surface){DestroyWindow(s->surface);s->surface=nullptr;}
}
OperationResult BrowserHost::SetFixedZoom(bool enabled,int percent){auto next=state_->settings;next.fixedWebZoomEnabled=enabled;next.fixedWebZoomPercent=percent;return Configure(next);}
OperationResult BrowserHost::Configure(const AppSettings& settings){
 if(state_->controller){const auto result=state_->zoom.Configure(settings);if(FAILED(result)){state_->zoom.Configure(state_->settings);return {false,BrowserError(result)};}}
 state_->settings=settings;return {};
}
void BrowserHost::SetBounds(RECT bounds) {
    state_->bounds=bounds;
    if(state_->surface)SetWindowPos(state_->surface,HWND_TOP,bounds.left,bounds.top,
        std::max(0L,bounds.right-bounds.left),std::max(0L,bounds.bottom-bounds.top),SWP_NOACTIVATE);
    if(state_->controller)state_->controller->put_Bounds({0,0,std::max(0L,bounds.right-bounds.left),std::max(0L,bounds.bottom-bounds.top)});
}
void BrowserHost::NotifyParentMoved() {if(state_->controller)state_->controller->NotifyParentWindowPositionChanged();}
void BrowserHost::SetPageSignalHandler(std::function<void(PageSignal)> handler) {state_->signal=std::move(handler);}
void BrowserHost::SetDocumentHandler(std::function<void(std::wstring,std::wstring)> handler) {state_->document=std::move(handler);}
void BrowserHost::SetNavigationHandlers(std::function<void()> cancel,std::function<void(std::wstring)> newTab,std::function<void()> closeTab) {state_->cancelGesture=std::move(cancel);state_->newTab=std::move(newTab);state_->closeTab=std::move(closeTab);}
void BrowserHost::SetFocusHandler(std::function<void(bool)> handler){state_->focus=std::move(handler);}
void BrowserHost::Focus(){if(state_->controller&&state_->visible&&!state_->occluded)state_->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);}
bool BrowserHost::SendMouseInput(UINT message,UINT keys,UINT data,POINT point){return SendMouse(*state_,message,keys,data,point);}
bool BrowserHost::SendMouse(State& state,UINT message,UINT keys,UINT data,POINT point){
    auto* s=&state;
    if(!s->composition||!s->ready||!s->visible||s->occluded||s->waiting)return false;
    if(message==WM_MOUSELEAVE){keys=0;data=0;point={};}
    const auto result=s->composition->SendMouseInput(static_cast<COREWEBVIEW2_MOUSE_EVENT_KIND>(message),static_cast<COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS>(keys),data,point);
    return SUCCEEDED(result);
}
void BrowserHost::CancelMouseInput(){CancelNativeMouse(*state_);}
void BrowserHost::CancelNativeMouse(State& s){
    // Clear each forwarded button before sending up; ReleaseCapture re-enters
    // this procedure synchronously via WM_CAPTURECHANGED.
    for(auto pair:{std::pair<UINT,UINT>{MK_LBUTTON,WM_LBUTTONUP},{MK_RBUTTON,WM_RBUTTONUP},{MK_MBUTTON,WM_MBUTTONUP},{MK_XBUTTON1,WM_XBUTTONUP},{MK_XBUTTON2,WM_XBUTTONUP}}){
        if(s.nativeButtons&pair.first){s.nativeButtons&=~pair.first;SendMouse(s,pair.second,s.nativeButtons,pair.first==MK_XBUTTON1?XBUTTON1:pair.first==MK_XBUTTON2?XBUTTON2:0,s.nativePoint);}
    }
    if(s.surface&&GetCapture()==s.surface)ReleaseCapture();
    SendMouse(s,WM_MOUSELEAVE,0,0,{});
}
bool BrowserHost::HandleParentWheel(UINT message,WPARAM w,LPARAM l){
    auto s=state_;if(!s->surface||(message!=WM_MOUSEWHEEL&&message!=WM_MOUSEHWHEEL))return false;
    RECT rect{};POINT point{GET_X_LPARAM(l),GET_Y_LPARAM(l)};
    if(!GetWindowRect(s->surface,&rect)||!PtInRect(&rect,point)||!IsWindowVisible(s->surface))return false;
    return HandleSurfaceMouse(*s,message,w,l);
}
bool BrowserHost::HandleSurfaceMouse(State& s,UINT message,WPARAM w,LPARAM l){
    const bool wheel=message==WM_MOUSEWHEEL||message==WM_MOUSEHWHEEL;
    const bool down=message==WM_LBUTTONDOWN||message==WM_RBUTTONDOWN||message==WM_MBUTTONDOWN||message==WM_XBUTTONDOWN||message==WM_LBUTTONDBLCLK||message==WM_RBUTTONDBLCLK||message==WM_MBUTTONDBLCLK||message==WM_XBUTTONDBLCLK;
    const bool up=message==WM_LBUTTONUP||message==WM_RBUTTONUP||message==WM_MBUTTONUP||message==WM_XBUTTONUP;
    if(!wheel&&!down&&!up&&message!=WM_MOUSEMOVE&&message!=WM_MOUSELEAVE)return false;
    if(!IsWindowVisible(s.parent)||(GetWindowLongPtrW(s.parent,GWL_EXSTYLE)&(WS_EX_TRANSPARENT|WS_EX_NOACTIVATE))){CancelNativeMouse(s);return false;}
    if(message==WM_MOUSELEAVE&&GetCapture()==s.surface&&s.nativeButtons)return true;
    POINT point{GET_X_LPARAM(l),GET_Y_LPARAM(l)};
    if(wheel)ScreenToClient(s.surface,&point);
    UINT data=wheel?static_cast<UINT>(GET_WHEEL_DELTA_WPARAM(w)):(message==WM_XBUTTONDOWN||message==WM_XBUTTONUP||message==WM_XBUTTONDBLCLK?GET_XBUTTON_WPARAM(w):0);

    if(down&&s.controller){SetCapture(s.surface);s.controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);}
    if(message==WM_MOUSEMOVE){TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,s.surface,0};TrackMouseEvent(&track);}
    const UINT keys=GET_KEYSTATE_WPARAM(w);
    const bool sent=SendMouse(s,message,keys,data,point);
    if(sent&&message!=WM_MOUSELEAVE){s.nativePoint=point;s.nativeButtons=keys&(MK_LBUTTON|MK_RBUTTON|MK_MBUTTON|MK_XBUTTON1|MK_XBUTTON2);}
    if(up&&!(keys&(MK_LBUTTON|MK_RBUTTON|MK_MBUTTON|MK_XBUTTON1|MK_XBUTTON2))&&GetCapture()==s.surface){
        ReleaseCapture();
        RECT client{};if(GetClientRect(s.surface,&client)&&!PtInRect(&client,point))SendMouse(s,WM_MOUSELEAVE,0,0,{});
    }
    // Own this input even while a navigation is pending; do not bubble a wheel
    // back to the parent and deliver the same user action a second time.
    return true;
}
LRESULT CALLBACK BrowserHost::SurfaceProcedure(HWND hwnd,UINT message,WPARAM w,LPARAM l){
    auto* s=reinterpret_cast<State*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(message==WM_NCCREATE){s=static_cast<State*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(s));}
    if(message==WM_NCDESTROY){SetWindowLongPtrW(hwnd,GWLP_USERDATA,0);return DefWindowProcW(hwnd,message,w,l);}
    if(s)try{
        if(message==WM_NCHITTEST){return GetWindowLongPtrW(s->parent,GWL_EXSTYLE)&WS_EX_TRANSPARENT?HTTRANSPARENT:HTCLIENT;}
        if(message==WM_SETCURSOR&&LOWORD(l)==HTCLIENT){HCURSOR cursor=nullptr;if(s->composition)s->composition->get_Cursor(&cursor);SetCursor(cursor?cursor:LoadCursorW(nullptr,IDC_ARROW));return TRUE;}
        if(message==WM_CANCELMODE||(message==WM_CAPTURECHANGED&&s->nativeButtons))CancelNativeMouse(*s);
        if(HandleSurfaceMouse(*s,message,w,l))return message==WM_XBUTTONDOWN||message==WM_XBUTTONUP||message==WM_XBUTTONDBLCLK?TRUE:0;
    }catch(...){/* Exceptions must not cross the native window procedure. */}
    return DefWindowProcW(hwnd,message,w,l);
}
void BrowserHost::GoBack() {if(CanGoBack()){if(state_->cancelGesture)state_->cancelGesture();state_->view->GoBack();}}
bool BrowserHost::CanGoBack() const {BOOL can=FALSE;if(state_->view)state_->view->get_CanGoBack(&can);return can!=FALSE;}
void BrowserHost::RefreshOrStop() {if(state_->view){if(state_->loading)state_->view->Stop();else state_->view->Reload();}}
void BrowserHost::RequestDiagnostics(std::function<void(std::wstring)> completion) {
    auto s=state_;if(!s->ready){completion(L"浏览器尚未就绪。");return;}
    const auto generation=s->generation;
    s->view->ExecuteScript(xiaochuang::MediaBridge::DiagnosticScript().c_str(),Callback<ICoreWebView2ExecuteScriptCompletedHandler>([weak=std::weak_ptr<State>(s),completion,generation](HRESULT hr,PCWSTR value)->HRESULT {if(auto s=weak.lock();s&&s->life.IsCurrent(generation)&&s->ready)completion(SUCCEEDED(hr)&&value?value:L"媒体诊断失败。");return S_OK;}).Get());
}
std::wstring BrowserHost::RuntimeVersion()const{wil::unique_cotaskmem_string version;if(state_->environment&&SUCCEEDED(state_->environment->get_BrowserVersionString(&version))&&version)return version.get();return {};}
}
