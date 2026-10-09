#pragma once
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <iterator>
struct TestDirectory {
    std::filesystem::path path;
    TestDirectory() {
        static std::atomic<unsigned> serial{0};wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);
        path=std::filesystem::path(exe).parent_path()/L"test-data"/(std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(++serial));
        std::filesystem::create_directories(path);
    }
    ~TestDirectory(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}
};
inline void WriteFixture(const std::filesystem::path& path,const std::wstring& text) {
    std::ofstream file(path,std::ios::binary);const unsigned short bom=0xfeff;
    file.write(reinterpret_cast<const char*>(&bom),2);file.write(reinterpret_cast<const char*>(text.data()),text.size()*sizeof(wchar_t));
}
inline std::string Bytes(const std::filesystem::path& path) {std::ifstream file(path,std::ios::binary);return {std::istreambuf_iterator<char>(file),{}};}
