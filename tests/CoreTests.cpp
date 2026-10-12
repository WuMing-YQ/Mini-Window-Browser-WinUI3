#include "../src/AppModel.h"
#include "../src/ConfigStore.h"
#include "../src/CoreLogic.h"
#include "../src/Diagnostics.h"
#include "../src/MediaBridge.h"

#include <windows.h>

#include <filesystem>
#include <iostream>
#include <string>

namespace {

int failures = 0;
int checks = 0;

void Check(bool condition, const wchar_t* name) {
    ++checks;
    if (condition) {
        std::cout << "[PASS " << checks << "]\n";
    } else {
        std::cout << "[FAIL " << checks << "]\n";
        OutputDebugStringW(name);
        ++failures;
    }
}

void TestConfigMigration() {
    wchar_t tempDirectory[MAX_PATH]{};
    GetTempPathW(static_cast<DWORD>(std::size(tempDirectory)), tempDirectory);
    const std::filesystem::path path = std::filesystem::path(tempDirectory) /
        (L"XiaoChuang-CoreTests-" + std::to_wstring(GetCurrentProcessId()) + L".ini");
    DeleteFileW(path.c_str());
    WritePrivateProfileStringW(L"Settings", L"HoleRadius", L"-20", path.c_str());
    WritePrivateProfileStringW(L"Settings", L"SnapThreshold", L"999", path.c_str());
    WritePrivateProfileStringW(L"Settings", L"HoldPlaybackRate", L"8", path.c_str());
    WritePrivateProfileStringW(L"Settings", L"HomeUrl", L"https://www.bilibili.com", path.c_str());
    WritePrivateProfileStringW(L"Session", L"TabCount", L"0", path.c_str());
    WritePrivateProfileStringW(L"Hotkeys", L"Immersion", L"0,48", path.c_str());

    xiaochuang::AppState state;
    const xiaochuang::ConfigStore store(path);
    Check(store.Load(state), L"旧配置可加载");
    Check(state.settings.holeRadius == 10, L"挖孔半径范围校验");
    Check(state.settings.snapThreshold == 200, L"吸附距离范围校验");
    Check(state.settings.holdPlaybackRate == 5, L"长按倍速范围校验");
    Check(state.settings.holeOpacityPercent == 100 &&
          state.settings.autoHideOpacityPercent == 100, L"新增透明度键默认迁移");
    Check(!state.settings.backgroundMediaHotkeys, L"后台媒体热键默认关闭");
    Check(!state.settings.autoFitVideoFullscreen, L"网页全屏比例适配默认关闭");
    Check(!state.settings.lockVideoFullscreenAspect, L"网页全屏比例锁定默认关闭");
    Check(!state.settings.maximizedTopDragEnabled, L"最大化顶部拖动恢复默认关闭");
    Check(state.hotkeys[xiaochuang::HotkeyIndex(xiaochuang::HotkeyAction::Immersion)].enabled,
          L"旧版双字段快捷键默认启用");
    Check(state.tabs.size() == 1, L"空旧会话自动生成主页标签");
    Check(!state.settings.fixedWebZoomEnabled && state.settings.fixedWebZoomPercent == 100,
          L"旧配置网页缩放默认100且不锁定");
    Check(xiaochuang::NormalizeInputUrl(L"ABOUT:BLANK") == L"about:blank",
          L"空白页地址不会误转为搜索");

    RECT rect{20, 30, 920, 630};
    Check(store.Save(state, rect, false), L"新配置可保存");
    Check(!std::filesystem::exists(path.wstring() + L".tmp"), L"配置原子保存后不遗留临时文件");
    xiaochuang::AppState reloaded;
    Check(store.Load(reloaded), L"新配置可重新加载");
    Check(reloaded.settings.holeOpacityPercent == 100, L"新增配置往返一致");
    Check(!reloaded.settings.autoFitVideoFullscreen, L"网页全屏比例适配配置往返一致");
    reloaded.settings.autoFitVideoFullscreen = true;
    reloaded.settings.lockVideoFullscreenAspect = true;
    reloaded.settings.maximizedTopDragEnabled = true;
    reloaded.settings.fixedWebZoomEnabled = true;
    reloaded.settings.fixedWebZoomPercent = 135;
    Check(store.Save(reloaded, rect, false), L"最大化顶部拖动配置可保存");
    xiaochuang::AppState dragSettingReloaded;
    Check(store.Load(dragSettingReloaded) && dragSettingReloaded.settings.maximizedTopDragEnabled,
          L"最大化顶部拖动配置往返一致");
    Check(dragSettingReloaded.settings.autoFitVideoFullscreen &&
          dragSettingReloaded.settings.lockVideoFullscreenAspect,
          L"网页全屏自动适配与比例锁定配置往返一致");
    Check(dragSettingReloaded.settings.fixedWebZoomEnabled &&
          dragSettingReloaded.settings.fixedWebZoomPercent == 135, L"固定网页缩放配置往返一致");
    WritePrivateProfileStringW(L"Settings", L"FixedWebZoomPercent", L"0", path.c_str());
    xiaochuang::AppState zoomRange;
    store.Load(zoomRange);
    Check(zoomRange.settings.fixedWebZoomPercent == 25, L"网页缩放下限校验");
    WritePrivateProfileStringW(L"Settings", L"FixedWebZoomPercent", L"999", path.c_str());
    store.Load(zoomRange);
    Check(zoomRange.settings.fixedWebZoomPercent == 500, L"网页缩放上限校验");

    const std::array<UINT, xiaochuang::kHotkeyCount> auditKeys = {
        L'1', VK_OEM_3, L'3', L'4', VK_XBUTTON1, VK_XBUTTON2, L'9', VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT};
    for (size_t index = 0; index < xiaochuang::kHotkeyCount; ++index) {
        reloaded.hotkeys[index].virtualKey = auditKeys[index];
        reloaded.hotkeys[index].modifiers = index % 2 == 0 ? MOD_CONTROL : 0;
        reloaded.hotkeys[index].enabled = index != xiaochuang::HotkeyIndex(
            xiaochuang::HotkeyAction::Previous);
    }
    Check(store.Save(reloaded, rect, false), L"全部快捷键绑定可保存");
    xiaochuang::AppState hotkeyReloaded;
    Check(store.Load(hotkeyReloaded), L"全部快捷键绑定可重新加载");
    bool allHotkeysRoundTrip = true;
    for (size_t index = 0; index < xiaochuang::kHotkeyCount; ++index) {
        allHotkeysRoundTrip = allHotkeysRoundTrip &&
            hotkeyReloaded.hotkeys[index].virtualKey == auditKeys[index] &&
            hotkeyReloaded.hotkeys[index].modifiers ==
                (index % 2 == 0 ? static_cast<UINT>(MOD_CONTROL) : 0U) &&
            hotkeyReloaded.hotkeys[index].enabled ==
                (index != xiaochuang::HotkeyIndex(xiaochuang::HotkeyAction::Previous));
    }
    Check(allHotkeysRoundTrip, L"键盘与鼠标侧键绑定完整往返且动作顺序不串位");

    WritePrivateProfileStringW(L"Hotkeys", L"Forward", L"4294967295,0", path.c_str());
    xiaochuang::AppState invalidHotkeyState;
    Check(store.Load(invalidHotkeyState) &&
          invalidHotkeyState.hotkeys[xiaochuang::HotkeyIndex(
              xiaochuang::HotkeyAction::SeekForward)].virtualKey == L'6',
          L"无效快捷键配置回退默认值");

    xiaochuang::Preset preset;
    preset.name = L"三标签";
    preset.tabs = {{L"哔哩哔哩", L"https://www.bilibili.com"},
                   {L"抖音", L"https://www.douyin.com"},
                   {L"YouTube", L"https://www.youtube.com"}};
    preset.activeTab = 1;
    preset.currentUrl = preset.tabs[1].url;
    preset.autoFitVideoFullscreen = true;
    preset.lockVideoFullscreenAspect = true;
    preset.maximizedTopDragEnabled = true;
    preset.fixedWebZoomEnabled = true;
    preset.fixedWebZoomPercent = 80;
    preset.hotkeys[xiaochuang::HotkeyIndex(xiaochuang::HotkeyAction::Next)].enabled = false;
    reloaded.presets = {preset};
    Check(store.Save(reloaded, rect, false), L"多标签预设可保存");
    xiaochuang::AppState presetReloaded;
    Check(store.Load(presetReloaded), L"多标签预设可加载");
    Check(presetReloaded.presets.size() == 1 && presetReloaded.presets[0].tabs.size() == 3,
          L"预设完整保存全部标签页");
    Check(presetReloaded.presets[0].activeTab == 1 &&
          presetReloaded.presets[0].tabs[1].title == L"抖音",
          L"预设保存当前标签索引和标题");
    Check(presetReloaded.presets[0].autoFitVideoFullscreen &&
          presetReloaded.presets[0].lockVideoFullscreenAspect &&
          presetReloaded.presets[0].maximizedTopDragEnabled,
          L"预设保存网页全屏比例与顶部拖动设置");
    Check(!presetReloaded.presets[0].hotkeys[xiaochuang::HotkeyIndex(
              xiaochuang::HotkeyAction::Next)].enabled,
          L"预设保存逐项快捷键启用状态");
    Check(presetReloaded.presets[0].fixedWebZoomEnabled &&
          presetReloaded.presets[0].fixedWebZoomPercent == 80, L"预设独立保存网页缩放");

    WritePrivateProfileSectionW(L"Presets", nullptr, path.c_str());
    WritePrivateProfileStringW(L"Presets", L"Count", L"1", path.c_str());
    WritePrivateProfileStringW(L"Presets", L"P0_Name", L"旧版预设", path.c_str());
    WritePrivateProfileStringW(L"Presets", L"P0_CurUrl", L"https://example.com/legacy", path.c_str());
    WritePrivateProfileStringW(L"Hotkeys", L"HideWin", L"0,57,0", path.c_str());
    WritePrivateProfileStringW(L"Presets", L"P0_HkHide", L"0,57", path.c_str());
    xiaochuang::AppState legacyPresetState;
    Check(store.Load(legacyPresetState), L"旧版单网址预设可迁移");
    Check(legacyPresetState.presets.size() == 1 &&
          legacyPresetState.presets[0].tabs.size() == 1 &&
          legacyPresetState.presets[0].tabs[0].url == L"https://example.com/legacy",
          L"旧版预设迁移为单标签数组");
    Check(!legacyPresetState.hotkeys[xiaochuang::HotkeyIndex(xiaochuang::HotkeyAction::ToggleHidden)].enabled &&
          legacyPresetState.presets[0].hotkeys[xiaochuang::HotkeyIndex(xiaochuang::HotkeyAction::ToggleHidden)].enabled,
          L"旧版预设热键不继承当前全局停用状态");
    Check(!legacyPresetState.presets[0].fixedWebZoomEnabled &&
          legacyPresetState.presets[0].fixedWebZoomPercent == 100, L"旧版预设缩放默认兼容");
    DeleteFileW(path.c_str());
}

void TestKeyReleaseRecovery() {
    using namespace xiaochuang;
    Check(ShouldRecoverStaleKeyDown(true, false, false), L"丢失抬起消息后下一次按键可自愈");
    Check(!ShouldRecoverStaleKeyDown(true, true, false), L"有效长按手势不会被重复按下打断");
    Check(ShouldForceReleaseMessageGesture(5000, false), L"消息型长按丢失抬起后超时恢复");
    Check(!ShouldForceReleaseMessageGesture(4999, false), L"有效长按在超时前保持状态");
}

void TestRawMouseButtons() {
    using namespace xiaochuang;
    const auto sideOne = DecodeRawMouseButtons(RI_MOUSE_BUTTON_4_DOWN | RI_MOUSE_BUTTON_4_UP);
    Check(sideOne.size() == 2 && sideOne[0].virtualKey == VK_XBUTTON1 && sideOne[0].down &&
          sideOne[1].virtualKey == VK_XBUTTON1 && !sideOne[1].down,
          L"Raw Input 识别鼠标侧键1按下和松开");
    const auto sideTwo = DecodeRawMouseButtons(RI_MOUSE_BUTTON_5_DOWN | RI_MOUSE_BUTTON_5_UP);
    Check(sideTwo.size() == 2 && sideTwo[0].virtualKey == VK_XBUTTON2 && sideTwo[0].down &&
          sideTwo[1].virtualKey == VK_XBUTTON2 && !sideTwo[1].down,
          L"Raw Input 识别鼠标侧键2按下和松开");
    const auto middle = DecodeRawMouseButtons(RI_MOUSE_BUTTON_3_DOWN | RI_MOUSE_BUTTON_3_UP);
    Check(middle.size() == 2 && middle[0].virtualKey == VK_MBUTTON && middle[0].down &&
          middle[1].virtualKey == VK_MBUTTON && !middle[1].down,
          L"Raw Input 识别鼠标中键按下和松开");
    Check(DecodeRawMouseButtons(0).empty(), L"Raw Input 忽略无关鼠标移动");
}

void TestMediaBridgeCommands() {
    using namespace xiaochuang;
    const std::wstring bootstrap = MediaBridge::BootstrapScript();
    Check(bootstrap.find(L"window.player || window.bilibiliPlayer") != std::wstring::npos,
          L"Bilibili 普通视频上一集下一集使用播放器接口");
    const size_t bangumiFallback = bootstrap.find(
        L"if (isBangumi && clickFirst(selectors)) return true;");
    const size_t bilibiliPlayerCall = bootstrap.find(
        L"if (callPlayerMethod(player, direction < 0 ? 'prev' : 'next')) return true;");
    Check(bangumiFallback != std::wstring::npos && bilibiliPlayerCall != std::wstring::npos &&
          bangumiFallback < bilibiliPlayerCall,
          L"Bilibili 番剧先点击有效的前后集按钮再尝试空操作播放器接口");
    Check(bootstrap.find(L"element.shadowRoot") != std::wstring::npos,
          L"媒体按钮适配支持开放 Shadow DOM");
    Check(bootstrap.find(L"[data-action=\"previous\"]") != std::wstring::npos &&
          bootstrap.find(L"[data-action=\"next\"]") != std::wstring::npos,
          L"未知站点使用保守的上一集下一集回退");
    Check(bootstrap.find(L"MWB_MEDIA_COMMAND_V1") != std::wstring::npos &&
          bootstrap.find(L"event.source !== window.parent") != std::wstring::npos,
          L"子框架媒体命令仅接受直接父框架广播");

    Check(MediaBridge::CommandScript(MediaCommand::TogglePlay).find(L"'togglePlay'") != std::wstring::npos,
          L"播放暂停命令映射正确");
    Check(MediaBridge::CommandScript(MediaCommand::SeekBackward).find(L"'seekBackward'") != std::wstring::npos,
          L"快退命令映射正确");
    Check(MediaBridge::CommandScript(MediaCommand::SeekForward).find(L"'seekForward'") != std::wstring::npos,
          L"快进命令映射正确");
    Check(MediaBridge::CommandScript(MediaCommand::Previous).find(L"'previous'") != std::wstring::npos,
          L"上一集命令映射正确");
    Check(MediaBridge::CommandScript(MediaCommand::Next).find(L"'next'") != std::wstring::npos,
          L"下一集命令映射正确");
    Check(MediaBridge::CommandScript(MediaCommand::Next).find(L"window.frames.length") != std::wstring::npos &&
          MediaBridge::CommandScript(MediaCommand::Next).find(L"frame.postMessage") != std::wstring::npos,
          L"顶层页面找不到媒体时向子框架转发命令");
    Check(MediaBridge::CommandScript(MediaCommand::HoldBackward, 99).find(L"const value=5;") != std::wstring::npos,
          L"长按倍速在媒体命令层限制为 2 到 5 倍");
}

void TestPresetNames() {
    using namespace xiaochuang;
    const std::vector<std::wstring> names = {L"原神", L"BILIBILI"};
    Check(TrimWhitespace(L"  场景名称 \r\n") == L"场景名称", L"预设名称去除首尾空白");
    Check(HasPresetNameConflict(names, L" 原神 "), L"预设名称拒绝空白变体重名");
    Check(HasPresetNameConflict(names, L"bilibili"), L"预设名称不区分拉丁字母大小写");
    Check(!HasPresetNameConflict(names, L"原神", 0), L"重命名时允许保留原名称");
    Check(HasPresetNameConflict(names, L"   "), L"预设名称拒绝纯空白");
}

void TestWindowGeometryHelpers() {
    using namespace xiaochuang;
    const RECT maximized{0, 0, 1920, 1040};
    const RECT normal{100, 100, 1100, 700};
    const RECT work{0, 0, 1920, 1040};
    const RECT restored = CalculateRestoredDragRect(maximized, normal, POINT{960, 20}, work, 68);
    Check(restored.left == 460 && restored.top == 0 && restored.right == 1460 && restored.bottom == 600,
          L"最大化拖动按光标比例恢复普通窗口");
    const SIZE wide = CalculateAspectFitWindowSize(1000, 68, 16.0 / 9.0, 1920, 1040);
    Check(wide.cx == 1000 && wide.cy == 631, L"16:9 视频自动计算小窗高度");
    const SIZE constrained = CalculateAspectFitWindowSize(1900, 68, 1.0, 1920, 1040);
    Check(constrained.cx == 972 && constrained.cy == 1040, L"视频比例适配不超出工作区");
    Check(ShouldApplyVideoFullscreenFit(WindowMode::Maximized, true, true, 16.0 / 9.0),
          L"最大化窗口也允许视频比例适配");
    Check(ShouldApplyVideoFullscreenFit(WindowMode::Normal, true, true, 16.0 / 9.0),
          L"退出小窗全屏后仍在网页全屏时立即应用比例");
    const RECT monitor{0, 0, 1920, 1080};
    const RECT taskbarWork{0, 0, 1920, 1040};
    const RECT bottomRight = PositionAspectFitRect(
        RECT{1420, 560, 1920, 1040}, SIZE{800, 518}, monitor, taskbarWork, 20);
    Check(bottomRight.left == 1120 && bottomRight.top == 562 &&
          bottomRight.right == 1920 && bottomRight.bottom == 1080,
          L"视频比例适配保持显示器右下角锚定而不抬到任务栏上方");
    const RECT widthDriven = ConstrainAspectSizingRect(
        RECT{100, 100, 900, 900}, WMSZ_RIGHT, 68, 16.0 / 9.0);
    Check(widthDriven.left == 100 && widthDriven.right == 900 &&
          widthDriven.top == 100 && widthDriven.bottom == 618,
          L"全屏拖动左右边缘时保持视频比例");
    const RECT heightDriven = ConstrainAspectSizingRect(
        RECT{100, 100, 700, 600}, WMSZ_BOTTOM, 68, 16.0 / 9.0);
    Check(heightDriven.left == 100 && heightDriven.right == 868 &&
          heightDriven.top == 100 && heightDriven.bottom == 600,
          L"全屏拖动上下边缘时保持视频比例");

}

void TestScriptResultDecoding() {
    using namespace xiaochuang;
    Check(DecodeExecuteScriptString(L"\"hello\\nworld\"") == L"hello\nworld",
          L"ExecuteScript 结果解码常用转义");
    Check(DecodeExecuteScriptString(L"\"\\u4F60\\u597D\"") == L"你好",
          L"ExecuteScript 结果解码 Unicode");
    Check(DecodeExecuteScriptString(L"\"bad\\u12x4 tail\"") == L"bad\\u12x4 tail",
          L"ExecuteScript 结果保留损坏 Unicode 转义");
}

} // namespace

int wmain() {
    TestConfigMigration();
    TestKeyReleaseRecovery();
    TestRawMouseButtons();
    TestMediaBridgeCommands();
    TestPresetNames();
    TestWindowGeometryHelpers();
    TestScriptResultDecoding();
    if (failures == 0) {
        std::cout << "All XiaoChuang core tests passed.\n";
        return 0;
    }
    std::cout << failures << " test(s) failed.\n";
    return 1;
}
