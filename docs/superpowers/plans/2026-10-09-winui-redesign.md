# WinUI 3 完整界面重构 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 用 WinUI 3 完整替换小窗浏览器应用界面，保留原有功能和数据，加入比例减淡及焦点内滚轮调窗。

**Architecture:** C++/WinRT WinUI 主窗口承载一个活动 WebView2；管理窗口承载书签、预设及设置，独立沉浸面板不参与网页窗口的穿透与减淡。原窗口、输入、媒体及配置模块经窄接口复用，实际系统状态由 Coordinator 汇总后发布到 UI，失败操作不得先显示成功。

**Tech Stack:** Windows x64、C++20、C++/WinRT、WinUI 3、Windows App SDK 1.8.260921001、MSVC v145、Windows SDK 10.0.26100.0、WebView2。

**Spec:** `docs/superpowers/specs/2026-10-09-winui-redesign.md`；视觉参考 `docs/design/winui-redesign-preview.html`，特别包括已补齐的书签浮出列表。

## Global Constraints

- Windows 首阶段 x64；新工程不依赖 Flutter、Dart、.NET 应用运行时或第二个用于绘制 UI 的 WebView。
- “WinUI 3”是框架，“紧凑 Fluent”是风格；正式主题跟随系统，并支持深／浅色。
- 两种沉浸：鼠标挖孔、移入减淡；最终不透明度为 B × (1 − R/100)。B、R 均为0–100%。R=0持续显示B，R=100移入完全透明。
- 挖孔半径0–4000物理像素，孔内0%、孔外B；两种模式独立记住B。旧参数不能在200% DPI下翻倍。
- 普通模式总是不透明且可操作；沉浸中的调节不重载网页、不暂停视频。主动隐藏单独遵循隐藏／媒体设置。
- Ctrl+Alt+滚轮为建议默认，每格1.05倍；只在普通、非最大化、非隐藏且主窗口／网页有焦点时处理一次。
- 保留旧普通约160×120、沉浸约64×64下限；320×240仅推荐尺寸。布局断点600／390／260 DIP。
- 不实现固定1080P视口、网页自动适宽、新后台多标签引擎或游戏进程注入。
- 保留旧配置、书签、会话、预设和网站数据；正式迁移前备份；测试输出与旧应用数据分开。
- 不继续游戏兼容性检查；办公室SSH仅本项目测试目录；不改安全软件设置，不执行参考项目脚本，不记录连接密码。
- 不推送GitHub；原仓库远程已移除。提交只包含本任务明确文件，不混入此前Flutter／demo的未提交文件。

## Review Focus

1. 快速关窗／重建时晚到的WebView回调不得复活已关闭窗口：Task 3的世代号测试。
2. 中文、重复URL、空集合和超长标题不得破坏书签菜单及预设选择：Task 7的集合／Unicode测试。
3. 只读目录、损坏配置、重复迁移不得覆盖旧数据或报告保存成功：Task 4的文件故障测试。
4. 200% DPI、负坐标显示器、隐藏系统光标不得重复缩放半径或错误减淡：Tasks 5／9的策略与几何测试。
5. 页面焦点、高精度滚轮、输入保护与窗口销毁交错不得吞正常滚动或遗留长按倍速：Tasks 6／10的路由及取消测试。

## 文件结构与共用约定

新增正式工程在 `winui/`，不将demo改名成产品，也不把现有 `Application.cpp` 复制成另一个巨型文件。以下为任务创建的文件；IDL／XAML配套生成文件由构建生成，不手改。

