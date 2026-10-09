#pragma once
#include <windows.h>
#undef GetCurrentTime
#include <unknwn.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
namespace miniwindow {
inline bool HasOwnedTextFocus(const winrt::Microsoft::UI::Xaml::XamlRoot& root) {
    if(!root)return false;
    auto element=winrt::Microsoft::UI::Xaml::Input::FocusManager::GetFocusedElement(root).try_as<winrt::Microsoft::UI::Xaml::DependencyObject>();
    while(element){if(element.try_as<winrt::Microsoft::UI::Xaml::Controls::TextBox>()||element.try_as<winrt::Microsoft::UI::Xaml::Controls::PasswordBox>()||element.try_as<winrt::Microsoft::UI::Xaml::Controls::RichEditBox>())return true;element=winrt::Microsoft::UI::Xaml::Media::VisualTreeHelper::GetParent(element);}
    return false;
}
inline UINT CurrentKeyModifiers(){UINT modifiers=0;if(GetKeyState(VK_CONTROL)&0x8000)modifiers|=MOD_CONTROL;if(GetKeyState(VK_MENU)&0x8000)modifiers|=MOD_ALT;if(GetKeyState(VK_SHIFT)&0x8000)modifiers|=MOD_SHIFT;if((GetKeyState(VK_LWIN)|GetKeyState(VK_RWIN))&0x8000)modifiers|=MOD_WIN;return modifiers;}
}
