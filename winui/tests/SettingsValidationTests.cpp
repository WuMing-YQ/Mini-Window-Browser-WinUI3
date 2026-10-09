#include "Test.h"
#include "../core/AppCoordinator.h"
namespace {
struct Browser:miniwindow::IBrowserPort {miniwindow::OperationResult Navigate(const std::wstring&)override{return {};}void SetVisible(bool)override{}void ExecuteMedia(miniwindow::MediaCommand,int)override{}void Close()override{};void RequestDiagnostics(std::function<void(std::wstring)> callback)override{callback(L"fixture media diagnostic");}std::wstring RuntimeVersion()const override{return L"fixture runtime";}};
struct Window:miniwindow::IWindowPort {miniwindow::OperationResult SetMode(miniwindow::WindowMode)override{return {};}miniwindow::OperationResult SetHidden(bool)override{return {};}miniwindow::OperationResult RestoreNormal()override{return {};}RECT NormalRect()const override{return {100,100,1100,700};}miniwindow::OperationResult ApplyGeometry(RECT,bool)override{return {};}};
struct Store:miniwindow::ISettingsPort {bool fail=false;miniwindow::OperationResult Save(const miniwindow::AppState&)override{return {!fail,L"disk failure"};}};
}
void SettingsValidationTests(){
 using namespace miniwindow;Browser browser;Window window;Store store;AppState state;AppCoordinator app(browser,window,store,state);
 auto draft=state.settings;draft.fixedWebZoomPercent=25;CHECK(app.UpdateSettings(draft).ok);draft.fixedWebZoomPercent=500;CHECK(app.UpdateSettings(draft).ok);
 draft.fixedWebZoomPercent=24;CHECK(!app.UpdateSettings(draft).ok);CHECK(state.settings.fixedWebZoomPercent==500);draft.fixedWebZoomPercent=501;CHECK(!app.UpdateSettings(draft).ok);
 draft=state.settings;draft.fixedWebZoomEnabled=false;draft.fixedWebZoomPercent=0;CHECK(!app.UpdateSettings(draft).ok);CHECK(state.settings.fixedWebZoomPercent==500);
 draft=state.settings;draft.homeUrl=L" \t ";CHECK(!app.UpdateSettings(draft).ok);CHECK(!state.settings.homeUrl.empty());
 draft=state.settings;draft.homeUrl=L" example.org ";CHECK(app.UpdateSettings(draft).ok);CHECK(state.settings.homeUrl==L"https://example.org");
 draft=state.settings;draft.holdPlaybackRate=1;CHECK(!app.UpdateSettings(draft).ok);draft=state.settings;draft.holeRadius=4001;CHECK(!app.UpdateSettings(draft).ok);
 draft=state.settings;draft.windowWheelModifiers=0;CHECK(!app.UpdateSettings(draft).ok);draft=state.settings;draft.snapThreshold=-1;CHECK(!app.UpdateSettings(draft).ok);
 draft=state.settings;draft.renderMode=xiaochuang::RenderMode::SoftwareCompatibility;CHECK(app.UpdateSettings(draft).ok);CHECK(app.Snapshot().pendingRestart);
 draft.renderMode=xiaochuang::RenderMode::AutomaticGpu;CHECK(app.UpdateSettings(draft).ok);CHECK(!app.Snapshot().pendingRestart);
 store.fail=true;draft=state.settings;draft.holdPlaybackRate=5;CHECK(!app.UpdateSettings(draft).ok);CHECK(state.settings.holdPlaybackRate==3);store.fail=false;
 bool runtimeFail=true;app.SetRuntimeHandler([&]{return OperationResult{!runtimeFail,L"native failure"};});draft=state.settings;draft.themeMode=xiaochuang::ThemeMode::Dark;CHECK(!app.UpdateSettings(draft).ok);CHECK(state.settings.themeMode==xiaochuang::ThemeMode::System);runtimeFail=false;
 auto old=state.hotkeys[1];auto binding=old;binding.virtualKey=state.hotkeys[0].virtualKey;CHECK(!app.UpdateHotkey(HotkeyAction::PlayPause,binding).ok);CHECK(state.hotkeys[1].virtualKey==old.virtualKey);
 binding=old;binding.virtualKey=VK_F10;binding.modifiers=MOD_CONTROL;app.SetHotkeyValidator([](auto,const auto&){return OperationResult{false,L"OS conflict"};});CHECK(!app.UpdateHotkey(HotkeyAction::PlayPause,binding).ok);CHECK(state.hotkeys[1].virtualKey==old.virtualKey);
 app.SetHotkeyValidator({});CHECK(app.UpdateHotkey(HotkeyAction::PlayPause,binding).ok);CHECK(state.hotkeys[1].virtualKey==VK_F10);binding.enabled=false;CHECK(app.UpdateHotkey(HotkeyAction::PlayPause,binding).ok);CHECK(!state.hotkeys[1].enabled);
 CHECK(app.SaveBookmark({}, {L"保留",L"example.org"}).ok);app.AddTab(L"other.example");const auto count=state.tabs.size();CHECK(app.ResetDefaults().ok);CHECK(state.bookmarks.size()==1);CHECK(state.tabs.size()==count);CHECK(state.settings.fixedWebZoomPercent==100);CHECK(state.hotkeys[1].enabled);
 std::wstring diagnostics;app.RequestDiagnostics([&](auto value){diagnostics=std::move(value);});CHECK(diagnostics.find(L"Windows:")!=std::wstring::npos);CHECK(diagnostics.find(L"fixture runtime")!=std::wstring::npos);CHECK(diagnostics.find(L"fixture media diagnostic")!=std::wstring::npos);
}
