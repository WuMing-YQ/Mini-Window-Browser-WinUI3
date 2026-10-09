#include "SessionController.h"
#include "../../src/ConfigStore.h"
namespace miniwindow {
void SessionController::EnsureTab() {
    if(state_.tabs.empty()) state_.tabs.push_back({L"主页",state_.settings.homeUrl});
    state_.currentTabIndex=std::clamp(state_.currentTabIndex,0,static_cast<int>(state_.tabs.size())-1);
    state_.session.activeTab=state_.currentTabIndex;
}
void SessionController::AddTab(std::wstring url) {
    state_.tabs.push_back({L"正在加载…",xiaochuang::NormalizeInputUrl(url)});
    state_.currentTabIndex=static_cast<int>(state_.tabs.size())-1;
    EnsureTab();
}
void SessionController::SelectTab(size_t index) {
    if(index>=state_.tabs.size()) return;
    state_.currentTabIndex=static_cast<int>(index);EnsureTab();
}
void SessionController::CloseTab(size_t index) {
    if(index>=state_.tabs.size()) return;
    state_.tabs.erase(state_.tabs.begin()+static_cast<ptrdiff_t>(index));
    if(static_cast<int>(index)<state_.currentTabIndex) --state_.currentTabIndex;
    EnsureTab();
}
void SessionController::CloseOthers(size_t index) {
    if(index>=state_.tabs.size()) return;
    auto active=state_.tabs[index];state_.tabs={std::move(active)};
    state_.currentTabIndex=0;EnsureTab();
}
void SessionController::CloseAll() {state_.tabs.clear();state_.currentTabIndex=0;EnsureTab();}
}
