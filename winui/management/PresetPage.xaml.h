#pragma once
#include "PresetPage.g.h"
#include "../core/AppCoordinator.h"
namespace winrt::MiniWindow::implementation {
struct PresetPage:PresetPageT<PresetPage> {
 PresetPage();void Bind(miniwindow::AppCoordinator&);void Refresh();bool Dirty();bool Save();Windows::Foundation::IAsyncOperation<bool> CanLeaveAsync();
private:void Load();Windows::Foundation::IAsyncAction SelectAsync(int);Windows::Foundation::IAsyncAction ActionAsync(bool remove);
 miniwindow::AppCoordinator* app_=nullptr;std::optional<size_t> selected_;std::wstring original_;bool syncing_=false,pending_=false;
};}
namespace winrt::MiniWindow::factory_implementation {struct PresetPage:PresetPageT<PresetPage,implementation::PresetPage>{};}
