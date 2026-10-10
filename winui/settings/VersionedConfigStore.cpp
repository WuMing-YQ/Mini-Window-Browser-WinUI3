#include "VersionedConfigStore.h"
#include "AtomicFile.h"
#include "../../src/ConfigStore.h"
#include <tlhelp32.h>
#include <fstream>
#include <sstream>
#include <map>
#include <cwctype>
#include <limits>
#include <optional>
#include <cstring>
namespace miniwindow {
using xiaochuang::ThemeMode;
using xiaochuang::RenderMode;
using xiaochuang::kHotkeyCount;
namespace {
constexpr wchar_t kSettings[]=L"Settings",kSession[]=L"Session",kHotkeys[]=L"Hotkeys",kBookmarks[]=L"Bookmarks",kPresets[]=L"Presets";
const std::array<const wchar_t*,xiaochuang::kHotkeyCount> kHotkeyKeys={L"Immersion",L"PlayPause",L"Back",L"Forward",L"PrevEp",L"NextEp",L"HideWin"};
struct IniWriter {
    std::map<std::wstring,std::vector<std::pair<std::wstring,std::wstring>>> sections;
    void Put(const wchar_t* section,const std::wstring& key,std::wstring value) {sections[section].emplace_back(key,std::move(value));}
    std::wstring Text() const {std::wstring result;for(const auto& [section,rows]:sections){result+=L"["+section+L"]\r\n";for(const auto& [key,value]:rows)result+=key+L"="+value+L"\r\n";result+=L"\r\n";}return result;}
};
void WriteString(IniWriter& writer,const wchar_t* section,const std::wstring& key,const std::wstring& value) {writer.Put(section,key,L"\""+value+L"\"");}
void WriteInt(IniWriter& writer,const wchar_t* section,const std::wstring& key,int value) {writer.Put(section,key,std::to_wstring(value));}
void WriteHotkey(IniWriter& writer,const wchar_t* section,const std::wstring& key,const HotkeyBinding& value) {writer.Put(section,key,std::to_wstring(value.modifiers)+L","+std::to_wstring(value.virtualKey)+L","+std::to_wstring(value.enabled?1:0));}
std::wstring Serialize(const AppState& state) {
    IniWriter transactionPath;
    const auto normalRect=state.session.normalRect;const bool maximized=state.session.maximized;
    const AppSettings& settings = state.settings;
    WriteInt(transactionPath, kSettings, L"HoleRadius", settings.holeRadius);
    WriteInt(transactionPath, kSettings, L"SnapThreshold", settings.snapThreshold);
    WriteInt(transactionPath, kSettings, L"HoleOpacityPercent", settings.holeOpacityPercent);
    WriteInt(transactionPath, kSettings, L"AutoHideOpacityPercent", settings.autoHideOpacityPercent);
    WriteInt(transactionPath, kSettings, L"HoldPlaybackRate", settings.holdPlaybackRate);
    WriteInt(transactionPath, kSettings, L"FixedWebZoomEnabled", settings.fixedWebZoomEnabled ? 1 : 0);
    WriteInt(transactionPath, kSettings, L"FixedWebZoomPercent", settings.fixedWebZoomPercent);
    WriteInt(transactionPath, kSettings, L"AutoPause", settings.autoPauseOnHide ? 1 : 0);
    WriteInt(transactionPath, kSettings, L"SystemTray", settings.useSystemTray ? 1 : 0);
    WriteInt(transactionPath, kSettings, L"BackgroundMediaHotkeys", settings.backgroundMediaHotkeys ? 1 : 0);
    WriteInt(transactionPath, kSettings, L"AutoFitVideoFullscreen", settings.autoFitVideoFullscreen ? 1 : 0);
    WriteInt(transactionPath, kSettings, L"LockVideoFullscreenAspect", settings.lockVideoFullscreenAspect ? 1 : 0);
    WriteInt(transactionPath, kSettings, L"MaximizedTopDragEnabled", settings.maximizedTopDragEnabled ? 1 : 0);
    WriteInt(transactionPath, kSettings, L"ImmersionStyle", static_cast<int>(settings.immersionStyle));
    WriteString(transactionPath, kSettings, L"RenderMode",
                settings.renderMode == RenderMode::SoftwareCompatibility ? L"Software" : L"Auto");
    const wchar_t* theme = L"System";
    if (settings.themeMode == ThemeMode::Light) theme = L"Light";
    else if (settings.themeMode == ThemeMode::Dark) theme = L"Dark";
    WriteString(transactionPath, kSettings, L"Theme", theme);
    WriteString(transactionPath, kSettings, L"HomeUrl", settings.homeUrl);

    WriteInt(transactionPath, kSession, L"WinX", normalRect.left);
    WriteInt(transactionPath, kSession, L"WinY", normalRect.top);
    WriteInt(transactionPath, kSession, L"WinW", normalRect.right - normalRect.left);
    WriteInt(transactionPath, kSession, L"WinH", normalRect.bottom - normalRect.top);
    WriteInt(transactionPath, kSession, L"Fullscreen", maximized ? 1 : 0);
    WriteInt(transactionPath, kSession, L"ActiveTab", state.currentTabIndex);
    WriteInt(transactionPath, kSession, L"TabCount", static_cast<int>(state.tabs.size()));
    for (size_t index = 0; index < state.tabs.size(); ++index) {
        WriteString(transactionPath, kSession, L"TabUrl" + std::to_wstring(index), state.tabs[index].url);
        WriteString(transactionPath, kSession, L"TabTitle" + std::to_wstring(index), state.tabs[index].title);
    }

    for (size_t index = 0; index < kHotkeyCount; ++index) {
        WriteHotkey(transactionPath, kHotkeys, kHotkeyKeys[index], state.hotkeys[index]);
    }

    WriteInt(transactionPath, kBookmarks, L"Count", static_cast<int>(state.bookmarks.size()));
    for (size_t index = 0; index < state.bookmarks.size(); ++index) {
        WriteString(transactionPath, kBookmarks, L"Title" + std::to_wstring(index), state.bookmarks[index].title);
        WriteString(transactionPath, kBookmarks, L"Url" + std::to_wstring(index), state.bookmarks[index].url);
    }

    WriteInt(transactionPath, kPresets, L"Count", static_cast<int>(state.presets.size()));
    const std::array<const wchar_t*, kHotkeyCount> suffixes = {
        L"HkImm", L"HkPlay", L"HkBack", L"HkFwd", L"HkPrev", L"HkNext", L"HkHide"
    };
    for (size_t index = 0; index < state.presets.size(); ++index) {
        const Preset& preset = state.presets[index];
        const std::wstring prefix = L"P" + std::to_wstring(index) + L"_";
        WriteString(transactionPath, kPresets, prefix + L"Name", preset.name);
        WriteString(transactionPath, kPresets, prefix + L"HomeUrl", preset.homeUrl);
        WriteString(transactionPath, kPresets, prefix + L"CurUrl", preset.currentUrl);
        const std::wstring currentTitle = !preset.tabs.empty()
            ? preset.tabs[static_cast<size_t>(std::clamp(
                preset.activeTab, 0, static_cast<int>(preset.tabs.size()) - 1))].title
            : L"正在加载…";
        WriteString(transactionPath, kPresets, prefix + L"CurTitle", currentTitle);
        WriteInt(transactionPath, kPresets, prefix + L"TabCount", static_cast<int>(preset.tabs.size()));
        WriteInt(transactionPath, kPresets, prefix + L"ActiveTab", preset.activeTab);
        for (size_t tabIndex = 0; tabIndex < preset.tabs.size(); ++tabIndex) {
            const std::wstring tabPrefix = prefix + L"Tab" + std::to_wstring(tabIndex) + L"_";
            WriteString(transactionPath, kPresets, tabPrefix + L"Url", preset.tabs[tabIndex].url);
            WriteString(transactionPath, kPresets, tabPrefix + L"Title", preset.tabs[tabIndex].title);
        }
        WriteInt(transactionPath, kPresets, prefix + L"X", preset.normalRect.left);
        WriteInt(transactionPath, kPresets, prefix + L"Y", preset.normalRect.top);
        WriteInt(transactionPath, kPresets, prefix + L"W", preset.normalRect.right - preset.normalRect.left);
        WriteInt(transactionPath, kPresets, prefix + L"H", preset.normalRect.bottom - preset.normalRect.top);
        WriteInt(transactionPath, kPresets, prefix + L"Fullscreen", preset.maximized ? 1 : 0);
        WriteInt(transactionPath, kPresets, prefix + L"Rad", preset.holeRadius);
        WriteInt(transactionPath, kPresets, prefix + L"Snap", preset.snapThreshold);
        WriteInt(transactionPath, kPresets, prefix + L"HoleOpacity", preset.holeOpacityPercent);
        WriteInt(transactionPath, kPresets, prefix + L"AutoHideOpacity", preset.autoHideOpacityPercent);
        WriteInt(transactionPath, kPresets, prefix + L"HoldRate", preset.holdPlaybackRate);
        WriteInt(transactionPath, kPresets, prefix + L"FixedWebZoomEnabled", preset.fixedWebZoomEnabled ? 1 : 0);
        WriteInt(transactionPath, kPresets, prefix + L"FixedWebZoomPercent", preset.fixedWebZoomPercent);
        WriteInt(transactionPath, kPresets, prefix + L"Pause", preset.autoPauseOnHide ? 1 : 0);
        WriteInt(transactionPath, kPresets, prefix + L"SysTray", preset.useSystemTray ? 1 : 0);
        WriteInt(transactionPath, kPresets, prefix + L"BackgroundMedia", preset.backgroundMediaHotkeys ? 1 : 0);
        WriteInt(transactionPath, kPresets, prefix + L"AutoFitVideo", preset.autoFitVideoFullscreen ? 1 : 0);
        WriteInt(transactionPath, kPresets, prefix + L"LockVideoAspect", preset.lockVideoFullscreenAspect ? 1 : 0);
        WriteInt(transactionPath, kPresets, prefix + L"MaxTopDrag", preset.maximizedTopDragEnabled ? 1 : 0);
        WriteInt(transactionPath, kPresets, prefix + L"ImmStyle", static_cast<int>(preset.immersionStyle));
        for (size_t hotkeyIndex = 0; hotkeyIndex < kHotkeyCount; ++hotkeyIndex) {
            WriteHotkey(transactionPath, kPresets, prefix + suffixes[hotkeyIndex], preset.hotkeys[hotkeyIndex]);
        }
    }
    WriteInt(transactionPath,L"Meta",L"SchemaVersion",1);
    WriteInt(transactionPath,kSettings,L"HoverReductionPercent",state.settings.hoverReductionPercent);
    WriteInt(transactionPath,kSettings,L"WindowWheelEnabled",state.settings.windowWheelEnabled);
    WriteInt(transactionPath,kSettings,L"WindowWheelModifiers",state.settings.windowWheelModifiers);
    for(size_t i=0;i<state.presets.size();++i) {
        const auto prefix=L"P"+std::to_wstring(i)+L"_";
        WriteInt(transactionPath,kPresets,prefix+L"HoverReductionPercent",state.presets[i].hoverReductionPercent);
        WriteInt(transactionPath,kPresets,prefix+L"WindowWheelEnabled",state.presets[i].windowWheelEnabled);
        WriteInt(transactionPath,kPresets,prefix+L"WindowWheelModifiers",state.presets[i].windowWheelModifiers);
    }
    return transactionPath.Text();
}

std::wstring Trim(std::wstring value) {
    auto first=value.find_first_not_of(L" \t\r"),last=value.find_last_not_of(L" \t\r");
    return first==std::wstring::npos?L"":value.substr(first,last-first+1);
}
std::wstring Lower(std::wstring value) {for(auto& c:value)c=static_cast<wchar_t>(towlower(c));return value;}
using Ini=std::map<std::wstring,std::map<std::wstring,std::wstring>>;
bool Integer(const std::wstring& text,long long& value) {
    if(text.empty())return false;size_t used=0;
    try {value=std::stoll(text,&used,10);}catch(...){return false;}
    return used==text.size();
}
OperationResult Parse(const std::wstring& text,Ini& ini) {
    if(text.find(L'\0')!=std::wstring::npos)return {false,L"配置包含无效字符。"};
    std::wistringstream stream(text);std::wstring line,section;
    while(std::getline(stream,line)) {
        line=Trim(line);if(line.empty()||line[0]==L';'||line[0]==L'#')continue;
        if(line.front()==L'['&&line.back()==L']'){section=Lower(Trim(line.substr(1,line.size()-2)));continue;}
        const auto equal=line.find(L'=');
        if(section.empty()||equal==std::wstring::npos)return {false,L"配置文件格式无效。"};
        auto key=Lower(Trim(line.substr(0,equal))),value=Trim(line.substr(equal+1));
        if(key.empty()||value.size()>4094)return {false,L"配置字段为空或过长。"};
        if(value.size()>=2&&((value.front()==L'"'&&value.back()==L'"')||(value.front()==L'\''&&value.back()==L'\'')))value=value.substr(1,value.size()-2);
        if(!ini[section].emplace(std::move(key),std::move(value)).second)return {false,L"配置包含重复字段。"};
    }
    if(!ini.contains(L"settings"))return {false,L"配置缺少 Settings 节。"};
    return {};
}
OperationResult ReadIni(const std::filesystem::path& path,Ini& ini) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file)return {false,L"无法读取配置文件。"};
    const auto size=file.tellg();if(size<0||size>16*1024*1024)return {false,L"配置文件大小无效。"};
    std::string bytes(static_cast<size_t>(size),'\0');file.seekg(0);file.read(bytes.data(),size);
    if(!file)return {false,L"配置文件读取不完整。"};
    std::wstring text;
    if(bytes.size()>=2&&static_cast<unsigned char>(bytes[0])==0xff&&static_cast<unsigned char>(bytes[1])==0xfe) {
        if(bytes.size()%2)return {false,L"UTF-16 配置文件不完整。"};
        text.resize((bytes.size()-2)/2);memcpy(text.data(),bytes.data()+2,bytes.size()-2);
    }else {
        // Legacy Win32 INI APIs read non-Unicode files in the system ANSI code page.
        int count=MultiByteToWideChar(CP_ACP,0,bytes.data(),static_cast<int>(bytes.size()),nullptr,0);
        if(!count)return {false,L"配置文件编码无效。"};
        text.resize(count);MultiByteToWideChar(CP_ACP,0,bytes.data(),static_cast<int>(bytes.size()),text.data(),count);
    }
    return Parse(text,ini);
}
using Range=std::pair<long long,long long>;
// Accept the historical radius range on read; ConfigStore clamps it to 10–2000.
// This preserves old settings and presets instead of rejecting the whole file.
const std::map<std::wstring,Range> settingsRanges={
    {L"holeradius",{0,4000}},{L"snapthreshold",{0,200}},
    {L"holeopacitypercent",{0,100}},{L"autohideopacitypercent",{0,100}},
    {L"holdplaybackrate",{2,5}},{L"fixedwebzoompercent",{25,500}},
    {L"fixedwebzoomenabled",{0,1}},{L"autopause",{0,1}},{L"disablehkontyping",{0,1}},
    {L"systemtray",{0,1}},{L"backgroundmediahotkeys",{0,1}},{L"autofitvideofullscreen",{0,1}},
    {L"lockvideofullscreenaspect",{0,1}},{L"maximizedtopdragenabled",{0,1}},
    {L"immersionstyle",{0,1}},{L"hoverreductionpercent",{0,100}},
    {L"windowwheelenabled",{0,1}},{L"windowwheelmodifiers",{1,15}}
};
const std::map<std::wstring,Range> presetRanges={
    {L"rad",{0,4000}},{L"snap",{0,200}},{L"holeopacity",{0,100}},{L"autohideopacity",{0,100}},
    {L"holdrate",{2,5}},{L"fixedwebzoompercent",{25,500}},{L"fixedwebzoomenabled",{0,1}},
    {L"pause",{0,1}},{L"hktyping",{0,1}},{L"systray",{0,1}},{L"backgroundmedia",{0,1}},
    {L"autofitvideo",{0,1}},{L"lockvideoaspect",{0,1}},{L"maxtopdrag",{0,1}},
    {L"immstyle",{0,1}},{L"fullscreen",{0,1}},{L"tabcount",{0,100}},{L"activetab",{0,99}},
    {L"x",{INT_MIN/2,INT_MAX/2}},{L"y",{INT_MIN/2,INT_MAX/2}},
    {L"w",{160,100000}},{L"h",{120,100000}},
    {L"hoverreductionpercent",{0,100}},{L"windowwheelenabled",{0,1}},{L"windowwheelmodifiers",{1,15}}
};
bool ValidHotkey(const std::wstring& text) {
    std::wistringstream input(text);std::wstring field;std::vector<long long> fields;
    while(std::getline(input,field,L',')){long long number;if(!Integer(Trim(field),number))return false;fields.push_back(number);}
    if(fields.size()<2||fields.size()>3||text.back()==L',')return false;
    const auto key=fields[1];return fields[0]>=0&&fields[0]<=15&&key>0&&key<256&&key!=VK_CONTROL&&key!=VK_SHIFT&&key!=VK_MENU&&key!=VK_LWIN&&key!=VK_RWIN&&(fields.size()==2||fields[2]==0||fields[2]==1);
}
OperationResult Validate(const Ini& ini,bool versioned) {
    if(versioned) {
        auto meta=ini.find(L"meta");
        if(meta==ini.end()||!meta->second.contains(L"schemaversion")||meta->second.at(L"schemaversion")!=L"1")return {false,L"配置版本不受支持，未修改文件。"};
    }
    for(const auto& [section,rows]:ini)for(const auto& [key,value]:rows) {
        std::optional<Range> range;bool hotkey=false;
        if(section==L"settings") {auto found=settingsRanges.find(key);if(found!=settingsRanges.end())range=found->second;
            if(key==L"theme"&&Lower(value)!=L"system"&&Lower(value)!=L"light"&&Lower(value)!=L"dark")return {false,L"主题配置无效。"};
            if(key==L"rendermode"&&Lower(value)!=L"auto"&&Lower(value)!=L"software")return {false,L"渲染配置无效。"};
        }
        if(section==L"session") {
            if(key==L"winx"||key==L"winy")range=Range{INT_MIN/2,INT_MAX/2};
            else if(key==L"winw")range=Range{160,100000};else if(key==L"winh")range=Range{120,100000};
            else if(key==L"fullscreen")range=Range{0,1};else if(key==L"tabcount")range=Range{0,100};else if(key==L"activetab")range=Range{0,99};
        }
        if(section==L"bookmarks"&&key==L"count")range=Range{0,1000};
        if(section==L"hotkeys")hotkey=true;
        if(section==L"presets") {
            if(key==L"count")range=Range{0,200};
            auto separator=key.find(L'_');if(separator!=std::wstring::npos){auto suffix=key.substr(separator+1);auto found=presetRanges.find(suffix);if(found!=presetRanges.end())range=found->second;
                hotkey=suffix==L"hkimm"||suffix==L"hkplay"||suffix==L"hkback"||suffix==L"hkfwd"||suffix==L"hkprev"||suffix==L"hknext"||suffix==L"hkhide";
            }
        }
        long long number;
        if(range&&(!Integer(value,number)||number<range->first||number>range->second))return {false,L"配置数值无效："+section+L"."+key};
        if(hotkey&&!ValidHotkey(value))return {false,L"快捷键配置无效："+key};
    }
    auto get=[&](const std::wstring& section,const std::wstring& key)->const std::wstring*{auto rows=ini.find(section);if(rows==ini.end())return nullptr;auto field=rows->second.find(key);return field==rows->second.end()?nullptr:&field->second;};
    auto count=[&](const std::wstring& section,const std::wstring& key)->int{auto value=get(section,key);return value?std::stoi(*value):0;};
    auto complete=[&](const std::wstring& section,const std::wstring& key,bool nonempty){auto value=get(section,key);return value&&(!nonempty||!value->empty());};
    if(versioned&&(!get(L"bookmarks",L"count")||!get(L"presets",L"count")||!get(L"session",L"tabcount")))return {false,L"配置列表计数缺失，已保护原文件。"};
    for(int i=0;i<count(L"bookmarks",L"count");++i){const auto suffix=std::to_wstring(i);if(!complete(L"bookmarks",L"url"+suffix,true)||!complete(L"bookmarks",L"title"+suffix,false))return {false,L"书签记录不完整，已保护原文件。"};}
    const int tabs=count(L"session",L"tabcount");for(int i=0;i<tabs;++i){const auto suffix=std::to_wstring(i);if(!complete(L"session",L"taburl"+suffix,true)||!complete(L"session",L"tabtitle"+suffix,false))return {false,L"会话标签不完整，已保护原文件。"};}
    if(tabs>0&&count(L"session",L"activetab")>=tabs)return {false,L"当前会话标签超出列表范围。"};
    for(int i=0;i<count(L"presets",L"count");++i){const auto prefix=L"p"+std::to_wstring(i)+L"_";
        if(!complete(L"presets",prefix+L"name",true))return {false,L"预设记录不完整，已保护原文件。"};
        if(versioned&&!get(L"presets",prefix+L"tabcount"))return {false,L"预设标签计数缺失，已保护原文件。"};
        const int presetTabs=count(L"presets",prefix+L"tabcount");
        for(int j=0;j<presetTabs;++j){const auto key=prefix+L"tab"+std::to_wstring(j)+L"_";if(!complete(L"presets",key+L"url",true)||!complete(L"presets",key+L"title",false))return {false,L"预设标签不完整，已保护原文件。"};}
        if(presetTabs>0&&count(L"presets",prefix+L"activetab")>=presetTabs)return {false,L"预设当前标签超出列表范围。"};
    }
    return {};
}
int Field(const Ini& ini,const std::wstring& section,const std::wstring& key,int fallback) {
    auto rows=ini.find(Lower(section));if(rows==ini.end())return fallback;
    auto value=rows->second.find(Lower(key));if(value==rows->second.end())return fallback;
    return std::stoi(value->second);
}
bool SafeText(const std::wstring& value){return value.size()<=4092&&value.find_first_of(L"\r\n") == std::wstring::npos&&value.find(L'\0')==std::wstring::npos;}
OperationResult ValidState(const AppState& state) {
    if(state.tabs.empty()||state.tabs.size()>100||state.bookmarks.size()>1000||state.presets.size()>200||state.currentTabIndex<0||static_cast<size_t>(state.currentTabIndex)>=state.tabs.size())return {false,L"配置列表或当前标签无效。"};
    auto tabsSafe=[](const auto& tabs){for(const auto& tab:tabs)if(!SafeText(tab.title)||!SafeText(tab.url))return false;return true;};
    if(!SafeText(state.settings.homeUrl)||!tabsSafe(state.tabs))return {false,L"网页地址或标题包含无效字符。"};
    for(const auto& bookmark:state.bookmarks)if(!SafeText(bookmark.title)||!SafeText(bookmark.url))return {false,L"书签包含无效字符。"};
    for(const auto& preset:state.presets)if(!SafeText(preset.name)||!SafeText(preset.homeUrl)||!SafeText(preset.currentUrl)||!tabsSafe(preset.tabs)||preset.tabs.size()>100||preset.activeTab<0||(!preset.tabs.empty()&&static_cast<size_t>(preset.activeTab)>=preset.tabs.size()))return {false,L"预设内容无效。"};
    return {};
}
} // namespace
VersionedConfigStore::VersionedConfigStore(std::filesystem::path newPath,std::filesystem::path legacyPath,ConfigFileOperations operations):path_(std::move(newPath)),legacy_(std::move(legacyPath)),operations_(std::move(operations)){}
OperationResult VersionedConfigStore::LoadOrMigrate(AppState& state) {
    writable_=false;
    try {
        const bool current=std::filesystem::exists(path_),legacy=std::filesystem::exists(legacy_);
        if(!current&&!legacy){state=AppState{};state.tabs.push_back({L"正在加载…",state.settings.homeUrl});writable_=true;return {};}
        const auto& source=current?path_:legacy_;Ini ini;
        auto result=ReadIni(source,ini);if(!result.ok)return result;
        result=Validate(ini,current);if(!result.ok)return result;
        AppState loaded;xiaochuang::ConfigStore(source).Load(loaded);
        loaded.settings.hoverReductionPercent=current?Field(ini,L"Settings",L"HoverReductionPercent",100):100;
        loaded.settings.windowWheelEnabled=Field(ini,L"Settings",L"WindowWheelEnabled",1)!=0;
        loaded.settings.windowWheelModifiers=Field(ini,L"Settings",L"WindowWheelModifiers",MOD_CONTROL|MOD_ALT);
        if(!current)loaded.settings.holeOpacityPercent=100;
        for(size_t i=0;i<loaded.presets.size();++i){auto& preset=loaded.presets[i];const auto prefix=L"P"+std::to_wstring(i)+L"_";
            preset.hoverReductionPercent=current?Field(ini,L"Presets",prefix+L"HoverReductionPercent",100):100;
            preset.windowWheelEnabled=Field(ini,L"Presets",prefix+L"WindowWheelEnabled",1)!=0;
            preset.windowWheelModifiers=Field(ini,L"Presets",prefix+L"WindowWheelModifiers",MOD_CONTROL|MOD_ALT);
            if(!current)preset.holeOpacityPercent=100;
        }
        result=ValidState(loaded);if(!result.ok)return result;
        if(!current){result=operations_.backup(legacy_);if(!result.ok)return result;result=operations_.replace(path_,Serialize(loaded));if(!result.ok)return result;}
        state=std::move(loaded);writable_=true;return {};
    }catch(const std::exception&){return {false,L"配置读取或迁移失败，原文件未修改。"};}
}
OperationResult VersionedConfigStore::Save(const AppState& state) {
    if(!writable_)return {false,L"配置读取失败，已保护原文件；请修复配置后重新启动。"};
    auto result=ValidState(state);if(!result.ok)return result;
    auto text=Serialize(state);Ini ini;result=Parse(text,ini);if(!result.ok)return result;
    result=Validate(ini,true);if(!result.ok)return result;
    return operations_.replace(path_,text);
}
bool LegacyBrowserRunning(const std::filesystem::path& directory) {
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)return true;
    PROCESSENTRY32W process{sizeof(process)};bool running=false;
    if(Process32FirstW(snapshot,&process))do {
        if(_wcsicmp(process.szExeFile,L"XiaoChuang.exe")!=0)continue;
        HANDLE handle=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,process.th32ProcessID);
        if(!handle){running=true;break;}
        wchar_t path[32768]{};DWORD length=32768;
        if(!QueryFullProcessImageNameW(handle,0,path,&length))running=true;
        else running=_wcsicmp(std::filesystem::path(path).parent_path().c_str(),directory.c_str())==0;
        CloseHandle(handle);if(running)break;
    }while(Process32NextW(snapshot,&process));
    CloseHandle(snapshot);return running;
}
} // namespace miniwindow
