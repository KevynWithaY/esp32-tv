
#include <Arduino.h>
#include "SDCardVideoSource.h"
#include "../AVIParser/AVIParser.h"
#include "../ChannelData/SDCardChannelData.h"



//#ifdef FRAMES_PER_SECOND
//#define DEFAULT_FPS FRAMES_PER_SECOND
//FPS = FRAMES_PER_SECOND;
//#else
//#define DEFAULT_FPS 15
//#endif

SDCardVideoSource::SDCardVideoSource(SDCardChannelData *mChannelData) : mChannelData(mChannelData)
{
}

void SDCardVideoSource::start()
{
  // nothing to do!
  #ifdef FRAMES_PER_SECOND
  FPS = FRAMES_PER_SECOND;
  #endif
}

void SDCardVideoSource::setFPS(int fps)
{
  if (fps > 0) 
  {
    FPS = fps;
  } 
  else 
  {
    #ifdef FRAMES_PER_SECOND
      FPS = FRAMES_PER_SECOND;
    #else
      FPS = 15;
    #endif  
  }
}

bool SDCardVideoSource::getVideoFrame(uint8_t **buffer, size_t &bufferLength, size_t &frameLength)
{
  AVIParser *parser = mChannelData->getVideoParser();
  if (!parser) {
    return false;
  }
  if (mState == VideoPlayerState::STOPPED || mState == VideoPlayerState::STATIC)
  {
    vTaskDelay(100 / portTICK_PERIOD_MS);
    Serial.println("SDCardVideoSource::getVideoFrame: video is stopped or static");
    return false;
  }
  if (mState == VideoPlayerState::PAUSED)
  {
    // video time is not passing, so keep moving the start time forward so it is now
    mLastAudioTimeUpdateMs = millis();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    Serial.println("SDCardVideoSource::getVideoFrame: video is paused");
    return false;
  }
  // work out the video time from a combination of the currentAudioSample and the elapsed time
  int elapsedTime = millis() - mLastAudioTimeUpdateMs;
  int videoTime = mAudioTimeMs + elapsedTime;
  int frameTime = 1000 * mFrameCount / FPS;
  if (videoTime <= frameTime)
  {
    return false;
  }
  while (videoTime > 1000 * mFrameCount / FPS)
  {
    mFrameCount++;
    frameLength = parser->getNextChunk((uint8_t **)buffer, bufferLength);
  }
  return true;
}
