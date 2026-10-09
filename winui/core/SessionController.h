#pragma once
#include "Ports.h"
namespace miniwindow {
class SessionController {
public:
    explicit SessionController(AppState& state):state_(state){}
    void AddTab(std::wstring);
    void SelectTab(size_t);
    void CloseTab(size_t);
    void CloseOthers(size_t);
    void CloseAll();
private:
    void EnsureTab();
    AppState& state_;
};
}