| 路径 | 责任 |
|---|---|
| `winui/MiniWindow.slnx`、`winui/MiniWindow.vcxproj`、`winui/packages.config`、`winui/app.manifest`、`winui/pch.*`、`winui/App.{idl,xaml,xaml.h,xaml.cpp}` | C++ WinUI可构建入口、包版本及生命周期 |
| `winui/MainWindow.{idl,xaml,xaml.h,xaml.cpp}` | 主窗口布局和命令绑定 |
| `winui/core/Ports.h`、`AppCoordinator.{h,cpp}`、`SessionController.{h,cpp}` | 无XAML依赖的状态与命令协调，沿用 `xiaochuang::AppState` |
| `winui/browser/BrowserHost.{h,cpp}`、`WebLifecycle.h`、`PageSignals.{h,cpp}` | WinUI WebView2初始化、导航、媒体与有限网页事件解析 |
| `winui/window/WindowService.{h,cpp}`、`TrayController.{h,cpp}`、`ThemeSync.{h,cpp}` | HWND消息／恢复、托盘与非客户区主题 |
| `winui/input/InputService.{h,cpp}`、`WindowWheelPolicy.{h,cpp}` | 原输入模块适配与焦点内调窗 |
| `winui/settings/VersionedConfigStore.{h,cpp}`、`AtomicFile.{h,cpp}` | 新配置版本、旧配置迁移、原子保存 |
| `winui/management/ManagementWindow.{idl,xaml,xaml.h,xaml.cpp}`、`BookmarkPage.*`、`PresetPage.*`、`SettingsPage.*` | 管理窗口及各页面；每组页面均含idl、xaml、xaml.h、xaml.cpp |
| `winui/immersion/ImmersionPanel.{idl,xaml,xaml.h,xaml.cpp}`、`ImmersionPolicy.{h,cpp}` | 独立沉浸面板与纯不透明度计算 |
| `winui/Resources/Theme.xaml` | 全局语义主题资源；不覆盖系统字体缩放 |
| `winui/tests/WinUICore.Tests.vcxproj`、各`*Tests.cpp`、`TestMain.cpp` | 无GUI控制台策略／数据测试，与原118项测试分开 |
| `tools/windows/build_winui.ps1`、`package_winui.ps1`、`doctor_winui.ps1` | 从现有VS找MSBuild、构建验证及独立测试包 |
| `winui/tests/WindowSmoke.ps1`、`winui/tests/browser-fixture.html`、`docs/validation/winui-redesign.md` | 仅桌面回归入口与如实记录的验收报告 |

共用命名空间 `miniwindow`；原模型仍为 `xiaochuang`。Ports.h统一引入原AppState、AppSettings、WindowMode、ImmersionStyle、Bookmark、Preset、HotkeyBinding、HotkeyAction、HotkeyGesture与MediaCommand类型，不重新声明同名枚举。

错误统一：`OperationResult { bool ok; std::wstring message; }`。`AppSnapshot { uint64_t revision; AppState state; bool browserReady; bool loading; std::wstring error; std::vector<std::wstring> hotkeyErrors; }`。UI只读快照，不直接改原生状态。

构建命令均在办公室源码快照根目录的PowerShell中执行，直接调用脚本；SSH下不额外启动第二个powershell.exe。新增构建脚本输出固定标志 `WINUI_BUILD_OK`、`WINUI_CORE_TESTS_OK`、`LEGACY_CORE_TESTS_OK`，失败退出非0。后文引用这些命令即使用同一约定。

## Task 1：独立C++ WinUI工程与可重复构建

**Files:** 新建上述 `winui/` 入口、空 `MainWindow`、`Resources/Theme.xaml`；创建 `tools/windows/{doctor_winui,build_winui}.ps1`；更新 `.gitignore` 仅增加新工程产物规则。

**Interfaces:** 产生 `build_winui.ps1 -Configuration Release -Platform x64 [-TestsOnly] [-OutputRoot <path>]`。`-TestsOnly`从Task 2起构建／执行新核心测试；脚本仍提供原核心测试执行标志。`App::OnLaunched(...)`创建一个主窗口；此任务不自动启动GUI。

