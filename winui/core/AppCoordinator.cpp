#include "AppCoordinator.h"
#include "../../src/ConfigStore.h"
namespace miniwindow {
AppCoordinator::AppCoordinator(IBrowserPort& browser,IWindowPort& window,ISettingsPort& settings)
    :browser_(browser),window_(window),settings_(settings) {
    SessionController(snapshot_.state).CloseAll();
}
AppCoordinator::~AppCoordinator() {subscriber_={};}
void AppCoordinator::Subscribe(std::function<void(const AppSnapshot&)> subscriber) {
    subscriber_=std::move(subscriber);
    if(subscriber_) subscriber_(snapshot_);
}
void AppCoordinator::Publish(OperationResult result) {
    snapshot_.error=result.ok?L"":std::move(result.message);
    ++snapshot_.revision;
    auto subscriber=subscriber_;
    if(subscriber) subscriber(snapshot_);
}
void AppCoordinator::Navigate(std::wstring input) {
    const auto url=xiaochuang::NormalizeInputUrl(input);
    auto result=browser_.Navigate(url);
    if(result.ok) snapshot_.state.tabs[static_cast<size_t>(snapshot_.state.currentTabIndex)].url=url;
    Publish(std::move(result));
}
void AppCoordinator::ToggleImmersion() {
    auto& state=snapshot_.state;
    if(state.immersionActive) {RestoreNormal();return;}
    const auto target=state.settings.immersionStyle==ImmersionStyle::Hole?WindowMode::ImmersionHole:WindowMode::ImmersionAutoHide;
    auto result=window_.SetMode(target);
    if(result.ok) {state.windowMode=target;state.immersionActive=true;state.windowHidden=false;browser_.SetVisible(true);}
    Publish(std::move(result));
}
void AppCoordinator::RestoreNormal() {
    auto result=window_.RestoreNormal();
    if(result.ok) {
        auto& state=snapshot_.state;
        state.windowMode=WindowMode::Normal;state.immersionActive=false;state.windowHidden=false;
        state.session.normalRect=window_.NormalRect();state.session.maximized=false;
        browser_.SetVisible(true);
    }
    Publish(std::move(result));
}
void AppCoordinator::ToggleHidden() {
    auto& state=snapshot_.state;
    const bool hidden=!state.windowHidden;
    auto result=window_.SetHidden(hidden);
    if(result.ok) {state.windowHidden=hidden;browser_.SetVisible(!hidden);}
    Publish(std::move(result));
}
void AppCoordinator::OnBrowserState(bool ready,bool loading,std::wstring error) {
    snapshot_.browserReady=ready;snapshot_.loading=loading;
    const bool ok=error.empty();Publish({ok,std::move(error)});
}
}
