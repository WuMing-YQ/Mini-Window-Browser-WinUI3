#include "Test.h"
#include "../core/AppCoordinator.h"
namespace {
struct Browser : miniwindow::IBrowserPort {
    bool fail=false;
    miniwindow::OperationResult Navigate(const std::wstring&) override {return {!fail,fail?L"Navigation failed":L""};}
    void SetVisible(bool) override {}
    void ExecuteMedia(miniwindow::MediaCommand,int) override {}
    void Close() override {}
};
struct Window : miniwindow::IWindowPort {
    bool fail=false;
    miniwindow::OperationResult SetMode(miniwindow::WindowMode) override {return {!fail,fail?L"Window failed":L""};}
    miniwindow::OperationResult SetHidden(bool) override {return {!fail,fail?L"Hide failed":L""};}
    miniwindow::OperationResult RestoreNormal() override {return {!fail,fail?L"Restore failed":L""};}
    RECT NormalRect() const override {return {20,30,820,630};}
};
struct Store : miniwindow::ISettingsPort {
    bool fail=false;
    miniwindow::OperationResult Save(const miniwindow::AppState&) override {return {!fail,fail?L"Save failed":L""};}
};
}
void CoordinatorTests() {
    Browser browser; Window window; Store store;
    miniwindow::AppCoordinator app(browser,window,store);
    uint64_t last=0; int updates=0;
    app.Subscribe([&](const auto& snapshot){CHECK(snapshot.revision>last);last=snapshot.revision;++updates;});
    window.fail=true;
    app.ToggleImmersion();
    CHECK(app.Snapshot().state.windowMode==miniwindow::WindowMode::Normal);
    CHECK(!app.Snapshot().error.empty());
    window.fail=false;
    app.ToggleImmersion();
    CHECK(app.Snapshot().state.windowMode==miniwindow::WindowMode::ImmersionHole);
    app.RestoreNormal();
    CHECK(app.Snapshot().state.windowMode==miniwindow::WindowMode::Normal);
    app.Navigate(L"example.org");
    CHECK(app.Snapshot().state.tabs[0].url==L"https://example.org");
    browser.fail=true;
    app.Navigate(L"https://bad.example/");
    CHECK(app.Snapshot().state.tabs[0].url==L"https://example.org");
    CHECK(!app.Snapshot().error.empty());
    window.fail=true;
    app.ToggleHidden();
    CHECK(!app.Snapshot().state.windowHidden);
    window.fail=false;
    app.ToggleHidden();
    CHECK(app.Snapshot().state.windowHidden);
    app.RestoreNormal();
    CHECK(!app.Snapshot().state.windowHidden);
    app.OnBrowserState(true,true,L"");
    CHECK(app.Snapshot().browserReady && app.Snapshot().loading);
    CHECK(updates>=9);
    app.Subscribe({});
    const int before=updates;
    app.OnBrowserState(false,false,L"closed");
    CHECK(updates==before);
}