- [ ] 在隔离执行工作区开始前读 using-git-worktrees 技能；保存当前未提交设计作为基线，确保worktree包含已审阅设计，不能从远程旧主分支丢掉它。
- [ ] 建立C++ WinUI空工程，固定 WindowsAppSDK=1.8.260921001、CppWinRT=2.0.240405.15、SDK.BuildTools=10.0.26100.7705、WIL=1.0.250325.1；使用PackageReference或packages.config中的一种，正式工程统一采用后者。编译时如版本不兼容，记录明确构建证据后调整，不为追新而升级。
- [ ] 设置C++20、v145、SDK26100、最低Windows版本10.0.19041.0、UTF-8；`WindowsPackageType=None`、`WindowsAppSDKSelfContained=true`，携带完整输出目录，不宣称单EXE。沿用原发布权限策略，桌面自动测试使用 `SmokeTestAsInvoker=true`；不要自动修改原应用提权策略。
- [ ] 实现环境脚本：查询VS2026原生编译和 `Microsoft.VisualStudio.Component.WindowsAppSdkSupport.Cpp`，从注册表查SDK路径，检查WebView2 Runtime；失败指出缺项。保留旧Flutter doctor脚本但不调用。
- [ ] 用 `./tools/windows/build_winui.ps1 -Configuration Release -Platform x64` 完成NuGet还原及干净构建；缺包、编译或复制失败均退出非0。验收：`WINUI_BUILD_OK`和独立可执行文件，原工程文件不变。
- [ ] 提交上述明确新工程和脚本文件：`build: add C++ WinUI application baseline`。

## Task 2：状态协调、标签会话与可替换端口

**Files:** 创建 `winui/core/*`、`winui/tests/{WinUICore.Tests.vcxproj,TestMain.cpp,SessionTests.cpp,CoordinatorTests.cpp}`；更新新工程和构建脚本，不重写旧UI。

**Interfaces:** `IBrowserPort { virtual OperationResult Navigate(const std::wstring&)=0; virtual void SetVisible(bool)=0; virtual void ExecuteMedia(MediaCommand,int)=0; virtual void Close()=0; }`；`IWindowPort { virtual OperationResult SetMode(WindowMode)=0; virtual OperationResult SetHidden(bool)=0; virtual OperationResult RestoreNormal()=0; virtual RECT NormalRect() const=0; }`；`ISettingsPort { virtual OperationResult Save(const AppState&)=0; }`。`AppCoordinator(IBrowserPort&,IWindowPort&,ISettingsPort&)`，`const AppSnapshot& Snapshot() const`，`void Subscribe(std::function<void(const AppSnapshot&)>)`，`void Navigate(std::wstring)`，`void ToggleImmersion()`，`void RestoreNormal()`，`void ToggleHidden()`，`void OnBrowserState(bool ready,bool loading,std::wstring error)`。

`SessionController(AppState&)`提供 `AddTab(std::wstring)`、`SelectTab(size_t)`、`CloseTab(size_t)`、`CloseOthers(size_t)`、`CloseAll()`；调用后由Coordinator发布新快照。

- [ ] 先写测试：最后一页关闭后 `tabs.size()==1 && tabs[0].url==homeUrl`；越界选择无副作用；关闭当前前面的页保持原活动页；窗口端口返回失败时快照mode不改变且error非空；每次成功发布revision严格增加。
- [ ] `./tools/windows/build_winui.ps1 -TestsOnly` 必须因缺实现失败，再实现上述接口和最小协调类；网址解析复用 `NormalizeInputUrl`，不向网页暴露窗口命令。
- [ ] 重跑新测试及原 `tests/XiaoChuang.Tests.vcxproj`。预期新用例全过，原118项保持通过；AppCoordinator有界订阅，销毁前解除UI引用。
- [ ] 提交：`refactor: add WinUI state coordinator and session ports`。

## Task 3：WebView2宿主、媒体与关闭后的回调保护

**Files:** 创建 `winui/browser/*`、`winui/tests/{WebLifecycleTests.cpp,PageSignalsTests.cpp,browser-fixture.html}`；更新 `MainWindow.*` 与 `App.*`。

