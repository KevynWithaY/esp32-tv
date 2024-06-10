#pragma once
#ifndef LED_MATRIX
#include "Display.h"

class TFT_eSPI;

class TFT: public Display {
private:
  TFT_eSPI *tft;
  uint16_t *dmaBuffer[2] = {NULL, NULL};
  int dmaBufferIndex = 0;
public:
  TFT();
  void drawPixels(int x, int y, int width, int height, uint16_t *pixels);
  void startWrite();
  void endWrite();
  int width();
  int height();
  void fillScreen(uint16_t color);
  void drawChannel(int channelIndex);
  void drawTuningText();
  void drawErrorText(String text);
  void drawVolumeText(int volume);
  void drawFPS(int fps);
  void drawArbitraryText(String text, int x, int y, uint16_t forecolor, uint16_t backcolor = TFT_BLACK, bool fillBackground = false, bool clearScreen = false);
};
#endif