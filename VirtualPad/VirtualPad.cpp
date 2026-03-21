#include <windows.h>
#include "PadEngine.h"
#include "AppWindow.h"

int main() {
    SetConsoleOutputCP(CP_UTF8);

    PadEngine engine;
    AppWindow window(engine);
    return window.run();
}
