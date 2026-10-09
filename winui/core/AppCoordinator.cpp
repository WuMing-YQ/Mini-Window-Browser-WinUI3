#include "AppCoordinator.h"
#include "../../src/ConfigStore.h"
#include "../../src/CoreLogic.h"
#include "../../src/Diagnostics.h"
#include "../settings/SettingsPolicy.h"
namespace miniwindow {
AppCoordinator::AppCoordinator(IBrowserPort& browser,IWindowPort& window,ISettingsPort& settings)
    :browser_(browser),window_(window),settings_(settings),state_(snapshot_.state) {
    runningRender_=state_.settings.renderMode;SessionController(state_).CloseAll();
}
AppCoordinator::AppCoordinator(IBrowserPort& browser,IWindowPort& window,ISettingsPort& settings,AppState& sharedState)
    :browser_(browser),window_(window),settings_(settings),state_(sharedState){runningRender_=state_.settings.renderMode;if(state_.tabs.empty())SessionController(state_).CloseAll();snapshot_.state=state_;}
AppCoordinator::~AppCoordinator() {subscriber_={};}
void AppCoordinator::Subscribe(std::function<void(const AppSnapshot&)> subscriber) {
    subscriber_=std::move(subscriber);
    if(subscriber_) subscriber_(snapshot_);
}
void AppCoordinator::Publish(OperationResult result) {
    snapshot_.error=result.ok?L"":std::move(result.message);
    snapshot_.state=state_;snapshot_.pendingRestart=state_.settings.renderMode!=runningRender_;
    ++snapshot_.revision;
    auto subscriber=subscriber_;
    if(subscriber) subscriber(snapshot_);
}
void AppCoordinator::Navigate(std::wstring input) {
    const auto url=xiaochuang::NormalizeInputUrl(input);
    auto result=browser_.Navigate(url);
    if(result.ok) state_.tabs[static_cast<size_t>(state_.currentTabIndex)].url=url;
    Publish(std::move(result));
}
void AppCoordinator::ToggleImmersion() {
    auto& state=state_;
    if(state.immersionActive) {RestoreNormal();return;}
    const auto target=state.settings.immersionStyle==ImmersionStyle::Hole?WindowMode::ImmersionHole:WindowMode::ImmersionAutoHide;
    auto result=window_.SetMode(target);
    if(result.ok) {state.windowMode=target;state.immersionActive=true;state.windowHidden=false;browser_.SetVisible(true);}
    Publish(std::move(result));
}
void AppCoordinator::RestoreNormal() {
    auto result=window_.RestoreNormal();
    if(result.ok) {
        auto& state=state_;
        state.windowMode=WindowMode::Normal;state.immersionActive=false;state.windowHidden=false;
        state.session.normalRect=window_.NormalRect();state.session.maximized=false;
        browser_.SetVisible(true);
    }
    Publish(std::move(result));
}
void AppCoordinator::ToggleHidden() {
    auto& state=state_;
    const bool hidden=!state.windowHidden;
    auto result=window_.SetHidden(hidden);
    if(result.ok) {state.windowHidden=hidden;browser_.SetVisible(!hidden);}
    Publish(std::move(result));
}
void AppCoordinator::OnBrowserState(bool ready,bool loading,std::wstring error) {
    snapshot_.browserReady=ready;snapshot_.loading=loading;
    const bool ok=error.empty();Publish({ok,std::move(error)});
}
void AppCoordinator::OnWindowState(WindowMode mode,bool hidden,bool immersive){state_.windowMode=mode;state_.windowHidden=hidden;state_.immersionActive=immersive;state_.session.normalRect=window_.NormalRect();state_.session.maximized=mode==WindowMode::Maximized;Publish();}
void AppCoordinator::OnHotkeyErrors(std::vector<std::wstring> errors){snapshot_.hotkeyErrors=std::move(errors);Publish();}
void AppCoordinator::OnDocument(std::wstring url,std::wstring title){auto& tab=state_.tabs[static_cast<size_t>(state_.currentTabIndex)];tab.url=std::move(url);tab.title=std::move(title);Publish();}
void AppCoordinator::NavigateActive(){auto result=browser_.Navigate(state_.tabs[static_cast<size_t>(state_.currentTabIndex)].url);Publish(std::move(result));}
void AppCoordinator::AddTab(std::wstring url){SessionController(state_).AddTab(std::move(url));NavigateActive();}
void AppCoordinator::SelectTab(size_t index){if(index>=state_.tabs.size())return;SessionController(state_).SelectTab(index);NavigateActive();}
void AppCoordinator::CloseTab(size_t index){if(index>=state_.tabs.size())return;const bool active=static_cast<int>(index)==state_.currentTabIndex;SessionController(state_).CloseTab(index);if(active)NavigateActive();else Publish();}
void AppCoordinator::CloseOtherTabs(size_t index){if(index>=state_.tabs.size())return;SessionController(state_).CloseOthers(index);NavigateActive();}
void AppCoordinator::CloseAllTabs(){SessionController(state_).CloseAll();NavigateActive();}
void AppCoordinator::OpenBookmark(size_t index){if(index>=state_.bookmarks.size())return;Navigate(state_.bookmarks[index].url);}
OperationResult AppCoordinator::CommitCollection(AppState next){next.session.normalRect=window_.NormalRect();next.session.maximized=state_.windowMode==WindowMode::Maximized;auto result=settings_.Save(next);if(result.ok)state_=std::move(next);return Finish(result);}
OperationResult AppCoordinator::SaveBookmark(std::optional<size_t> index,Bookmark bookmark){
    bookmark.title=xiaochuang::TrimWhitespace(std::move(bookmark.title));bookmark.url=xiaochuang::TrimWhitespace(std::move(bookmark.url));
    if(bookmark.title.empty()||bookmark.url.empty())return Finish({false,L"请填写书签标题和网址。"});
    if(index&&*index>=state_.bookmarks.size())return Finish({false,L"书签已不存在。"});
    bookmark.url=xiaochuang::NormalizeInputUrl(bookmark.url);auto next=state_;if(index)next.bookmarks[*index]=std::move(bookmark);else next.bookmarks.push_back(std::move(bookmark));return CommitCollection(std::move(next));
}
OperationResult AppCoordinator::DeleteBookmark(size_t index){if(index>=state_.bookmarks.size())return Finish({false,L"书签已不存在。"});auto next=state_;next.bookmarks.erase(next.bookmarks.begin()+static_cast<ptrdiff_t>(index));return CommitCollection(std::move(next));}
OperationResult AppCoordinator::ValidatePresetName(std::wstring& name,std::optional<size_t> ignore){name=xiaochuang::TrimWhitespace(std::move(name));if(name.empty())return {false,L"预设名称不能为空。"};std::vector<std::wstring> names;for(const auto& preset:state_.presets)names.push_back(preset.name);if(xiaochuang::HasPresetNameConflict(names,name,ignore.value_or(static_cast<size_t>(-1))))return {false,L"已存在同名预设。"};return {};}
OperationResult AppCoordinator::SaveCurrentPreset(std::wstring name){
    auto valid=ValidatePresetName(name);if(!valid.ok)return Finish(valid);
    Preset preset;preset.name=std::move(name);preset.normalRect=window_.NormalRect();preset.maximized=state_.windowMode==WindowMode::Maximized;
    preset.hoverReductionPercent=state_.settings.hoverReductionPercent;
    preset.windowWheelEnabled=state_.settings.windowWheelEnabled;
    preset.windowWheelModifiers=state_.settings.windowWheelModifiers;
    preset.holeRadius=state_.settings.holeRadius;
    preset.snapThreshold=state_.settings.snapThreshold;
    preset.holeOpacityPercent=state_.settings.holeOpacityPercent;
    preset.autoHideOpacityPercent=state_.settings.autoHideOpacityPercent;
    preset.holdPlaybackRate=state_.settings.holdPlaybackRate;
    preset.fixedWebZoomPercent=state_.settings.fixedWebZoomPercent;
    preset.fixedWebZoomEnabled=state_.settings.fixedWebZoomEnabled;
    preset.autoPauseOnHide=state_.settings.autoPauseOnHide;
    preset.disableHotkeysOnTyping=state_.settings.disableHotkeysOnTyping;
    preset.useSystemTray=state_.settings.useSystemTray;
    preset.backgroundMediaHotkeys=state_.settings.backgroundMediaHotkeys;
    preset.autoFitVideoFullscreen=state_.settings.autoFitVideoFullscreen;
    preset.lockVideoFullscreenAspect=state_.settings.lockVideoFullscreenAspect;
    preset.maximizedTopDragEnabled=state_.settings.maximizedTopDragEnabled;
    preset.immersionStyle=state_.settings.immersionStyle;
    preset.homeUrl=state_.settings.homeUrl;
    preset.tabs=state_.tabs;preset.activeTab=state_.currentTabIndex;preset.currentUrl=state_.tabs[static_cast<size_t>(state_.currentTabIndex)].url;preset.hotkeys=state_.hotkeys;auto next=state_;next.presets.push_back(std::move(preset));return CommitCollection(std::move(next));
}
OperationResult AppCoordinator::RenamePreset(size_t index,std::wstring name){if(index>=state_.presets.size())return Finish({false,L"预设已不存在。"});auto valid=ValidatePresetName(name,index);if(!valid.ok)return Finish(valid);auto next=state_;next.presets[index].name=std::move(name);return CommitCollection(std::move(next));}
OperationResult AppCoordinator::DeletePreset(size_t index){if(index>=state_.presets.size())return Finish({false,L"预设已不存在。"});auto next=state_;next.presets.erase(next.presets.begin()+static_cast<ptrdiff_t>(index));return CommitCollection(std::move(next));}
OperationResult AppCoordinator::ApplyPreset(size_t index){
    if(index>=state_.presets.size())return Finish({false,L"预设已不存在。"});
    const auto previous=state_;const auto preset=state_.presets[index];
    state_.settings.hoverReductionPercent=preset.hoverReductionPercent;
    state_.settings.windowWheelEnabled=preset.windowWheelEnabled;
    state_.settings.windowWheelModifiers=preset.windowWheelModifiers;
    state_.settings.holeRadius=preset.holeRadius;
    state_.settings.snapThreshold=preset.snapThreshold;
    state_.settings.holeOpacityPercent=preset.holeOpacityPercent;
    state_.settings.autoHideOpacityPercent=preset.autoHideOpacityPercent;
    state_.settings.holdPlaybackRate=preset.holdPlaybackRate;
    state_.settings.fixedWebZoomPercent=preset.fixedWebZoomPercent;
    state_.settings.fixedWebZoomEnabled=preset.fixedWebZoomEnabled;
    state_.settings.autoPauseOnHide=preset.autoPauseOnHide;
    state_.settings.disableHotkeysOnTyping=preset.disableHotkeysOnTyping;
    state_.settings.useSystemTray=preset.useSystemTray;
    state_.settings.backgroundMediaHotkeys=preset.backgroundMediaHotkeys;
    state_.settings.autoFitVideoFullscreen=preset.autoFitVideoFullscreen;
    state_.settings.lockVideoFullscreenAspect=preset.lockVideoFullscreenAspect;
    state_.settings.maximizedTopDragEnabled=preset.maximizedTopDragEnabled;
    state_.settings.immersionStyle=preset.immersionStyle;
    state_.settings.homeUrl=preset.homeUrl;
    state_.settings.Clamp();state_.hotkeys=preset.hotkeys;state_.tabs=preset.tabs;if(state_.tabs.empty())state_.tabs.push_back({L"预设页面",preset.currentUrl.empty()?state_.settings.homeUrl:preset.currentUrl});for(auto& tab:state_.tabs)if(tab.url.empty())tab.url=state_.settings.homeUrl;
    state_.currentTabIndex=std::clamp(preset.activeTab,0,static_cast<int>(state_.tabs.size())-1);state_.session.activeTab=state_.currentTabIndex;
    auto result=window_.ApplyGeometry(preset.normalRect,preset.maximized);
    if(result.ok&&runtime_)result=runtime_();
    if(result.ok)result=browser_.Navigate(state_.tabs[static_cast<size_t>(state_.currentTabIndex)].url);
    if(result.ok){state_.windowMode=preset.maximized?WindowMode::Maximized:WindowMode::Normal;state_.immersionActive=false;state_.windowHidden=false;state_.session.normalRect=preset.normalRect;state_.session.maximized=preset.maximized;browser_.SetVisible(true);result=settings_.Save(state_);}
    if(!result.ok){state_=previous;auto restore=window_.ApplyGeometry(previous.session.normalRect,previous.session.maximized);if(restore.ok&&previous.immersionActive)restore=window_.SetMode(previous.windowMode);if(restore.ok&&previous.windowHidden)restore=window_.SetHidden(true);if(runtime_)runtime_();browser_.Navigate(state_.tabs[static_cast<size_t>(state_.currentTabIndex)].url);browser_.SetVisible(!state_.windowHidden);if(!restore.ok)result.message+=L" 窗口恢复失败："+restore.message;}
    return Finish(result);
}
OperationResult AppCoordinator::CommitRuntime(AppState next){
 const auto previous=state_;next.session.normalRect=window_.NormalRect();next.session.maximized=state_.windowMode==WindowMode::Maximized;state_=std::move(next);
 auto result=runtime_?runtime_():OperationResult{};
 if(result.ok)result=settings_.Save(state_);
 if(!result.ok){state_=previous;if(runtime_){auto rollback=runtime_();if(!rollback.ok)result.message+=L" 恢复失败："+rollback.message;}}
 return Finish(result);
}
OperationResult AppCoordinator::UpdateSettings(const AppSettings& draft){
 auto result=ValidateSettings(draft);if(!result.ok)return Finish(result);
 auto next=state_;next.settings=draft;next.settings.homeUrl=xiaochuang::NormalizeInputUrl(xiaochuang::TrimWhitespace(draft.homeUrl));return CommitRuntime(std::move(next));
}
OperationResult AppCoordinator::UpdateHotkey(HotkeyAction action,const HotkeyBinding& draft){
 const auto index=xiaochuang::HotkeyIndex(action);if(index>=state_.hotkeys.size())return Finish({false,L"无效的快捷键动作。"});
 constexpr UINT allowed=MOD_CONTROL|MOD_ALT|MOD_SHIFT|MOD_WIN;
 if(draft.virtualKey==0||draft.virtualKey>255||(draft.modifiers&~allowed))return Finish({false,L"无效的快捷键。"});
 for(size_t i=0;i<state_.hotkeys.size();++i)if(i!=index&&draft.enabled&&state_.hotkeys[i].enabled&&draft.virtualKey==state_.hotkeys[i].virtualKey&&draft.modifiers==state_.hotkeys[i].modifiers)return Finish({false,L"此组合已分配给其他动作。"});
 if(hotkeyValidator_){auto valid=hotkeyValidator_(action,draft);if(!valid.ok)return Finish(valid);}
 auto next=state_;next.hotkeys[index]=draft;next.hotkeys[index].id=state_.hotkeys[index].id;next.hotkeys[index].displayName=state_.hotkeys[index].displayName;return CommitRuntime(std::move(next));
}
OperationResult AppCoordinator::ResetDefaults(){
 auto next=state_;next.settings=AppSettings{};next.settings.themeMode=state_.settings.themeMode;next.settings.renderMode=state_.settings.renderMode;next.hotkeys=xiaochuang::DefaultHotkeys();return CommitRuntime(std::move(next));
}
void AppCoordinator::RequestDiagnostics(std::function<void(std::wstring)> done){
 auto text=xiaochuang::BuildSystemDiagnostics(state_,browser_.RuntimeVersion(),state_.windowMode);
 text+=L"WinUI 3 native interface\r\nHole basis: "+std::to_wstring(state_.settings.holeOpacityPercent)+L"%\r\nHover reduction: "+std::to_wstring(state_.settings.hoverReductionPercent)+L"%\r\nRunning render: "+(runningRender_==xiaochuang::RenderMode::AutomaticGpu?std::wstring(L"Automatic GPU"):std::wstring(L"Software compatibility"))+L"\r\nPending restart: "+(snapshot_.pendingRestart?std::wstring(L"yes"):std::wstring(L"no"))+L"\r\n";
 browser_.RequestDiagnostics([done=std::move(done),text=std::move(text)](std::wstring media){done(text+L"Media: "+xiaochuang::DecodeExecuteScriptString(media));});
}
}
