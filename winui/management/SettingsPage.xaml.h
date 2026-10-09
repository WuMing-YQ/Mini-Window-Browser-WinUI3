#pragma once
#include "SettingsPage.g.h"
#include "../core/AppCoordinator.h"
#include <map>
#include <set>
namespace winrt::MiniWindow::implementation {
struct SettingsPage:SettingsPageT<SettingsPage>{
 SettingsPage();void Bind(miniwindow::AppCoordinator&,std::function<void(miniwindow::HotkeyAction)>,std::function<void()>,std::function<void()> cancelCapture={});
 void Refresh();bool Dirty()const{return !dirty_.empty();}Windows::Foundation::IAsyncOperation<bool> CanLeaveAsync();
private:
 using Panel=Microsoft::UI::Xaml::Controls::StackPanel;
 struct Field{Microsoft::UI::Xaml::UIElement control{nullptr};std::function<miniwindow::OperationResult(miniwindow::AppSettings&)> read;std::function<void(const miniwindow::AppSettings&)> write;};
 void Boolean(Panel,const std::wstring&,bool miniwindow::AppSettings::*);
 void Number(Panel,const std::wstring&,int miniwindow::AppSettings::*,int,int);
 void Choice(Panel,const std::wstring&,std::vector<std::wstring>,std::function<int(const miniwindow::AppSettings&)>,std::function<void(miniwindow::AppSettings&,int)>);
 void Changed(const std::wstring&,bool);bool Commit(std::optional<std::wstring> only={});
 void Dependencies();Windows::Foundation::IAsyncAction ResetAsync();void Diagnose();
 miniwindow::AppCoordinator* app_=nullptr;std::map<std::wstring,Field> fields_;std::set<std::wstring> dirty_;std::wstring editing_;bool syncing_=false;
 std::vector<Panel> panels_;std::array<Microsoft::UI::Xaml::Controls::CheckBox,xiaochuang::kHotkeyCount> enabled_{};
 std::array<Microsoft::UI::Xaml::Controls::Button,xiaochuang::kHotkeyCount> bindings_{};
 Microsoft::UI::Xaml::Controls::TextBlock hotkeyStatus_{nullptr},restart_{nullptr};
 Microsoft::UI::Xaml::Controls::TextBox diagnostic_{nullptr};Microsoft::UI::Xaml::DispatcherTimer diagnosticTimer_{nullptr};uint64_t diagnosticGeneration_=0;
};}
namespace winrt::MiniWindow::factory_implementation{struct SettingsPage:SettingsPageT<SettingsPage,implementation::SettingsPage>{};}
