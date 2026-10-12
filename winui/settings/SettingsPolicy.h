#pragma once
#include "../core/Ports.h"
#include "../../src/ConfigStore.h"
#include "../../src/CoreLogic.h"
namespace miniwindow {
struct SettingsDependencyState{bool zoomEnabled,lockAspectEnabled;};
inline SettingsDependencyState SettingsDependencies(const AppSettings& value){return {value.fixedWebZoomEnabled,value.autoFitVideoFullscreen};}
inline OperationResult ValidateSettings(const AppSettings& value){
 if(xiaochuang::TrimWhitespace(value.homeUrl).empty())return {false,L"主页不能为空。"};
 if(value.fixedWebZoomPercent<25||value.fixedWebZoomPercent>500)return {false,L"网页缩放须为 25–500%。"};
 if(value.holeRadius<10||value.holeRadius>2000)return {false,L"挖孔半径须为 10–2000 物理像素。"};
 if(value.snapThreshold<0||value.snapThreshold>200)return {false,L"吸附距离须为 0–200。"};
 if(value.holdPlaybackRate<2||value.holdPlaybackRate>5)return {false,L"长按倍速须为 2–5。"};
 if(value.holeOpacityPercent<0||value.holeOpacityPercent>100||value.autoHideOpacityPercent<0||value.autoHideOpacityPercent>100||value.hoverReductionPercent<0||value.hoverReductionPercent>100)return {false,L"不透明度与减淡比例须为 0–100%。"};
 if(value.immersionStyle!=ImmersionStyle::Hole&&value.immersionStyle!=ImmersionStyle::AutoHide)return {false,L"无效的沉浸模式。"};
 if(value.renderMode!=xiaochuang::RenderMode::AutomaticGpu&&value.renderMode!=xiaochuang::RenderMode::SoftwareCompatibility)return {false,L"无效的渲染模式。"};
 if(value.themeMode!=xiaochuang::ThemeMode::System&&value.themeMode!=xiaochuang::ThemeMode::Light&&value.themeMode!=xiaochuang::ThemeMode::Dark)return {false,L"无效的主题。"};
 return {};
}
}
