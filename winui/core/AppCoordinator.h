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
    void OnDocument(std::wstring,std::wstring,uint64_t id=0);
    void OnWindowState(WindowMode,bool,bool);
    void OnHotkeyErrors(std::vector<std::wstring>);
    void OnCaptureStatus(std::wstring text){snapshot_.captureStatus=std::move(text);Publish();}
    void OpenBookmark(size_t);
    std::optional<size_t> BookmarkIndex(uint64_t id)const;
    OperationResult SaveBookmark(std::optional<size_t>,Bookmark);
    OperationResult DeleteBookmark(size_t);
    OperationResult SaveCurrentPreset(std::wstring);
    OperationResult RenamePreset(size_t,std::wstring);
    OperationResult ApplyPreset(size_t);
    OperationResult DeletePreset(size_t);
    void SetRuntimeHandler(std::function<OperationResult()> handler){runtime_=std::move(handler);}
    OperationResult UpdateImmersion(ImmersionStyle,int basis,int reduction,int radiusPhysicalPixels,bool persist=true);
    OperationResult FlushSettings();
    OperationResult UpdateSettings(const AppSettings&);
    OperationResult UpdateHotkey(HotkeyAction,const HotkeyBinding&);
    OperationResult ResetDefaults();
    void RequestDiagnostics(std::function<void(std::wstring)>);
    void SetHotkeyValidator(std::function<OperationResult(HotkeyAction,const HotkeyBinding&)> handler){hotkeyValidator_=std::move(handler);}
    xiaochuang::RenderMode RunningRenderMode()const{return runningRender_;}
private:
    OperationResult CommitRuntime(AppState);
    xiaochuang::RenderMode runningRender_=xiaochuang::RenderMode::AutomaticGpu;
    std::function<OperationResult(HotkeyAction,const HotkeyBinding&)> hotkeyValidator_;
    OperationResult CommitCollection(AppState,std::optional<std::vector<uint64_t>> ids={});
    void InitializeBookmarkIds();uint64_t nextBookmarkId_=1;
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
