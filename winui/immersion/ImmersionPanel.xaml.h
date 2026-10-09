#pragma once
#include "ImmersionPanel.g.h"
#include "../core/AppCoordinator.h"
namespace winrt::MiniWindow::implementation {
struct ImmersionPanel:ImmersionPanelT<ImmersionPanel>{
 ImmersionPanel();void Bind(HWND,miniwindow::AppCoordinator&,std::function<void()>,std::function<bool(UINT,UINT)>);
 void Open();void ClosePanel();void Shutdown();void Refresh();void RefreshTheme();bool ForegroundTextFocus();bool ContainsScreenPoint(POINT)const;
private:
 bool Apply();void UpdateCurrent();void QueueSave();bool Flush();
 HWND hwnd_=nullptr,owner_=nullptr;miniwindow::AppCoordinator* app_=nullptr;
 Microsoft::UI::Xaml::DispatcherTimer saveTimer_{nullptr},statusTimer_{nullptr};
 bool syncing_=false,dirty_=false,pendingSave_=false,shuttingDown_=false;
};}
namespace winrt::MiniWindow::factory_implementation{struct ImmersionPanel:ImmersionPanelT<ImmersionPanel,implementation::ImmersionPanel>{};}
