#pragma once
#include "ManagementWindow.g.h"
#include "BookmarkPage.xaml.h"
#include "PresetPage.xaml.h"
#include "SettingsPage.xaml.h"
namespace miniwindow {enum class ManagementPage{Bookmarks,Presets,Settings};}
namespace winrt::MiniWindow::implementation {
struct ManagementWindow:ManagementWindowT<ManagementWindow>{
 ManagementWindow();void Bind(HWND,miniwindow::AppCoordinator&,std::function<void()>,std::function<bool(UINT,UINT)>,std::function<void(miniwindow::HotkeyAction)>,std::function<void()> panel={},std::function<void()> cancelCapture={});void Open(miniwindow::ManagementPage);void Refresh();void RefreshTheme();void Shutdown();bool Dirty();bool ForegroundTextFocus();
 Windows::Foundation::IAsyncOperation<bool> CanLeaveAsync();
private:Windows::Foundation::IAsyncAction NavigateAsync(miniwindow::ManagementPage);Windows::Foundation::IAsyncAction HideAsync();void SetPage(miniwindow::ManagementPage);
 std::function<void()> cancelCapture_;
 miniwindow::AppCoordinator* app_=nullptr;com_ptr<BookmarkPage> bookmarks_;com_ptr<PresetPage> presets_;com_ptr<SettingsPage> settings_;miniwindow::ManagementPage page_=miniwindow::ManagementPage::Bookmarks;HWND hwnd_=nullptr;bool syncing_=false,pending_=false,shuttingDown_=false;
};}
namespace winrt::MiniWindow::factory_implementation{struct ManagementWindow:ManagementWindowT<ManagementWindow,implementation::ManagementWindow>{};}
