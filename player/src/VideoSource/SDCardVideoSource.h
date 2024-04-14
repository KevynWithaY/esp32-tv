
#pragma once

#include "VideoSource.h"

class SDCardChannelData;

class SDCardVideoSource : public VideoSource
{
private:
  SDCardChannelData *mChannelData;
  int mFrameCount = 0;
  int FPS = 15;

public:
  SDCardVideoSource(SDCardChannelData *channelData);
  void start();
  void setFPS(int fps);
  // see superclass for documentation
  bool getVideoFrame(uint8_t **buffer, size_t &bufferLength, size_t &frameLength);
  void setChannel(int channel) {
    mFrameCount = 0;
  }
};
