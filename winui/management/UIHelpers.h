#pragma once
#include "../pch.h"
namespace miniwindow::ui {
inline winrt::Microsoft::UI::Xaml::Controls::TextBlock Text(const std::wstring& value){winrt::Microsoft::UI::Xaml::Controls::TextBlock text;text.Text(value);text.TextWrapping(winrt::Microsoft::UI::Xaml::TextWrapping::Wrap);return text;}
inline winrt::Windows::Foundation::IAsyncOperation<bool> Confirm(winrt::Microsoft::UI::Xaml::XamlRoot root,std::wstring title,std::wstring message,std::wstring action=L"确认"){
    winrt::Microsoft::UI::Xaml::Controls::ContentDialog dialog;dialog.XamlRoot(root);dialog.Title(winrt::box_value(title));dialog.Content(winrt::box_value(message));dialog.PrimaryButtonText(action);dialog.CloseButtonText(L"取消");
    co_return co_await dialog.ShowAsync()==winrt::Microsoft::UI::Xaml::Controls::ContentDialogResult::Primary;
}
}
