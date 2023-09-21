#pragma once

#include "VideoSource.h"

class NetworkVideoSource: public VideoSource {
  private:
    int mCurrentChannel = 0;
    bool mFrameReady=false;
    uint8_t *mCurrentFrameBuffer = NULL;
    size_t mCurrentFrameLength = 0;
    size_t mCurrentFrameBufferLength = 0;
    const char *mFrameURL = NULL;
    SemaphoreHandle_t mCurrentFrameMutex = NULL;

    static void _frameDownloaderTask(void *arg);
    void frameDownloaderTask();
  public:
    NetworkVideoSource(const char *frameUrl);
    void start();
    // see superclass for documentation
    bool getVideoFrame(uint8_t **buffer, size_t &bufferLength, size_t &frameLength);
    void setChannel(int channel);
};
