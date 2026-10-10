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
Button Action(const std::wstring& name){Button button;button.Content(box_value(name));button.MinHeight(34);AccessibleName(button,name);return button;}
Style ResourceStyle(const wchar_t* key){return Application::Current().Resources().Lookup(box_value(key)).as<Style>();}
std::wstring Help(const std::wstring& name){
 const std::map<std::wstring,std::wstring> descriptions{
  {L"最小化到托盘",L"最小化后留在系统托盘，可随时恢复。"},
  {L"边缘吸附距离",L"单位为像素；按住 Ctrl 拖动可暂时绕过吸附。"},
  {L"最大化顶部拖动",L"最大化时拖动顶部边缘，恢复为普通窗口。"},
  {L"组合键＋滚轮调整窗口大小",L"仅普通窗口有焦点时生效，每格调整 5%。"},
  {L"沉浸模式",L"挖孔与移入减淡分别记住基础不透明度。"},
  {L"挖孔模式基础不透明度 (%)",L"孔外的显示不透明度；孔内始终透明。"},
  {L"减淡模式基础不透明度 (%)",L"鼠标移出浏览窗口后的显示不透明度。"},
  {L"移入额外减淡比例 (%)",L"按比例降低：基础 80%、减淡 50%，结果为 40%。"},
  {L"挖孔半径（物理像素）",L"鼠标周围透明区域的大小，10–2000 px。"},
  {L"主页",L"新建标签及关闭最后一个标签时使用。"},
  {L"固定网页缩放",L"在导航和重新加载时保持指定缩放比例。"},
  {L"网页缩放 (%)",L"调整网页内容大小，范围 25–500%。"},
  {L"视频全屏自动按比例调窗",L"网页视频进入全屏时，让小窗适配画面比例。"},
  {L"锁定视频全屏比例",L"开启自动调窗后，拖动边缘时保持视频比例。"},
  {L"隐藏时自动暂停",L"主动隐藏窗口时暂停；进入沉浸不暂停。"},
  {L"隐藏后允许媒体快捷键",L"窗口隐藏时仍可控制网页媒体。"},
  {L"长按快进倍速",L"松开按键后恢复之前的播放状态和倍速。"},
  {L"主题",L"主窗口、设置和沉浸控制面板保持一致。"},
  {L"网页渲染模式",L"修改后下次启动生效，当前网页继续播放。"}
 };auto item=descriptions.find(name);return item==descriptions.end()?L"":item->second;
}
void Row(StackPanel const& panel,const std::wstring& name,FrameworkElement const& control,const std::wstring& description=L""){
 Grid grid;grid.ColumnSpacing(20);ColumnDefinition label;label.Width({1,GridUnitType::Star});grid.ColumnDefinitions().Append(label);ColumnDefinition value;value.Width({1,GridUnitType::Auto});grid.ColumnDefinitions().Append(value);
 StackPanel text;text.Spacing(4);text.VerticalAlignment(VerticalAlignment::Center);auto title=miniwindow::ui::Text(name);title.Style(ResourceStyle(L"SettingsLabelStyle"));text.Children().Append(title);
 const auto help=description.empty()?Help(name):description;if(!help.empty()){auto detail=miniwindow::ui::Text(help);detail.Style(ResourceStyle(L"SettingsDescriptionStyle"));text.Children().Append(detail);}
 grid.Children().Append(text);control.VerticalAlignment(VerticalAlignment::Center);Grid::SetColumn(control,1);grid.Children().Append(control);
 Border card;card.Style(ResourceStyle(L"SettingsCardStyle"));card.Child(grid);panel.Children().Append(card);
}
void Group(StackPanel const& panel,const std::wstring& title){auto text=miniwindow::ui::Text(title);text.Style(ResourceStyle(L"SettingsGroupStyle"));panel.Children().Append(text);}
ToggleSwitch Switch(const std::wstring& name){ToggleSwitch value;value.MinWidth(0);value.OnContent(box_value(L""));value.OffContent(box_value(L""));AccessibleName(value,name);return value;}

}
SettingsPage::SettingsPage(){
 InitializeComponent();sliderTimer_=DispatcherTimer();sliderTimer_.Interval(std::chrono::milliseconds(300));
 sliderTimer_.Tick([weak=get_weak()](auto&&,auto&&){if(auto self=weak.get())self->FlushSliderChanges();});
}
void SettingsPage::Range(Panel panel,const std::wstring& name,int miniwindow::AppSettings::*member,int low,int high){
 Panel controls;controls.Width(220);controls.Spacing(2);
 TextBlock value;value.HorizontalAlignment(HorizontalAlignment::Right);
 Slider slider;slider.Minimum(low);slider.Maximum(high);slider.StepFrequency(1);AccessibleName(slider,name);
 controls.Children().Append(value);controls.Children().Append(slider);Row(panel,name,controls);
 fields_[name]={slider,[slider,member,low,high](auto& settings){settings.*member=std::clamp(static_cast<int>(std::round(slider.Value())),low,high);return miniwindow::OperationResult{};},[slider,value,member](const auto& settings){slider.Value(settings.*member);value.Text(std::to_wstring(settings.*member));}};
 slider.ValueChanged([this,name,value](auto&&,auto const& args){value.Text(std::to_wstring(static_cast<int>(std::round(args.NewValue()))));if(syncing_||!app_)return;Changed(name,false);pendingSliders_.insert(name);sliderTimer_.Stop();sliderTimer_.Start();});
}
void SettingsPage::FlushSliderChanges(){
 sliderTimer_.Stop();auto pending=std::move(pendingSliders_);pendingSliders_.clear();
 for(const auto& name:pending)if(dirty_.contains(name))Commit(name);
}
void SettingsPage::Boolean(Panel panel,const std::wstring& name,bool miniwindow::AppSettings::*member){
 auto box=Switch(name);Row(panel,name,box);
 fields_[name]={box,[box,member](auto& settings){settings.*member=box.IsOn();return miniwindow::OperationResult{};},[box,member](const auto& settings){box.IsOn(settings.*member);}};
 box.Toggled([this,name](auto&&,auto&&){Changed(name,true);});
}
void SettingsPage::Number(Panel panel,const std::wstring& name,int miniwindow::AppSettings::*member,int low,int high){
 TextBox box;box.Width(116);box.TextAlignment(TextAlignment::Right);AccessibleName(box,name);box.PlaceholderText(std::to_wstring(low)+L"–"+std::to_wstring(high));Row(panel,name,box);
 fields_[name]={box,[box,member,low,high,name](auto& settings){auto text=std::wstring(box.Text());wchar_t* end=nullptr;const auto number=wcstol(text.c_str(),&end,10);if(text.empty()||end==text.c_str()||*end||number<low||number>high)return miniwindow::OperationResult{false,name+L"须为 "+std::to_wstring(low)+L"–"+std::to_wstring(high)+L" 的整数。"};settings.*member=static_cast<int>(number);return miniwindow::OperationResult{};},[box,member](const auto& settings){box.Text(std::to_wstring(settings.*member));}};
 box.TextChanged([this,name](auto&&,auto&&){Changed(name,false);});box.GotFocus([this,name](auto&&,auto&&){editing_=name;});box.LostFocus([this,name](auto&&,auto&&){editing_.clear();if(dirty_.contains(name))Commit(name);});
}
void SettingsPage::Choice(Panel panel,const std::wstring& name,std::vector<std::wstring> options,std::function<int(const miniwindow::AppSettings&)> get,std::function<void(miniwindow::AppSettings&,int)> set){
 ComboBox box;box.Width(164);AccessibleName(box,name);for(const auto& item:options)box.Items().Append(box_value(item));Row(panel,name,box);
 fields_[name]={box,[box,set](auto& settings){set(settings,box.SelectedIndex());return miniwindow::OperationResult{};},[box,get](const auto& settings){box.SelectedIndex(get(settings));}};box.SelectionChanged([this,name](auto&&,auto&&){Changed(name,true);});
}
void SettingsPage::Bind(miniwindow::AppCoordinator& app,std::function<void(miniwindow::HotkeyAction)> capture,std::function<void()> cancelCapture){
 app_=&app;syncing_=true;cancelCapture_=std::move(cancelCapture);
 for(int i=0;i<6;++i){Panel section;section.Spacing(6);section.Visibility(Visibility::Collapsed);Sections().Children().Append(section);panels_.push_back(section);}
 SelectCategory(0);
 using S=miniwindow::AppSettings;
 Group(panels_[0],L"窗口行为");Boolean(panels_[0],L"最小化到托盘",&S::useSystemTray);Number(panels_[0],L"边缘吸附距离",&S::snapThreshold,0,200);Boolean(panels_[0],L"最大化顶部拖动",&S::maximizedTopDragEnabled);Boolean(panels_[0],L"组合键＋滚轮调整窗口大小",&S::windowWheelEnabled);
 panels_[0].Children().Append(miniwindow::ui::Text(L"普通窗口有焦点时，每格调整 5%；组合键在“快捷键”中选择。"));
 Group(panels_[1],L"显示方式");Choice(panels_[1],L"沉浸模式",{L"鼠标挖孔",L"移入减淡"},[](const S& s){return static_cast<int>(s.immersionStyle);},[](S& s,int n){s.immersionStyle=static_cast<miniwindow::ImmersionStyle>(n);});
 Range(panels_[1],L"挖孔模式基础不透明度 (%)",&S::holeOpacityPercent,0,100);Range(panels_[1],L"减淡模式基础不透明度 (%)",&S::autoHideOpacityPercent,0,100);Range(panels_[1],L"移入额外减淡比例 (%)",&S::hoverReductionPercent,0,100);Range(panels_[1],L"挖孔半径（物理像素）",&S::holeRadius,10,2000);
 panels_[1].Children().Append(miniwindow::ui::Text(L"移入时显示不透明度 = 基础不透明度 × (1 − 减淡比例)。普通模式始终不透明。"));auto restore=Action(L"恢复普通模式");restore.Click([this](auto&&,auto&&){app_->RestoreNormal();});Row(panels_[1],L"恢复窗口",restore,L"恢复完整不透明、可点击的普通窗口。");
 Group(panels_[2],L"网页");TextBox home;home.Width(230);AccessibleName(home,L"主页");Row(panels_[2],L"主页",home);fields_[L"主页"]={home,[home](S& s){s.homeUrl=std::wstring(home.Text());return miniwindow::OperationResult{};},[home](const S& s){home.Text(s.homeUrl);}};home.TextChanged([this](auto&&,auto&&){Changed(L"主页",false);});home.GotFocus([this](auto&&,auto&&){editing_=L"主页";});home.LostFocus([this](auto&&,auto&&){editing_.clear();if(dirty_.contains(L"主页"))Commit(L"主页");});
 Boolean(panels_[2],L"固定网页缩放",&S::fixedWebZoomEnabled);Number(panels_[2],L"网页缩放 (%)",&S::fixedWebZoomPercent,25,500);Group(panels_[2],L"媒体与播放");Boolean(panels_[2],L"视频全屏自动按比例调窗",&S::autoFitVideoFullscreen);Boolean(panels_[2],L"锁定视频全屏比例",&S::lockVideoFullscreenAspect);Boolean(panels_[2],L"隐藏时自动暂停",&S::autoPauseOnHide);Boolean(panels_[2],L"隐藏后允许媒体快捷键",&S::backgroundMediaHotkeys);Number(panels_[2],L"长按快进倍速",&S::holdPlaybackRate,2,5);
 Panel modifiers;modifiers.Orientation(Orientation::Horizontal);modifiers.Spacing(8);Group(panels_[3],L"窗口调节");Row(panels_[3],L"滚轮组合键",modifiers,L"至少选择一个修饰键，按住后滚动鼠标。");
 for(const auto& entry:std::vector<std::pair<std::wstring,UINT>>{{L"Ctrl",MOD_CONTROL},{L"Alt",MOD_ALT},{L"Shift",MOD_SHIFT},{L"Win",MOD_WIN}}){CheckBox box;box.Content(box_value(entry.first));AccessibleName(box,L"滚轮组合键 "+entry.first);modifiers.Children().Append(box);auto key=L"滚轮组合键 "+entry.first;const auto bit=entry.second;fields_[key]={box,[box,bit](S& s){if(box.IsChecked().Value())s.windowWheelModifiers|=bit;else s.windowWheelModifiers&=~bit;return miniwindow::OperationResult{};},[box,bit](const S& s){box.IsChecked((s.windowWheelModifiers&bit)!=0);}};box.Checked([this,key](auto&&,auto&&){Changed(key,true);});box.Unchecked([this,key](auto&&,auto&&){Changed(key,true);});}
 Group(panels_[3],L"全局快捷键");
 for(size_t i=0;i<xiaochuang::kHotkeyCount;++i){Panel controls;controls.Orientation(Orientation::Horizontal);controls.Spacing(12);auto label=app.Snapshot().state.hotkeys[i].displayName;auto enabled=Switch(label+L" 启用");enabled_[i]=enabled;auto button=Action(label+L" 修改快捷键");button.MinWidth(100);bindings_[i]=button;controls.Children().Append(button);controls.Children().Append(enabled);Row(panels_[3],label,controls);auto apply=[this,i,enabled](auto&&,auto&&){if(syncing_)return;auto value=app_->Snapshot().state.hotkeys[i];value.enabled=enabled.IsOn();auto result=app_->UpdateHotkey(static_cast<miniwindow::HotkeyAction>(i),value);if(!result.ok)Feedback().Text(result.message);Refresh();};enabled.Toggled(apply);button.Click([capture,i](auto&&,auto&&){capture(static_cast<miniwindow::HotkeyAction>(i));});}

 hotkeyStatus_=TextBlock();hotkeyStatus_.TextWrapping(TextWrapping::Wrap);AccessibleName(hotkeyStatus_,L"快捷键状态");panels_[3].Children().Append(hotkeyStatus_);
 Group(panels_[4],L"个性化");Choice(panels_[4],L"主题",{L"跟随系统",L"浅色",L"深色"},[](const S& s){return static_cast<int>(s.themeMode);},[](S& s,int n){s.themeMode=static_cast<xiaochuang::ThemeMode>(n);});panels_[4].Children().Append(miniwindow::ui::Text(L"主窗口、管理窗口和沉浸控制面板同步外观及标题栏。"));
 Group(panels_[5],L"运行与兼容性");Choice(panels_[5],L"网页渲染模式",{L"自动 GPU",L"软件兼容"},[](const S& s){return static_cast<int>(s.renderMode);},[](S& s,int n){s.renderMode=static_cast<xiaochuang::RenderMode>(n);});restart_=TextBlock();restart_.TextWrapping(TextWrapping::Wrap);AccessibleName(restart_,L"重启提示");panels_[5].Children().Append(restart_);
 auto diagnose=Action(L"获取诊断");diagnose.Click([this](auto&&,auto&&){Diagnose();});Row(panels_[5],L"运行诊断",diagnose,L"读取系统、WebView2、窗口与网页媒体状态。");diagnostic_=TextBox();diagnostic_.AcceptsReturn(true);diagnostic_.IsReadOnly(true);diagnostic_.TextWrapping(TextWrapping::Wrap);diagnostic_.Height(200);AccessibleName(diagnostic_,L"诊断结果");panels_[5].Children().Append(diagnostic_);
 auto copy=Action(L"复制诊断");copy.Click([this](auto&&,auto&&){const auto value=std::wstring(diagnostic_.Text());if(value.empty()){Feedback().Text(L"请先获取诊断。");return;}Feedback().Text(xiaochuang::CopyTextToClipboard(nullptr,value)?L"诊断已复制。":L"复制失败，请重试。");});panels_[5].Children().Append(copy);auto reset=Action(L"恢复默认设置…");reset.Click([this](auto&&,auto&&){ResetAsync();});Group(panels_[5],L"重置");Row(panels_[5],L"恢复默认",reset,L"保留书签、标签、场景预设、主题与渲染模式。");Group(panels_[5],L"关于小窗浏览器");
 panels_[5].Children().Append(miniwindow::ui::Text(L"Mini Window Browser · WinUI 3 衍生版\n基于 azurplain / Mini-Window-Browser，由本版维护者借助 AI 按个人需求修改，是原项目的个人衍生版本。\n本版的修改不代表原作者发布或认可。"));
 panels_[5].Children().Append(miniwindow::ui::Text(L"诚挚感谢 azurplain 与原项目贡献者的设计、开发和维护付出，以及开放源码、允许继续学习与修改的善意。"));
 panels_[5].Children().Append(miniwindow::ui::Text(L"许可证：GPL-3.0，保留原项目署名；许可文件见 docs/LICENSE.txt。\n完整声明见随包 README.md。"));HyperlinkButton upstream;upstream.Content(box_value(L"查看原项目"));upstream.NavigateUri(Windows::Foundation::Uri(L"https://github.com/azurplain/Mini-Window-Browser"));panels_[5].Children().Append(upstream);
 syncing_=false;Refresh();
}
void SettingsPage::SelectCategory(int category){
 if(category<0||category>=static_cast<int>(panels_.size()))return;
 if(!syncing_&&cancelCapture_)cancelCapture_();category_=category;
 const wchar_t* titles[]={L"窗口与行为",L"沉浸",L"网页与媒体",L"快捷键",L"外观",L"诊断与关于"};
 const wchar_t* descriptions[]={L"让小窗以顺手的方式停留、移动和调整大小。",L"两种沉浸方式独立保存参数，拖动滑块即可调整。",L"网页显示、全屏画面与媒体播放，各归其位。",L"配置常用操作；点击键位后按下新组合，Esc 取消。",L"选择适合你的外观，所有窗口同步更新。",L"查看实际运行信息，调整兼容性，或恢复默认设置。"};
 CategoryTitle().Text(titles[category]);CategoryDescription().Text(descriptions[category]);
 for(size_t i=0;i<panels_.size();++i)panels_[i].Visibility(static_cast<int>(i)==category?Visibility::Visible:Visibility::Collapsed);
}
void SettingsPage::Changed(const std::wstring& name,bool immediate){if(syncing_||!app_)return;dirty_.insert(name);if(immediate)Commit(name);}
bool SettingsPage::Commit(std::optional<std::wstring> only){
 auto draft=app_->Snapshot().state.settings;for(const auto& key:dirty_){if(only&&key!=*only)continue;auto result=fields_.at(key).read(draft);if(!result.ok){Feedback().Text(result.message);return false;}}
 auto result=app_->UpdateSettings(draft);if(!result.ok){Feedback().Text(result.message);return false;}if(only)dirty_.erase(*only);else dirty_.clear();Feedback().Text(L"设置已保存。");Refresh();return true;
}
void SettingsPage::Dependencies(){const auto deps=miniwindow::SettingsDependencies(app_->Snapshot().state.settings);fields_.at(L"网页缩放 (%)").control.as<Control>().IsEnabled(deps.zoomEnabled);fields_.at(L"锁定视频全屏比例").control.as<Control>().IsEnabled(deps.lockAspectEnabled);}
void SettingsPage::Refresh(){if(!app_)return;syncing_=true;const auto& snapshot=app_->Snapshot();for(const auto& [key,field]:fields_)if(!dirty_.contains(key)&&editing_!=key)field.write(snapshot.state.settings);for(size_t i=0;i<enabled_.size();++i){const auto& value=snapshot.state.hotkeys[i];enabled_[i].IsOn(value.enabled);bindings_[i].Content(box_value(xiaochuang::HotkeyDisplayText(value)));}auto status=snapshot.captureStatus;for(const auto& error:snapshot.hotkeyErrors)status+=L"\n注册失败："+error;hotkeyStatus_.Text(status);restart_.Text(snapshot.pendingRestart?L"渲染模式将在下次启动生效，当前网页继续播放。":L"当前渲染模式已生效。");Dependencies();syncing_=false;}
Windows::Foundation::IAsyncOperation<bool> SettingsPage::CanLeaveAsync(){auto lifetime=get_strong();FlushSliderChanges();if(!Dirty())co_return true;ContentDialog dialog;dialog.XamlRoot(XamlRoot());dialog.Title(box_value(L"设置尚未保存"));dialog.Content(box_value(L"请检查输入值，或放弃尚未保存的修改。"));dialog.PrimaryButtonText(L"保存并继续");dialog.SecondaryButtonText(L"放弃修改");dialog.CloseButtonText(L"继续编辑");auto result=co_await dialog.ShowAsync();if(result==ContentDialogResult::Primary)co_return Commit({});if(result==ContentDialogResult::Secondary){dirty_.clear();editing_.clear();Refresh();co_return true;}co_return false;}
Windows::Foundation::IAsyncAction SettingsPage::ResetAsync(){auto lifetime=get_strong();if(co_await miniwindow::ui::Confirm(XamlRoot(),L"恢复默认设置？",L"恢复窗口功能和快捷键；保留书签、标签、场景预设、主题与渲染模式。",L"恢复默认")){auto result=app_->ResetDefaults();if(result.ok){dirty_.clear();editing_.clear();Refresh();}Feedback().Text(result.ok?L"已恢复默认设置。":result.message);}}
void SettingsPage::Diagnose(){
 const auto generation=++diagnosticGeneration_;diagnostic_.Text(L"正在读取实际系统与网页数据…");if(diagnosticTimer_)diagnosticTimer_.Stop();diagnosticTimer_=DispatcherTimer();diagnosticTimer_.Interval(std::chrono::seconds(5));diagnosticTimer_.Tick([weak=get_weak(),generation](auto&&,auto&&){if(auto self=weak.get();self&&self->diagnosticGeneration_==generation){self->diagnosticTimer_.Stop();++self->diagnosticGeneration_;self->diagnostic_.Text(L"诊断超时，请确认网页就绪后重试。");}});diagnosticTimer_.Start();
 app_->RequestDiagnostics([weak=get_weak(),generation](std::wstring value){if(auto self=weak.get();self&&self->diagnosticGeneration_==generation){self->diagnosticTimer_.Stop();self->diagnostic_.Text(value);}});
}
}
