#include "Test.h"
#include "../core/SessionController.h"
void SessionTests() {
    xiaochuang::AppState state;
    state.settings.homeUrl=L"https://example.org/home";
    miniwindow::SessionController session(state);
    session.AddTab(L"https://example.org/a");
    session.CloseTab(0);
    CHECK(state.tabs.size()==1);
    CHECK(state.tabs[0].url==L"https://example.org/home");
    session.AddTab(L"https://example.org/b");
    session.AddTab(L"https://example.org/c");
    CHECK(state.currentTabIndex==2);
    session.SelectTab(999);
    CHECK(state.currentTabIndex==2);
    session.CloseTab(0);
    CHECK(state.currentTabIndex==1);
    CHECK(state.tabs[1].url==L"https://example.org/c");
    session.CloseOthers(1);
    CHECK(state.tabs.size()==1);
    CHECK(state.currentTabIndex==0);
    CHECK(state.tabs[0].url==L"https://example.org/c");
    session.CloseAll();
    CHECK(state.tabs.size()==1 && state.tabs[0].url==L"https://example.org/home");
    session.CloseOthers(400);
    CHECK(state.tabs[0].url==L"https://example.org/home");
}