**Interfaces:** `BrowserHost(WebView2 const&,std::filesystem::path profileDirectory,std::function<void(bool,bool,std::wstring)> status)`实现IBrowserPort；`winrt::Windows::Foundation::IAsyncAction InitializeAsync()`；`void SetFixedZoom(bool enabled,int percent)`；`void SetPageSignalHandler(std::function<void(PageSignal)>)`。

`WebLifecycle { uint64_t Begin(); bool IsCurrent(uint64_t) const; void Close(); }`。`PageSignal { enum Kind { Typing,Fullscreen,VideoAspect }; Kind kind; bool flag; double aspect; }`，`std::optional<PageSignal> ParsePageSignal(const std::wstring&)`。

- [ ] 先测：Begin后Close使旧token无效；重建后旧token不能发布ready；非法JSON／无限aspect／未允许kind返回nullopt；合法typing/fullscreen消息正确解析。运行TestsOnly见失败。
- [ ] 使用WinUI XAML内置WebView2，所有调用在UI线程异步等待，不阻塞STA。通过弱引用＋generation保护初始化／导航／诊断回调，撤销全部事件后Close。启动检测Runtime缺失并呈现可恢复错误，不静默安装。
- [ ] 从旧 `InitializeWebView/ConfigureWebViewEvents/HandleWebMessage/ExecuteMediaAction`按职责迁移；复用 `MediaBridge` 脚本和 `WebViewZoom.h`。保留媒体站点适配、SPA／子框架处理、网页全屏限制及隐藏可见渲染策略；一份活动WebView，不创建每标签实例。
- [ ] 自有profile路径来自宿主，测试版固定在测试包data目录；旧profile选择留给Task 4。导航／标题回写到当前会话，不把用户输入拼接成脚本。关闭时取消长按回调接口交Task 6。
- [ ] 跑TestsOnly和干净构建；桌面fixture仅验证初始化、导航、播放信号、固定缩放与关窗恢复，不启动游戏。记录尚未人工运行项。
- [ ] 提交：`feat: host WebView2 with guarded lifecycle and media bridge`。

## Task 4：版本化配置与旧数据一次性迁移

**Files:** 创建 `winui/settings/*`、`winui/tests/{ConfigMigrationTests.cpp,AtomicFileTests.cpp,fixtures/legacy-config.ini}`；修改 `src/AppModel.h` 仅增加兼容字段；新应用接入该Store，旧版不切换Store。

**Interfaces:** 新字段 `AppSettings::hoverReductionPercent=100`、`windowWheelEnabled=true`、`windowWheelModifiers=MOD_CONTROL|MOD_ALT`，Preset同名字段。`VersionedConfigStore(path newPath,path legacyPath)`实现ISettingsPort，`OperationResult LoadOrMigrate(AppState&)`。`OperationResult WriteAtomically(const path&,const std::wstring&)`，`OperationResult BackupLegacy(const path&)`。

新文件 `config.winui.ini`，`[Meta] SchemaVersion=1`；新字段专用键，原字段与书签／标签编码兼容。旧 `config.ini` 原地只读，备份为不存在的 `config.legacy-backup-<timestamp>.ini`，禁止覆盖既有备份。

- [ ] 先测：中文书签、热键启用位及全预设字段保留；旧整体B=80迁移为B=80/R=100；旧hole有效B=100；半径400在DPI192仍存400；再次启动不重复迁移；源损坏／备份失败／写入失败时旧文件逐字节不变，结果ok=false。
- [ ] 扩展模型Clamp；迁移使用旧ConfigStore读取后严格校验，错误不静默改为成功。实现同目录临时文件→检查写入／刷新→原子替换，清理失败临时文件，不复用旧Save中忽略返回值的写法。
- [ ] 网站数据不复制进源码。仅选择旧profile路径且旧进程已退出时复用；检测占用时给出关闭旧实例／使用独立测试数据的选择，避免双实例写同一profile。
- [ ] 跑新配置测试和原118项；注入只读／失败写入测试应可重复，不靠管理员目录权限偶然失败。
- [ ] 提交：`feat: migrate legacy settings with backup and atomic saves`。

