#include "pch.h"
#include "SettingsPage.xaml.h"
#include "SettingsPage.g.cpp"
#include "UIHelpers.h"
#include "../settings/SettingsPolicy.h"
#include "../../src/ConfigStore.h"
#include "../../src/Diagnostics.h"
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Windows.System.h>
#include <cwchar>
#include <cmath>
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
namespace winrt::MiniWindow::implementation {
namespace {
void AccessibleName(DependencyObject const& control,const std::wstring& name){Automation::AutomationProperties::SetName(control,name);}
Button Action(const std::wstring& name){Button button;button.Content(box_value(name));AccessibleName(button,name);return button;}
}
SettingsPage::SettingsPage(){InitializeComponent();}
void SettingsPage::Boolean(Panel panel,const std::wstring& name,bool miniwindow::AppSettings::*member){
 CheckBox box;box.Content(box_value(name));AccessibleName(box,name);panel.Children().Append(box);
 fields_[name]={box,[box,member](auto& settings){settings.*member=box.IsChecked().Value();return miniwindow::OperationResult{};},[box,member](const auto& settings){box.IsChecked(settings.*member);}};
 box.Checked([this,name](auto&&,auto&&){Changed(name,true);});box.Unchecked([this,name](auto&&,auto&&){Changed(name,true);});
}
void SettingsPage::Number(Panel panel,const std::wstring& name,int miniwindow::AppSettings::*member,int low,int high){
 TextBox box;box.Header(box_value(name));AccessibleName(box,name);box.PlaceholderText(std::to_wstring(low)+L"–"+std::to_wstring(high));panel.Children().Append(box);
 fields_[name]={box,[box,member,low,high,name](auto& settings){auto text=std::wstring(box.Text());wchar_t* end=nullptr;const auto number=wcstol(text.c_str(),&end,10);if(text.empty()||end==text.c_str()||*end||number<low||number>high)return miniwindow::OperationResult{false,name+L"须为 "+std::to_wstring(low)+L"–"+std::to_wstring(high)+L" 的整数。"};settings.*member=static_cast<int>(number);return miniwindow::OperationResult{};},[box,member](const auto& settings){box.Text(std::to_wstring(settings.*member));}};
 box.TextChanged([this,name](auto&&,auto&&){Changed(name,false);});box.GotFocus([this,name](auto&&,auto&&){editing_=name;});box.LostFocus([this,name](auto&&,auto&&){editing_.clear();if(dirty_.contains(name))Commit(name);});
}
void SettingsPage::Choice(Panel panel,const std::wstring& name,std::vector<std::wstring> options,std::function<int(const miniwindow::AppSettings&)> get,std::function<void(miniwindow::AppSettings&,int)> set){
 ComboBox box;box.Header(box_value(name));box.HorizontalAlignment(HorizontalAlignment::Stretch);AccessibleName(box,name);for(const auto& item:options)box.Items().Append(box_value(item));panel.Children().Append(box);
 fields_[name]={box,[box,set](auto& settings){set(settings,box.SelectedIndex());return miniwindow::OperationResult{};},[box,get](const auto& settings){box.SelectedIndex(get(settings));}};box.SelectionChanged([this,name](auto&&,auto&&){Changed(name,true);});
}
void SettingsPage::Bind(miniwindow::AppCoordinator& app,std::function<void(miniwindow::HotkeyAction)> capture,std::function<void()> panel){
 app_=&app;syncing_=true;
 for(const auto& category:{L"窗口与行为",L"沉浸",L"网页与媒体",L"快捷键",L"外观",L"诊断与关于"}){Categories().Items().Append(box_value(category));Panel section;section.Spacing(12);section.Visibility(Visibility::Collapsed);Sections().Children().Append(section);panels_.push_back(section);}
 Categories().SelectionChanged([this](auto&&,auto&&){for(size_t i=0;i<panels_.size();++i)panels_[i].Visibility(Categories().SelectedIndex()==static_cast<int>(i)?Visibility::Visible:Visibility::Collapsed);});Categories().SelectedIndex(0);
 using S=miniwindow::AppSettings;
 Boolean(panels_[0],L"最小化到托盘",&S::useSystemTray);Number(panels_[0],L"边缘吸附距离",&S::snapThreshold,0,200);Boolean(panels_[0],L"最大化顶部拖动",&S::maximizedTopDragEnabled);Boolean(panels_[0],L"组合键＋滚轮调整窗口大小",&S::windowWheelEnabled);
 panels_[0].Children().Append(miniwindow::ui::Text(L"普通窗口有焦点时，每格调整 5%；组合键在“快捷键”中选择。"));
 Choice(panels_[1],L"沉浸模式",{L"鼠标挖孔",L"移入减淡"},[](const S& s){return static_cast<int>(s.immersionStyle);},[](S& s,int n){s.immersionStyle=static_cast<miniwindow::ImmersionStyle>(n);});
 Number(panels_[1],L"挖孔模式基础不透明度 (%)",&S::holeOpacityPercent,0,100);Number(panels_[1],L"减淡模式基础不透明度 (%)",&S::autoHideOpacityPercent,0,100);Number(panels_[1],L"移入额外减淡比例 (%)",&S::hoverReductionPercent,0,100);Number(panels_[1],L"挖孔半径（物理像素）",&S::holeRadius,0,4000);
 panels_[1].Children().Append(miniwindow::ui::Text(L"移入时显示不透明度 = 基础不透明度 × (1 − 减淡比例)。普通模式始终不透明。"));auto open=Action(L"打开沉浸控制面板");open.IsEnabled(static_cast<bool>(panel));open.Click([panel](auto&&,auto&&){if(panel)panel();});panels_[1].Children().Append(open);auto restore=Action(L"恢复普通模式");restore.Click([this](auto&&,auto&&){app_->RestoreNormal();});panels_[1].Children().Append(restore);
 TextBox home;home.Header(box_value(L"主页"));AccessibleName(home,L"主页");panels_[2].Children().Append(home);fields_[L"主页"]={home,[home](S& s){s.homeUrl=std::wstring(home.Text());return miniwindow::OperationResult{};},[home](const S& s){home.Text(s.homeUrl);}};home.TextChanged([this](auto&&,auto&&){Changed(L"主页",false);});home.GotFocus([this](auto&&,auto&&){editing_=L"主页";});home.LostFocus([this](auto&&,auto&&){editing_.clear();if(dirty_.contains(L"主页"))Commit(L"主页");});
 Boolean(panels_[2],L"固定网页缩放",&S::fixedWebZoomEnabled);Number(panels_[2],L"网页缩放 (%)",&S::fixedWebZoomPercent,25,500);Boolean(panels_[2],L"视频全屏自动按比例调窗",&S::autoFitVideoFullscreen);Boolean(panels_[2],L"锁定视频全屏比例",&S::lockVideoFullscreenAspect);Boolean(panels_[2],L"隐藏时自动暂停",&S::autoPauseOnHide);Boolean(panels_[2],L"隐藏后允许媒体快捷键",&S::backgroundMediaHotkeys);Number(panels_[2],L"长按快进倍速",&S::holdPlaybackRate,2,5);
 Boolean(panels_[3],L"输入文字时暂停快捷键",&S::disableHotkeysOnTyping);
 Panel modifiers;modifiers.Orientation(Orientation::Horizontal);modifiers.Spacing(8);panels_[3].Children().Append(miniwindow::ui::Text(L"调整窗口大小的滚轮组合键"));panels_[3].Children().Append(modifiers);
 for(const auto& entry:std::vector<std::pair<std::wstring,UINT>>{{L"Ctrl",MOD_CONTROL},{L"Alt",MOD_ALT},{L"Shift",MOD_SHIFT},{L"Win",MOD_WIN}}){CheckBox box;box.Content(box_value(entry.first));AccessibleName(box,L"滚轮组合键 "+entry.first);modifiers.Children().Append(box);auto key=L"滚轮组合键 "+entry.first;const auto bit=entry.second;fields_[key]={box,[box,bit](S& s){if(box.IsChecked().Value())s.windowWheelModifiers|=bit;else s.windowWheelModifiers&=~bit;return miniwindow::OperationResult{};},[box,bit](const S& s){box.IsChecked((s.windowWheelModifiers&bit)!=0);}};box.Checked([this,key](auto&&,auto&&){Changed(key,true);});box.Unchecked([this,key](auto&&,auto&&){Changed(key,true);});}
 for(size_t i=0;i<xiaochuang::kHotkeyCount;++i){Panel row;row.Orientation(Orientation::Horizontal);row.Spacing(8);auto label=app.Snapshot().state.hotkeys[i].displayName;CheckBox enabled;enabled.Content(box_value(label));AccessibleName(enabled,label+L" 启用");enabled_[i]=enabled;auto button=Action(label+L" 修改快捷键");bindings_[i]=button;row.Children().Append(enabled);row.Children().Append(button);panels_[3].Children().Append(row);auto apply=[this,i,enabled](auto&&,auto&&){if(syncing_)return;auto value=app_->Snapshot().state.hotkeys[i];value.enabled=enabled.IsChecked().Value();auto result=app_->UpdateHotkey(static_cast<miniwindow::HotkeyAction>(i),value);if(!result.ok)Feedback().Text(result.message);Refresh();};enabled.Checked(apply);enabled.Unchecked(apply);button.Click([capture,i](auto&&,auto&&){capture(static_cast<miniwindow::HotkeyAction>(i));});}
 hotkeyStatus_=TextBlock();hotkeyStatus_.TextWrapping(TextWrapping::Wrap);AccessibleName(hotkeyStatus_,L"快捷键状态");panels_[3].Children().Append(hotkeyStatus_);
 Choice(panels_[4],L"主题",{L"跟随系统",L"浅色",L"深色"},[](const S& s){return static_cast<int>(s.themeMode);},[](S& s,int n){s.themeMode=static_cast<xiaochuang::ThemeMode>(n);});panels_[4].Children().Append(miniwindow::ui::Text(L"主窗口、管理窗口和沉浸控制面板同步外观及标题栏。"));
 Choice(panels_[5],L"网页渲染模式",{L"自动 GPU",L"软件兼容"},[](const S& s){return static_cast<int>(s.renderMode);},[](S& s,int n){s.renderMode=static_cast<xiaochuang::RenderMode>(n);});restart_=TextBlock();restart_.TextWrapping(TextWrapping::Wrap);AccessibleName(restart_,L"重启提示");panels_[5].Children().Append(restart_);
 auto diagnose=Action(L"获取诊断");diagnose.Click([this](auto&&,auto&&){Diagnose();});panels_[5].Children().Append(diagnose);diagnostic_=TextBox();diagnostic_.AcceptsReturn(true);diagnostic_.IsReadOnly(true);diagnostic_.TextWrapping(TextWrapping::Wrap);diagnostic_.Height(200);AccessibleName(diagnostic_,L"诊断结果");panels_[5].Children().Append(diagnostic_);
 auto copy=Action(L"复制诊断");copy.Click([this](auto&&,auto&&){const auto value=std::wstring(diagnostic_.Text());if(value.empty()){Feedback().Text(L"请先获取诊断。");return;}Feedback().Text(xiaochuang::CopyTextToClipboard(nullptr,value)?L"诊断已复制。":L"复制失败，请重试。");});panels_[5].Children().Append(copy);auto reset=Action(L"恢复默认设置…");reset.Click([this](auto&&,auto&&){ResetAsync();});panels_[5].Children().Append(reset);
 panels_[5].Children().Append(miniwindow::ui::Text(L"Mini Window Browser 1.4.4 · WinUI 3 重构版\n原项目：azurplain / Mini-Window-Browser\n许可证：GPL-3.0（随应用提供 LICENSE）"));HyperlinkButton upstream;upstream.Content(box_value(L"查看原项目"));upstream.NavigateUri(Windows::Foundation::Uri(L"https://github.com/azurplain/Mini-Window-Browser"));panels_[5].Children().Append(upstream);
 syncing_=false;Refresh();
}
void SettingsPage::Changed(const std::wstring& name,bool immediate){if(syncing_||!app_)return;dirty_.insert(name);if(immediate)Commit(name);}
bool SettingsPage::Commit(std::optional<std::wstring> only){
 auto draft=app_->Snapshot().state.settings;for(const auto& key:dirty_){if(only&&key!=*only)continue;auto result=fields_.at(key).read(draft);if(!result.ok){Feedback().Text(result.message);return false;}}
 auto result=app_->UpdateSettings(draft);if(!result.ok){Feedback().Text(result.message);return false;}if(only)dirty_.erase(*only);else dirty_.clear();Feedback().Text(L"设置已保存。");Refresh();return true;
}
void SettingsPage::Dependencies(){const auto deps=miniwindow::SettingsDependencies(app_->Snapshot().state.settings);fields_.at(L"网页缩放 (%)").control.as<Control>().IsEnabled(deps.zoomEnabled);fields_.at(L"锁定视频全屏比例").control.as<Control>().IsEnabled(deps.lockAspectEnabled);}
void SettingsPage::Refresh(){if(!app_)return;syncing_=true;const auto& snapshot=app_->Snapshot();if(!snapshot.error.empty())Feedback().Text(snapshot.error);for(const auto& [key,field]:fields_)if(!dirty_.contains(key)&&editing_!=key)field.write(snapshot.state.settings);for(size_t i=0;i<enabled_.size();++i){const auto& value=snapshot.state.hotkeys[i];enabled_[i].IsChecked(value.enabled);bindings_[i].Content(box_value(xiaochuang::HotkeyDisplayText(value)));}auto status=snapshot.captureStatus;for(const auto& error:snapshot.hotkeyErrors)status+=L"\n注册失败："+error;hotkeyStatus_.Text(status);restart_.Text(snapshot.pendingRestart?L"渲染模式将在下次启动生效，当前网页继续播放。":L"当前渲染模式已生效。");Dependencies();syncing_=false;}
Windows::Foundation::IAsyncOperation<bool> SettingsPage::CanLeaveAsync(){auto lifetime=get_strong();if(!Dirty())co_return true;ContentDialog dialog;dialog.XamlRoot(XamlRoot());dialog.Title(box_value(L"设置尚未保存"));dialog.Content(box_value(L"请检查输入值，或放弃尚未保存的修改。"));dialog.PrimaryButtonText(L"保存并继续");dialog.SecondaryButtonText(L"放弃修改");dialog.CloseButtonText(L"继续编辑");auto result=co_await dialog.ShowAsync();if(result==ContentDialogResult::Primary)co_return Commit({});if(result==ContentDialogResult::Secondary){dirty_.clear();editing_.clear();Refresh();co_return true;}co_return false;}
Windows::Foundation::IAsyncAction SettingsPage::ResetAsync(){auto lifetime=get_strong();if(co_await miniwindow::ui::Confirm(XamlRoot(),L"恢复默认设置？",L"恢复窗口功能和快捷键；保留书签、标签、场景预设、主题与渲染模式。",L"恢复默认")){auto result=app_->ResetDefaults();if(result.ok){dirty_.clear();editing_.clear();Refresh();}Feedback().Text(result.ok?L"已恢复默认设置。":result.message);}}
void SettingsPage::Diagnose(){
 const auto generation=++diagnosticGeneration_;diagnostic_.Text(L"正在读取实际系统与网页数据…");if(diagnosticTimer_)diagnosticTimer_.Stop();diagnosticTimer_=DispatcherTimer();diagnosticTimer_.Interval(std::chrono::seconds(5));diagnosticTimer_.Tick([weak=get_weak(),generation](auto&&,auto&&){if(auto self=weak.get();self&&self->diagnosticGeneration_==generation){self->diagnosticTimer_.Stop();++self->diagnosticGeneration_;self->diagnostic_.Text(L"诊断超时，请确认网页就绪后重试。");}});diagnosticTimer_.Start();
 app_->RequestDiagnostics([weak=get_weak(),generation](std::wstring value){if(auto self=weak.get();self&&self->diagnosticGeneration_==generation){self->diagnosticTimer_.Stop();self->diagnostic_.Text(value);}});
}
}
