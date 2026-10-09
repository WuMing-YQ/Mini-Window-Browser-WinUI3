#include "Test.h"
#include "../core/AppCoordinator.h"
#include "../core/ChromeLayout.h"
namespace {
struct Browser:miniwindow::IBrowserPort {bool fail=false;std::wstring url;miniwindow::OperationResult Navigate(const std::wstring& value)override{if(fail)return {false,L"failed"};url=value;return {};}void SetVisible(bool)override{}void ExecuteMedia(miniwindow::MediaCommand,int)override{}void Close()override{}};
struct Window:miniwindow::IWindowPort {bool fail=false;miniwindow::WindowMode mode=miniwindow::WindowMode::Normal;bool hidden=false;RECT rect{10,20,810,620};miniwindow::OperationResult SetMode(miniwindow::WindowMode value)override{if(!fail)mode=value;return {!fail,L"window"};}miniwindow::OperationResult SetHidden(bool value)override{hidden=value;return {};}miniwindow::OperationResult RestoreNormal()override{return {!fail,L"window"};}RECT NormalRect()const override{return rect;}miniwindow::OperationResult ApplyGeometry(RECT value,bool maximized)override{if(fail)return {false,L"geometry"};rect=value;hidden=false;mode=maximized?miniwindow::WindowMode::Maximized:miniwindow::WindowMode::Normal;return {};}};
struct Store:miniwindow::ISettingsPort {bool fail=false;miniwindow::OperationResult Save(const miniwindow::AppState&)override{return {!fail,L"save"};}};
}
void PresetTests(){
 using namespace miniwindow;
 Browser browser;Window window;Store store;AppState shared;AppCoordinator app(browser,window,store,shared);
 CHECK(app.SaveBookmark({}, {L"保留书签",L"example.org"}).ok);
 CHECK(!app.SaveCurrentPreset(L"  ").ok);CHECK(app.SaveCurrentPreset(L" 场景🚀 ").ok);CHECK(shared.presets[0].name==L"场景🚀");
 CHECK(!app.SaveCurrentPreset(L"场景🚀").ok);CHECK(!app.RenamePreset(0,L" \t ").ok);
 shared.settings.hoverReductionPercent=50;shared.settings.windowWheelModifiers=MOD_SHIFT;shared.settings.fixedWebZoomEnabled=true;shared.settings.fixedWebZoomPercent=125;
 app.AddTab(L"other.example");CHECK(app.SaveCurrentPreset(L"第二场景").ok);
 CHECK(!app.RenamePreset(1,L" 场景🚀 ").ok);CHECK(app.RenamePreset(1,L" 别名 ").ok);
 shared.settings.themeMode=xiaochuang::ThemeMode::Dark;shared.settings.renderMode=xiaochuang::RenderMode::SoftwareCompatibility;
 app.CloseAllTabs();CHECK(app.ApplyPreset(1).ok);CHECK(shared.tabs.size()==2);CHECK(shared.currentTabIndex==1);
 CHECK(shared.settings.themeMode==xiaochuang::ThemeMode::Dark);CHECK(shared.settings.renderMode==xiaochuang::RenderMode::SoftwareCompatibility);
 CHECK(shared.settings.hoverReductionPercent==50);CHECK(shared.settings.windowWheelModifiers==MOD_SHIFT);CHECK(shared.settings.fixedWebZoomPercent==125);
 CHECK(shared.bookmarks.size()==1);CHECK(!shared.immersionActive&&!shared.windowHidden);
 shared.windowMode=WindowMode::ImmersionHole;shared.immersionActive=true;shared.windowHidden=true;window.mode=WindowMode::ImmersionHole;window.hidden=true;
 browser.fail=true;auto before=shared.tabs;CHECK(!app.ApplyPreset(0).ok);CHECK(shared.tabs.size()==before.size());CHECK(window.mode==WindowMode::ImmersionHole);CHECK(window.hidden);browser.fail=false;
 window.fail=true;CHECK(!app.ApplyPreset(0).ok);CHECK(shared.tabs.size()==before.size());window.fail=false;
 CHECK(app.DeletePreset(0).ok);CHECK(shared.presets.size()==1);CHECK(!app.DeletePreset(9).ok);
}