## Task 5：完整窗口模式、托盘与标题栏恢复

**Files:** 创建 `winui/window/*`、`winui/tests/{WindowAdapterTests.cpp,ThemeStateTests.cpp,WindowSmoke.ps1}`；修改 `src/WindowModeController.{h,cpp}`仅必要的宿主适配钩子。

**Interfaces:** `WindowService(HWND,AppSettings&,std::function<void(bool chromeVisible)>,std::function<void()>)`实现IWindowPort；`void HandleMessage(UINT,WPARAM,LPARAM)`转接原控制器；`void RefreshTheme(ThemeMode)`。`TrayController(HWND,std::function<void(TrayAction)>)`，`void Sync(bool immersion,bool hidden,bool minimizeToTray)`；`TrayAction={Show,RestoreNormal,ImmersionControls,Settings,Exit}`。

`ThemeSync::Apply(HWND,ThemeMode)`统一Root.RequestedTheme与AppWindow.TitleBar／非客户区颜色；不能复制demo只恢复边框的处理。

- [ ] 测试：SetMode失败完整回到先前mode／几何；B=0恢复后alpha=255且无孔／无穿透／无NoActivate；控制面板关窗不改变浏览器mode；沉浸或hidden时即便minimizeToTray=false仍有托盘恢复；主题切换及恢复后Chrome与caption使用同一theme状态。
- [ ] 封装 `WindowModeController` HWND消息路由、无缝最大化、置顶、不抢焦点、吸附、网页全屏比例和几何恢复；原 `visibleMode_` 与hidden继续分开。WinUI尺寸变化更新实际chrome高度；DPI转换恰好一次，复用现有负坐标／边缘锚点函数。
- [ ] 初期窗口API通过可注入函数表在测试中模拟失败，正式使用Win32实现。任何恢复步骤失败仍尝试后续步骤并报告实际错误，不隐藏恢复入口。
- [ ] 新桌面Smoke脚本只对本次EXE的明确PID／HWND操作，验证普通→最大化→沉浸→恢复和alpha／样式／主题状态；禁止按进程名称批量结束。视觉标题栏仍需用户人工确认，不能用属性读回代替颜色验收。
- [ ] 构建／原核心／新测试全过；记录200% DPI标题栏人工项为未执行，待用户体验。提交：`feat: restore window modes tray and themed caption`。

## Task 6：热键、输入保护与长按媒体操作迁移

**Files:** 创建 `winui/input/InputService.{h,cpp}`、`winui/tests/{InputRoutingTests.cpp,HoldCancelTests.cpp}`；修改 `src/InputGuard.{h,cpp}`添加必要的WinUI文本焦点入口，保留旧调用。

**Interfaces:** `InputService(HWND,AppState&,std::function<void(HotkeyAction,HotkeyGesture)>)`，`void Start()`、`void Stop()`、`void OnMessage(UINT,WPARAM,LPARAM)`、`void SetOwnedTextFocus(bool)`、`void OnPageTyping(bool)`、`void CancelGesture()`、`void BeginCapture(HotkeyAction)`、`void CancelCapture()`；`InputGuard::SetOwnedTextFocus(bool)`补充XAML输入控件无独立Edit HWND的情况。

- [ ] 先测：XAML地址输入／管理表单焦点抑制热键，离开后恢复；网页typing仅应用前台时抑制；外部过期UIA结果丢弃；400ms长按阈值沿用旧策略；失焦、导航、隐藏／关闭、捕获取消均触发StopHold且只触发一次。
- [ ] 复用原 `HotkeyManager`、Raw Input鼠标侧键、键盘后备及 `InputGuard`工作线程；挂接XAML GotFocus/LostFocus，并对主窗口、管理和面板焦点分别识别。保留全部7动作和每项enabled；注册错误直接进入快照，键位冲突不覆盖旧有效绑定。
- [ ] 与BrowserHost接入导航／切页取消，向 `MediaBridge::CommandScript`传原动作及2–5倍速；所有UI更新经DispatcherQueue。
- [ ] 新测试和旧118项全过；fixture验证文字输入保护，明确不测试游戏。提交：`feat: adapt hotkeys typing guard and media gestures to WinUI`。

