#include "Test.h"
#include "TestFiles.h"
#include "../settings/AtomicFile.h"
void AtomicFileTests() {
    TestDirectory directory;
    auto target=directory.path/L"config.winui.ini";
    CHECK(miniwindow::WriteAtomically(target,L"你好🚀").ok);
    CHECK(Bytes(target).substr(0,2)==std::string("\xff\xfe",2));
    auto before=Bytes(target);
    HANDLE lock=CreateFileW(target.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    CHECK(lock!=INVALID_HANDLE_VALUE);
    CHECK(!miniwindow::WriteAtomically(target,L"replacement").ok);
    CloseHandle(lock);
    CHECK(Bytes(target)==before);
    CHECK(!miniwindow::WriteAtomically(directory.path/L"missing"/L"config.ini",L"write").ok);
    CHECK(miniwindow::WriteAtomically(target,L"updated").ok);
    CHECK(Bytes(target)!=before);
    CHECK(miniwindow::BackupLegacy(target).ok);
    CHECK(miniwindow::BackupLegacy(target).ok);
    size_t backups=0,temporary=0;
    for(const auto& item:std::filesystem::directory_iterator(directory.path)) {
        auto name=item.path().filename().wstring();if(name.starts_with(L"config.legacy-backup-"))++backups;
        if(name.ends_with(L".tmp"))++temporary;
    }
    CHECK(backups==2);
    CHECK(temporary==0);
}
