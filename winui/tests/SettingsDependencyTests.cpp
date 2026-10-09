#include "Test.h"
#include "../settings/SettingsPolicy.h"
void SettingsDependencyTests(){
 miniwindow::AppSettings settings;settings.fixedWebZoomPercent=175;settings.lockVideoFullscreenAspect=true;
 auto dependency=miniwindow::SettingsDependencies(settings);CHECK(!dependency.zoomEnabled);CHECK(!dependency.lockAspectEnabled);CHECK(settings.fixedWebZoomPercent==175);CHECK(settings.lockVideoFullscreenAspect);
 settings.fixedWebZoomEnabled=true;settings.autoFitVideoFullscreen=true;dependency=miniwindow::SettingsDependencies(settings);CHECK(dependency.zoomEnabled);CHECK(dependency.lockAspectEnabled);
 settings.fixedWebZoomEnabled=false;settings.autoFitVideoFullscreen=false;dependency=miniwindow::SettingsDependencies(settings);CHECK(!dependency.zoomEnabled);CHECK(!dependency.lockAspectEnabled);CHECK(settings.fixedWebZoomPercent==175);CHECK(settings.lockVideoFullscreenAspect);
}