## Task 7：紧凑主界面、书签快捷列表与完整管理窗口

**Files:** 完成 `MainWindow.*`、`Resources/Theme.xaml`，创建 `winui/management/ManagementWindow.*`、`BookmarkPage.*`、`PresetPage.*`、`winui/tests/{BookmarkTests.cpp,PresetTests.cpp,LayoutPolicyTests.cpp}`。

**Interfaces:** Coordinator增加 `void OpenBookmark(size_t)`、`OperationResult SaveBookmark(std::optional<size_t>,Bookmark)`、`OperationResult DeleteBookmark(size_t)`、`OperationResult SaveCurrentPreset(std::wstring)`、`OperationResult RenamePreset(size_t,std::wstring)`、`OperationResult ApplyPreset(size_t)`、`OperationResult DeletePreset(size_t)`。`ManagementWindow::Open(ManagementPage)`，`ManagementPage={Bookmarks,Presets,Settings}`；复用一个窗口。`ChromeLayout CalculateChromeLayout(double widthDip)`，返回 `Full/Compact/Small/Minimal`。

- [ ] 先测：标题为“视频🚀／你好”、超长标题与同URL不同标题可正常保存并显示；空集合禁用打开／删除；预设trim后空名／重名拒绝；ApplyPreset替换会话但保留书签和全局主题／渲染；宽度600/599、390/389、260/259落入正确布局。
- [ ] 主界面按稿实现原生TabView／地址TextBox／按钮／菜单；使用系统字形与AutomationProperties.Name。后退、刷新／停止和网页加载状态由BrowserHost真实能力驱动。实现所有标签关闭菜单，最小布局仍可新建／切换／关闭标签。
- [ ] 书签按钮使用原生Flyout／列表，单击书签当前标签打开，底部“管理书签…”才开管理窗口；窄窗“更多→书签”复用同一列表。列表空状态、长标题截断提示、Esc／方向键遵循原生行为。
- [ ] 完成书签新增／编辑／删除、预设概要／保存／重命名／应用／删除；破坏操作先确认。编辑草稿切换或关窗时处理未保存更改；保存错误保留草稿并显示消息。管理窗口在普通模式外独立不透明，不跟随小窗穿透。
- [ ] 跑TestsOnly及桌面布局检查，160/240/360/560/820 DIP无操作入口丢失；100%／200% DPI不对网页施加自动适宽。提交：`feat: rebuild browser chrome bookmarks and preset management`。

## Task 8：六类设置、诊断与应用状态反馈

**Files:** 创建 `winui/management/SettingsPage.*`、`winui/tests/{SettingsValidationTests.cpp,SettingsDependencyTests.cpp}`；接入 `src/Diagnostics.*` 与新Store。

**Interfaces:** Coordinator增加 `OperationResult UpdateSettings(const AppSettings&)`、`OperationResult UpdateHotkey(HotkeyAction,const HotkeyBinding&)`、`OperationResult ResetDefaults()`、`void RequestDiagnostics(std::function<void(std::wstring)>)`。SettingsPage将编辑草稿校验后提交，快照订阅只回写非编辑字段，避免每次tick覆盖输入。

