#include <windows.h>
#include "Log.h"
#include "config/ConfigLoader.h"
#include "PadEngine.h"
#include "AppWindow.h"

int main() {
    SetConsoleOutputCP(CP_UTF8);

    VirtualPadConfig vpCfg;
    try { vpCfg = loadVirtualPadConfig("data/virtualpad.json"); } catch (...) {}

    Log::init(vpCfg.logLevel);

    PadEngine engine;
    AppWindow window(engine);
    return window.run();
}
