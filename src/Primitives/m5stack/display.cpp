#include "display.h"
#include "M5Cardputer.h"

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg);
}

int width() {
    return M5Cardputer.Display.width();
}

int height() {
    return M5Cardputer.Display.height();
}

void fillRect(int x, int y, int w, int h, uint32_t color) {
    M5Cardputer.Display.fillRect(x, y, w, h, color);
}

void fillCircle(int x, int y, int radius, uint32_t color) {
    M5Cardputer.Display.fillCircle(x, y, radius, color);
}