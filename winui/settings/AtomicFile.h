#pragma once
#include "../core/Ports.h"
#include <filesystem>
namespace miniwindow {
OperationResult WriteAtomically(const std::filesystem::path&,const std::wstring&);
OperationResult BackupLegacy(const std::filesystem::path&);
}
