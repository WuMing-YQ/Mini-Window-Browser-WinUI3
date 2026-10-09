#pragma once
#include "MainWindow.g.h"
namespace winrt::MiniWindow::implementation { struct MainWindow : MainWindowT<MainWindow> { MainWindow(); }; }
namespace winrt::MiniWindow::factory_implementation { struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow> {}; }
