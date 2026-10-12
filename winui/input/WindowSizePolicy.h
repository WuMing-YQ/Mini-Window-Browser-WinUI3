#pragma once
#include "../core/Ports.h"
namespace miniwindow {
RECT ResizeWindowBySteps(RECT current,int steps,RECT workArea,SIZE minimum,int snapDistance=0);
}
