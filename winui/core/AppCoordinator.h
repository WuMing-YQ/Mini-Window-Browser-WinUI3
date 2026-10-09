#pragma once
#include "SessionController.h"
namespace miniwindow {
class AppCoordinator {
public:
    AppCoordinator(IBrowserPort&,IWindowPort&,ISettingsPort&);
    ~AppCoordinator();
    const AppSnapshot& Snapshot() const noexcept {return snapshot_;}
    void Subscribe(std::function<void(const AppSnapshot&)>);
    void Navigate(std::wstring);
    void ToggleImmersion();
    void RestoreNormal();
    void ToggleHidden();
    void OnBrowserState(bool ready,bool loading,std::wstring error);
private:
    void Publish(OperationResult result={});
    IBrowserPort& browser_;
    IWindowPort& window_;
    ISettingsPort& settings_;
    AppSnapshot snapshot_;
    std::function<void(const AppSnapshot&)> subscriber_;
};
}