- [ ] 先测：fixed=false禁止写入无效zoom但保留已存zoom；25/500合法，24/501返回字段错误；fit=false禁用lock但保留值；空主页拒绝且草稿保留；render变化仅标记pendingRestart；ResetDefaults保留标签／书签。
- [ ] 按规格六类逐项迁移，不漏主页、托盘、隐藏暂停、输入保护、后台媒体、倍速、视频全屏比例、最大化拖动、固定缩放、渲染和主题；快捷键捕获支持Esc取消与独立启用。一般值即时作用、保存节流；错误不发成功通知。
- [ ] 诊断继续使用真实Windows／HAGS／Runtime／媒体帧结果，异步失败可见；关于保留版本、上游署名／链接和GPL-3.0。渲染模式显示重启提示，不自动重启或刷新网页。
- [ ] 跑新／旧测试并逐项对照规格设置表。提交：`feat: migrate settings diagnostics and about UI`。

## Task 9：比例减淡与独立沉浸控制面板

**Files:** 创建 `winui/immersion/*`、`winui/tests/{ImmersionPolicyTests.cpp,ImmersionPanelStateTests.cpp}`；修改WindowService及原窗口控制器的alpha计算入口。

**Interfaces:** `double CalculateImmersionOpacity(WindowMode,int basis,int reduction,bool cursorInside)`；`BYTE ToWindowAlpha(double)`仅最终取整；`ImmersionPanel::Open()`，`void ClosePanel()`；Coordinator增加 `OperationResult UpdateImmersion(ImmersionStyle,int basis,int reduction,int radiusPhysicalPixels)`。鼠标触发按坐标判断，不再要求 `CURSOR_SHOWING`；锁定鼠标的体验留待后续游戏测试，不伪装已解决。

- [ ] 先测：Normal始终100；B80/R50/inside=true→40，outside→80；R0→80，R100→0；Hole孔外B、孔内区域透明；B0时仍可RestoreNormal；alpha只算一次：40%→102。光标可见／隐藏不改变坐标内的减淡结果。
- [ ] 模式分别记住B，参数0–100／半径0–4000校验；挖孔继续只在光标／窗口／半径变化时更新区域并沿用约20Hz上限。半径使用物理像素，输入DPI不再次乘2。
- [ ] 独立WinUI非模态面板，滑块＋数值输入与实时结果；没有debug开关或“模拟鼠标”产品控件。面板打开不暂停浏览器鼠标判断；面板区域不计入浏览器范围；关面板不退出沉浸。恢复按钮／托盘在B0情况下正常。
- [ ] 原生API失败回滚普通可操作状态、发布错误；拖滑块不导航／不重建／不发媒体暂停。跑新／旧测试与桌面fixture，记录游戏尚未测。提交：`feat: add proportional dimming and live immersion controls`。

## Task 10：焦点内组合键＋滚轮调窗

**Files:** 创建 `winui/input/WindowWheelPolicy.{h,cpp}`、`winui/tests/{WindowWheelPolicyTests.cpp,WheelRoutingTests.cpp}`；修改InputService、WindowService及设置页。

**Interfaces:** `WheelContext { WindowMode mode; bool hidden,maximized,mainOrWebFocused,managementFocused; UINT modifiers; }`；`bool ShouldResizeWindow(const WheelContext&,UINT configuredModifiers)`；`WindowWheelAccumulator::Add(int delta)`返回完整刻度数并保留余量；`RECT ResizeWindowBySteps(RECT current,int steps,RECT workArea,SIZE minimum)`；`bool InputService::TryHandleWheel(int delta,UINT modifiers)`返回true表示已消费。

- [ ] 先测：普通＋主／网页焦点＋CtrlAlt=true；沉浸／最大化／隐藏／管理焦点／组合不符均false；delta30四次产生1格，正负余量抵消；steps+1再−1允许仅1px舍入误差；负坐标workArea内结果可见且不低于minimum。
- [ ] 实现每格乘／除1.05、保持外框比例、左上角锚定和工作区边界约束；不改页面zoom。先用WinUI PointerWheelChanged的handledEventsToo路由覆盖XAML与网页控件，再仅对本进程所属子窗口的WM_MOUSEWHEEL补充线程消息过滤；由统一TryHandleWheel入口消费，避免两个路径重复处理。检查真实前台／焦点归属，只拦截已消费组合；不安装全系统鼠标移动钩子。如果桌面fixture证明任一路由覆盖不到网页，先修正宿主输入路由再交付，不把只有工具栏可用算通过。
- [ ] 设置支持选择Ctrl／Alt／Shift／Win修饰组合，不允许空组合，默认Ctrl+Alt；兼容预设保存。退出普通／失焦／关窗清空滚轮余量，防止稍后跳动。
- [ ] 跑策略测试和桌面fixture：地址栏、网页正文、网页输入焦点都可调窗；管理输入不触发；普通滚轮仍滚网页；每个事件仅缩放一次。提交：`feat: resize focused window with modifier and wheel`。

