#pragma once
#include "BrowserHost.h"
namespace miniwindow {
// The coordinator retains this stable port while a failed browser host is rebuilt.
struct BrowserPort final:IBrowserPort {
 BrowserHost* host=nullptr;
 std::function<OperationResult()> syncTabs;
 OperationResult SyncTabs(const std::vector<xiaochuang::TabData>&,size_t)override{return syncTabs?syncTabs():OperationResult{};}
 OperationResult Navigate(const std::wstring& url)override{return host?host->Navigate(url):OperationResult{false,L"浏览器正在初始化。"};}
 void SetVisible(bool visible)override{if(host)host->SetVisible(visible);}
 void ExecuteMedia(MediaCommand command,int rate)override{if(host)host->ExecuteMedia(command,rate);}
 std::wstring RuntimeVersion()const override{return host?host->RuntimeVersion():L"";}
 void RequestDiagnostics(std::function<void(std::wstring)> done)override{if(host)host->RequestDiagnostics(std::move(done));else done(L"浏览器尚未就绪。");}
 void Close()override{if(host)host->Close();}
};}
