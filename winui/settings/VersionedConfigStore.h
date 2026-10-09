#pragma once
#include "../core/Ports.h"
#include <filesystem>
#include "AtomicFile.h"
namespace miniwindow {
struct ConfigFileOperations {
    std::function<OperationResult(const std::filesystem::path&)> backup=BackupLegacy;
    std::function<OperationResult(const std::filesystem::path&,const std::wstring&)> replace=WriteAtomically;
};
class VersionedConfigStore final:public ISettingsPort {
public:
    VersionedConfigStore(std::filesystem::path newPath,std::filesystem::path legacyPath,ConfigFileOperations operations={});
    OperationResult LoadOrMigrate(AppState&);
    OperationResult Save(const AppState&) override;
    const std::filesystem::path& Path() const noexcept {return path_;}
private:
    std::filesystem::path path_,legacy_;
    bool writable_=true;
    ConfigFileOperations operations_;
};
bool LegacyBrowserRunning(const std::filesystem::path& legacyDirectory);
}
