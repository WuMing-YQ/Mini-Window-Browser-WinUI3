#pragma once
#include "../core/Ports.h"
#include "PageSignals.h"
#include <winrt/Windows.Foundation.h>
#include <filesystem>
#include <memory>
namespace miniwindow {
class BrowserHost final : public IBrowserPort {
public:
    BrowserHost(HWND,std::filesystem::path,std::function<void(bool,bool,std::wstring)>);
    ~BrowserHost();
    winrt::Windows::Foundation::IAsyncAction InitializeAsync();
    OperationResult Navigate(const std::wstring&) override;
    void SetVisible(bool) override;
    void ExecuteMedia(MediaCommand,int) override;
    void Close() override;
    OperationResult SetFixedZoom(bool,int);
    OperationResult Configure(const AppSettings&);
    void SetBounds(RECT);
    void NotifyParentMoved();
    void SetPageSignalHandler(std::function<void(PageSignal)>);
    void SetDocumentHandler(std::function<void(std::wstring,std::wstring)>);
    void SetNavigationHandlers(std::function<void()> cancelGesture,std::function<void(std::wstring)> newTab,std::function<void()> closeTab);
    void SetFocusHandler(std::function<void(bool)>);
    void Focus();
    void GoBack();
    bool CanGoBack() const;
    void RefreshOrStop();
    void RequestDiagnostics(std::function<void(std::wstring)>) override;
    std::wstring RuntimeVersion()const override;
private:
    struct State;
    static void ConfigureEvents(const std::shared_ptr<State>&,uint64_t);
    std::shared_ptr<State> state_;
};
}
