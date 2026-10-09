#pragma once
#include "BrowserHost.h"
namespace miniwindow {
// The coordinator retains this stable port while a failed browser host is rebuilt.
struct BrowserPort final:IBrowserPort {
 BrowserHost* host=nullptr;
 OperationResult Navigate(const std::wstring& url)override{return host?host->Navigate(url):OperationResult{false,L"浏览器正在初始化。"};}
 void SetVisible(bool visible)override{if(host)host->SetVisible(visible);}
 void ExecuteMedia(MediaCommand command,int rate)override{if(host)host->ExecuteMedia(command,rate);}
 void Close()override{if(host)host->Close();}
};}
