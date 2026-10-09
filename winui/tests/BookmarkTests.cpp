#include "Test.h"
#include "../core/AppCoordinator.h"
#include "../core/ChromeLayout.h"
namespace {
struct Browser:miniwindow::IBrowserPort {bool fail=false;std::wstring url;miniwindow::OperationResult Navigate(const std::wstring& value)override{if(fail)return {false,L"failed"};url=value;return {};}void SetVisible(bool)override{}void ExecuteMedia(miniwindow::MediaCommand,int)override{}void Close()override{}};
struct Window:miniwindow::IWindowPort {bool fail=false;RECT rect{10,20,810,620};miniwindow::OperationResult SetMode(miniwindow::WindowMode)override{return {!fail,L"window"};}miniwindow::OperationResult SetHidden(bool)override{return {};}miniwindow::OperationResult RestoreNormal()override{return {!fail,L"window"};}RECT NormalRect()const override{return rect;}miniwindow::OperationResult ApplyGeometry(RECT value,bool)override{if(fail)return {false,L"geometry"};rect=value;return {};}};
struct Store:miniwindow::ISettingsPort {bool fail=false;miniwindow::OperationResult Save(const miniwindow::AppState&)override{return {!fail,L"save"};}};
}
void BookmarkTests(){
 using namespace miniwindow;
Browser browser;Window window;Store store;AppState shared;AppCoordinator app(browser,window,store,shared);
 CHECK(!app.DeleteBookmark(0).ok);app.OpenBookmark(0);CHECK(browser.url.empty());
 CHECK(app.SaveBookmark({}, {L"视频🚀／你好",L"https://example.org"}).ok);
 CHECK(app.SaveBookmark({}, {std::wstring(2000,L'长'),L"https://example.org"}).ok);
 CHECK(app.Snapshot().state.bookmarks.size()==2);CHECK(app.Snapshot().state.bookmarks[0].title==L"视频🚀／你好");
 app.OpenBookmark(1);CHECK(browser.url==L"https://example.org");
 CHECK(app.SaveBookmark(0,{L"修改",L"other.example"}).ok);CHECK(shared.bookmarks[0].url==L"https://other.example");
 CHECK(!app.SaveBookmark({}, {L"",L""}).ok);
 store.fail=true;CHECK(!app.SaveBookmark({}, {L"保留草稿",L"example.org"}).ok);CHECK(shared.bookmarks.size()==2);store.fail=false;
 CHECK(app.DeleteBookmark(1).ok);CHECK(shared.bookmarks.size()==1);
}