## Task 11：独立测试包、完整回归与交付说明

**Files:** 创建 `tools/windows/package_winui.ps1`、`docs/validation/winui-redesign.md`；按需更新 `tools/sync_windows.py`及其测试，只补新工程必要排除规则。

**Interfaces:** `package_winui.ps1 -SourceDirectory <path> -OutputRoot <path>`先调用构建／测试，再输出 `WinUI-<timestamp>/`与zip、manifest；标志 `WINUI_PACKAGE_EXE`、`WINUI_PACKAGE_ZIP`。manifest含版本、源码摘要、依赖版本、文件SHA256和人工验收状态。

- [ ] 构建Release并跑原118项与全部新增测试；失败不生成“通过”包。不为文案／可逆样式写镜像测试，逻辑测试只验证真实策略／状态／失败路径。
- [ ] 只同步源码清单到 `D:\Projects\Mini-Window-Browser\sources\<id>`；排除.git、work、配置、Cookies、日志、bin/obj、凭据。没有执行另一个项目sync脚本或覆盖旧EXE。
- [ ] 输出包含WinUI自包含运行库、VC运行库依赖及许可证；先检查所有必需文件和manifest，再zip。WebView2 Runtime使用系统安装，缺失时给出明确错误，不自动下载。
- [ ] 桌面Smoke只运行本测试EXE／fixture，结束前恢复窗口与释放输入。记录普通浏览、标题栏恢复、深浅色、DPI、配置迁移、预设／书签、沉浸面板和滚轮各项的实际状态。
- [ ] 提供用户启动入口，用户在办公室手动体验。**游戏／独占全屏兼容性继续标未执行，不作为本轮交付宣称通过项。** 不添加自动测试游戏或重启电脑动作。
- [ ] 进行整分支代码复查；只有用户选择允许的执行方式后才按该方式调度审阅代理。提交：`build: package and document WinUI redesign validation`。

## 计划自审与执行交接

- 规格覆盖：主界面／菜单／书签列表Task 7；管理与原设置Tasks 7–8；浏览与媒体Tasks 3/6；沉浸／恢复Tasks 5/9；滚轮Task 10；数据Task 4；环境和交付Tasks 1/11。
- 五个Review Focus均映射到具体失败测试。原118项是基线，新增策略用独立测试项目；不把旧测试通过当作新窗口视觉通过。
- 端口与各任务签名统一；外部网页消息仅能表达typing/fullscreen/aspect，不操作配置和系统窗口；C#／Flutter原型不进入生产依赖。
- 本计划尚未执行。建议在当前会话由主代理逐项实现，原因是窗口生命周期、输入及网页宿主接口联系紧密，连续实现更容易及时修正接口；用户也可选分任务子代理执行。
- 审阅本计划并选择执行方式后，再进入 implementation 技能；本次不启动正式代码迁移。

技术依据（用于实现时核对API，不替代本项目测试）：[WinUI 3 WebView2](https://learn.microsoft.com/en-us/windows/apps/develop/ui/controls/webview2)、[Windows App SDK自包含部署](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/self-contained-deploy/deploy-self-contained-apps)、[CppWinRT 2.0.240405.15包](https://www.nuget.org/packages/Microsoft.Windows.CppWinRT/2.0.240405.15)。
