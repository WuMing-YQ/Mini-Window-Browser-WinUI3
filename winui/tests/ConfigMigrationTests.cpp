#include "Test.h"
#include "TestFiles.h"
#include "../settings/VersionedConfigStore.h"
#include "../../src/ConfigStore.h"
void ConfigMigrationTests() {
    TestDirectory directory;
    auto legacy=directory.path/L"config.ini",target=directory.path/L"config.winui.ini";
    WriteFixture(legacy,L"[Settings]\r\nAutoHideOpacityPercent=80\r\nHoleOpacityPercent=42\r\nHoleRadius=400\r\nHomeUrl=https://example.org/\r\n[Hotkeys]\r\nImmersion=3,65,0\r\n[Bookmarks]\r\nCount=1\r\nTitle0=视频🚀／你好\r\nUrl0=https://example.org/video\r\n[Presets]\r\nCount=1\r\nP0_Name=办公室\r\nP0_AutoHideOpacity=80\r\nP0_HoleOpacity=35\r\nP0_Rad=400\r\nP0_Snap=27\r\nP0_HoldRate=5\r\nP0_Pause=0\r\nP0_HkTyping=0\r\nP0_SysTray=1\r\nP0_BackgroundMedia=1\r\nP0_AutoFitVideo=1\r\nP0_LockVideoAspect=1\r\nP0_MaxTopDrag=1\r\nP0_FixedWebZoomEnabled=1\r\nP0_FixedWebZoomPercent=175\r\nP0_ImmStyle=1\r\nP0_X=-1234\r\nP0_Y=70\r\nP0_W=800\r\nP0_H=450\r\nP0_Fullscreen=1\r\nP0_TabCount=1\r\nP0_Tab0_Url=https://example.org/video\r\nP0_Tab0_Title=中文🚀\r\nP0_ActiveTab=0\r\nP0_HkPlay=3,66,0\r\n");
    const auto original=Bytes(legacy);
    miniwindow::VersionedConfigStore store(target,legacy);
    miniwindow::AppState state;
    CHECK(store.LoadOrMigrate(state).ok);
    CHECK(Bytes(legacy)==original);
    CHECK(state.settings.autoHideOpacityPercent==80 && state.settings.hoverReductionPercent==100);
    CHECK(state.settings.holeOpacityPercent==100);
    CHECK(state.settings.holeRadius==400);
    CHECK(!state.hotkeys[0].enabled && state.hotkeys[0].modifiers==3);
    CHECK(state.bookmarks.size()==1 && state.bookmarks[0].title==L"视频🚀／你好");
    CHECK(state.presets.size()==1);
    const auto& preset=state.presets[0];
    CHECK(preset.holeRadius==400 && preset.snapThreshold==27 && preset.holdPlaybackRate==5);
    CHECK(preset.autoHideOpacityPercent==80 && preset.holeOpacityPercent==100 && preset.hoverReductionPercent==100);
    CHECK(!preset.autoPauseOnHide && preset.useSystemTray && preset.backgroundMediaHotkeys);
    CHECK(preset.autoFitVideoFullscreen && preset.lockVideoFullscreenAspect && preset.maximizedTopDragEnabled);
    CHECK(preset.fixedWebZoomEnabled && preset.fixedWebZoomPercent==175);
    CHECK(preset.normalRect.left==-1234 && preset.normalRect.top==70 && preset.normalRect.right==-434 && preset.normalRect.bottom==520 && preset.maximized);
    CHECK(preset.immersionStyle==miniwindow::ImmersionStyle::AutoHide && preset.tabs[0].title==L"中文🚀");
    CHECK(!preset.hotkeys[1].enabled && preset.hotkeys[1].virtualKey==66);
    state.settings.hoverReductionPercent=50;state.settings.windowWheelModifiers=MOD_SHIFT|MOD_ALT;
    state.presets[0].hoverReductionPercent=25;state.presets[0].windowWheelEnabled=false;
    CHECK(store.Save(state).ok);
    miniwindow::AppState second;CHECK(store.LoadOrMigrate(second).ok);
    CHECK(second.settings.hoverReductionPercent==50 && second.settings.windowWheelModifiers==(MOD_SHIFT|MOD_ALT));
    CHECK(second.presets[0].hoverReductionPercent==25 && !second.presets[0].windowWheelEnabled);
    size_t backups=0;for(const auto& item:std::filesystem::directory_iterator(directory.path))if(item.path().filename().wstring().starts_with(L"config.legacy-backup-"))++backups;
    CHECK(backups==1);
    const auto saved=Bytes(target);SetFileAttributesW(target.c_str(),FILE_ATTRIBUTE_READONLY);
    CHECK(!store.Save(state).ok);CHECK(Bytes(target)==saved);SetFileAttributesW(target.c_str(),FILE_ATTRIBUTE_NORMAL);
    TestDirectory broken;auto old=broken.path/L"config.ini",fresh=broken.path/L"config.winui.ini";
    WriteFixture(old,L"this is not an ini file");const auto bad=Bytes(old);
    miniwindow::VersionedConfigStore invalid(fresh,old);CHECK(!invalid.LoadOrMigrate(second).ok);CHECK(Bytes(old)==bad);CHECK(!std::filesystem::exists(fresh));
    WriteFixture(old,L"[Settings]\r\nHoleRadius=not-a-number\r\n");CHECK(!invalid.LoadOrMigrate(second).ok);
    WriteFixture(fresh,L"[Meta]\r\nSchemaVersion=999\r\n[Settings]\r\nHoleRadius=400\r\n");CHECK(!invalid.LoadOrMigrate(second).ok);
    TestDirectory failure;auto failureOld=failure.path/L"config.ini",failureNew=failure.path/L"config.winui.ini";
    WriteFixture(failureOld,L"[Settings]\r\nHoleRadius=400\r\n");const auto failureBytes=Bytes(failureOld);
    bool writeAttempted=false;
    miniwindow::ConfigFileOperations io;
    io.backup=[](const auto&){return miniwindow::OperationResult{false,L"backup denied"};};
    io.replace=[&](const auto&,const auto&){writeAttempted=true;return miniwindow::OperationResult{};};
    miniwindow::VersionedConfigStore backupFailure(failureNew,failureOld,io);
    CHECK(!backupFailure.LoadOrMigrate(second).ok);CHECK(!writeAttempted);CHECK(Bytes(failureOld)==failureBytes);CHECK(!std::filesystem::exists(failureNew));
    miniwindow::VersionedConfigStore writeFailure(failure.path/L"missing"/L"config.winui.ini",failureOld);
    CHECK(!writeFailure.LoadOrMigrate(second).ok);CHECK(Bytes(failureOld)==failureBytes);
    TestDirectory absent;miniwindow::VersionedConfigStore defaults(absent.path/L"config.winui.ini",absent.path/L"config.ini");CHECK(defaults.LoadOrMigrate(second).ok);CHECK(second.settings.holeRadius==400);
}
