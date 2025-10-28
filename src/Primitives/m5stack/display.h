#pragma once

#include <cstdint>

// port of the Graphics library for M5Stack series
// https://github.com/m5stack/M5GFX

void displaySetup();

int width();

int height();

void fillRect(int x, int y, int w, int h, uint32_t color);

void fillCircle(int x, int y, int radius, uint32_t color);
