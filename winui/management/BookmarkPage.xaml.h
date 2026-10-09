#pragma once
#include "BookmarkPage.g.h"
#include "../core/AppCoordinator.h"
namespace winrt::MiniWindow::implementation {
struct BookmarkPage:BookmarkPageT<BookmarkPage> {
 BookmarkPage();void Bind(miniwindow::AppCoordinator&);void Refresh();bool Dirty();bool Save();
 Windows::Foundation::IAsyncOperation<bool> CanLeaveAsync();
private:
 void Load();Windows::Foundation::IAsyncAction SelectAsync(int);Windows::Foundation::IAsyncAction DeleteAsync();
 miniwindow::AppCoordinator* app_=nullptr;std::optional<size_t> selected_;std::optional<uint64_t> selectedId_;miniwindow::Bookmark original_;bool syncing_=false,pending_=false;
};
}
namespace winrt::MiniWindow::factory_implementation {struct BookmarkPage:BookmarkPageT<BookmarkPage,implementation::BookmarkPage>{};}
