#include "AtomicFile.h"
#include <atomic>
namespace miniwindow {
namespace {
std::atomic<unsigned long> sequence{0};
std::wstring UniqueSuffix() {return std::to_wstring(GetTickCount64())+L"-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(++sequence);}
OperationResult Failure(const wchar_t* action,DWORD error) {return {false,std::wstring(action)+L"（Windows 错误 "+std::to_wstring(error)+L"）。"};}
}
OperationResult WriteAtomically(const std::filesystem::path& target,const std::wstring& content) {
    if(content.size()>(MAXDWORD-2)/sizeof(wchar_t))return {false,L"配置文件过大。"};
    const auto temporary=std::filesystem::path(target.wstring()+L"."+UniqueSuffix()+L".tmp");
    HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return Failure(L"无法创建临时配置文件",GetLastError());
    const unsigned short bom=0xfeff;
    DWORD written=0;
    bool ok=WriteFile(file,&bom,2,&written,nullptr)&&written==2;
    const auto size=static_cast<DWORD>(content.size()*sizeof(wchar_t));
    if(ok)ok=WriteFile(file,content.data(),size,&written,nullptr)&&written==size;
    if(ok)ok=FlushFileBuffers(file)!=FALSE;
    DWORD error=ok?ERROR_SUCCESS:GetLastError();
    if(!CloseHandle(file)&&ok){ok=false;error=GetLastError();}
    if(ok) {ok=MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;if(!ok)error=GetLastError();}
    if(!ok){DeleteFileW(temporary.c_str());return Failure(L"配置保存失败，原文件未替换",error);}
    return {};
}
OperationResult BackupLegacy(const std::filesystem::path& legacy) {
    const auto backup=legacy.parent_path()/(L"config.legacy-backup-"+UniqueSuffix()+L".ini");
    if(!CopyFileW(legacy.c_str(),backup.c_str(),TRUE))return Failure(L"旧配置备份失败，未执行迁移",GetLastError());
    return {};
}
}
