#include <windows.h>
#include <string>

namespace {
int ReportFailure(DWORD error) {
    std::wstring message = L"无法启动小窗浏览器。请完整解压便携包，保留 MiniWindow.exe 旁的 app 文件夹。\n\n";
    wchar_t detail[1024]{};
    if (FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                      nullptr, error, 0, detail, 1024, nullptr)) {
        message += detail;
    }
    message += L"\n错误代码：" + std::to_wstring(error);
    MessageBoxW(nullptr, message.c_str(), L"小窗浏览器", MB_OK | MB_ICONERROR);
    return 1;
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR arguments, int) {
    // Resolve relative to this EXE, independent of the shortcut's working directory.
    std::wstring module(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
    if (!length) return ReportFailure(GetLastError());
    if (length >= module.size()) return ReportFailure(ERROR_INSUFFICIENT_BUFFER);
    module.resize(length);
    const auto slash = module.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return ReportFailure(ERROR_BAD_PATHNAME);
    const auto directory = module.substr(0, slash + 1) + L"app";
    const auto executable = directory + L"\\MiniWindow.exe";
    std::wstring command = L"\"" + executable + L"\"";
    if (arguments && *arguments) command += std::wstring(L" ") + arguments;

    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    // No shell, scripts, downloads, extraction or injection; launch the bundled app.
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
                        0, nullptr, directory.c_str(), &startup, &process)) {
        return ReportFailure(GetLastError());
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return 0;
}
