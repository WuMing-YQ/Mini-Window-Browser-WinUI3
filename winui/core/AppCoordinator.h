#pragma once
#include "SessionController.h"
#include <optional>
namespace miniwindow {
class AppCoordinator {
public:
    AppCoordinator(IBrowserPort&,IWindowPort&,ISettingsPort&);
    AppCoordinator(IBrowserPort&,IWindowPort&,ISettingsPort&,AppState& sharedState);
    ~AppCoordinator();
    const AppSnapshot& Snapshot() const noexcept {return snapshot_;}
    void Subscribe(std::function<void(const AppSnapshot&)>);
    void Navigate(std::wstring);
    void ToggleImmersion();
    void RestoreNormal();
    void ToggleHidden();
    void OnBrowserState(bool ready,bool loading,std::wstring error);
    void AddTab(std::wstring);void SelectTab(size_t);void CloseTab(size_t);void CloseOtherTabs(size_t);void CloseAllTabs();
    void OnDocument(std::wstring,std::wstring);
    void OnWindowState(WindowMode,bool,bool);
    void OnHotkeyErrors(std::vector<std::wstring>);
    void OpenBookmark(size_t);
    OperationResult SaveBookmark(std::optional<size_t>,Bookmark);
    OperationResult DeleteBookmark(size_t);
    OperationResult SaveCurrentPreset(std::wstring);
    OperationResult RenamePreset(size_t,std::wstring);
    OperationResult ApplyPreset(size_t);
    OperationResult DeletePreset(size_t);
    void SetRuntimeHandler(std::function<OperationResult()> handler){runtime_=std::move(handler);}
private:
    OperationResult CommitCollection(AppState);
    OperationResult ValidatePresetName(std::wstring&,std::optional<size_t> ignore={});
    OperationResult Finish(OperationResult result){Publish(result);return result;}
    void NavigateActive();
    void Publish(OperationResult result={});
    IBrowserPort& browser_;
    IWindowPort& window_;
    ISettingsPort& settings_;
    AppSnapshot snapshot_;
    AppState& state_;
    std::function<OperationResult()> runtime_;
    std::function<void(const AppSnapshot&)> subscriber_;
};
}
