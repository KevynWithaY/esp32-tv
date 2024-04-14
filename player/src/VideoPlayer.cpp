#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "VideoPlayer.h"
#include "AudioOutput/AudioOutput.h"
#include "ChannelData/ChannelData.h"
#include "VideoSource/VideoSource.h"
#include "AudioSource/AudioSource.h"
#include "Displays/Display.h"
#include <list>

#ifdef FRAME_PLAYER_CORE
#define FRAME_CORE FRAME_PLAYER_CORE
#else
#define FRAME_CORE 1
#endif

#ifdef AUDIO_PLAYER_CORE
#define AUDIO_CORE AUDIO_PLAYER_CORE
#else
#define AUDIO_CORE 1
#endif

void VideoPlayer::_framePlayerTask(void *param)
{
  VideoPlayer *player = (VideoPlayer *)param;
  player->framePlayerTask();
}

void VideoPlayer::_audioPlayerTask(void *param)
{
  VideoPlayer *player = (VideoPlayer *)param;
  player->audioPlayerTask();
}

VideoPlayer::VideoPlayer(ChannelData *channelData, VideoSource *videoSource, AudioSource *audioSource, Display &display, AudioOutput *audioOutput)
: mChannelData(channelData), mVideoSource(videoSource), mAudioSource(audioSource), mDisplay(display), mState(VideoPlayerState::STOPPED), mAudioOutput(audioOutput)
{
}

void VideoPlayer::start()
{
  mVideoSource->start();
  mAudioSource->start();

  // launch the frame player task
  xTaskCreatePinnedToCore(_framePlayerTask, "Frame Player", 10000, this, 1, NULL, FRAME_CORE);

  // launch the audio player task
  xTaskCreatePinnedToCore(_audioPlayerTask, "audio_loop", 10000, this, 1, NULL, AUDIO_CORE);
}

void VideoPlayer::setChannel(int channel)
{
  mChannelData->setChannel(channel);
  mVideoSource->setFPS(mChannelData->getFileFPS());
  // set the audio sample to 0 - TODO - move this somewhere else?
  mCurrentAudioSample = 0;
  mChannelVisible = millis();
  // update the video source
  mVideoSource->setChannel(channel);
}

void VideoPlayer::volumeUpdated()
{
  showVolumeText = true;
  volumeTextTime = millis();
}

void VideoPlayer::play()
{
  if (mState == VideoPlayerState::PLAYING)
  {
    return;
  }
  mState = VideoPlayerState::PLAYING;
  mVideoSource->setState(VideoPlayerState::PLAYING);
  mCurrentAudioSample = 0;
  showPrePlayStatic=true;
  prePlayStaticTime=millis();
}

void VideoPlayer::stop()
{
  if (mState == VideoPlayerState::STOPPED)
  {
    return;
  }
  mState = VideoPlayerState::STOPPED;
  mVideoSource->setState(VideoPlayerState::STOPPED);
  mCurrentAudioSample = 0;
  mDisplay.fillScreen(DisplayColors::BLACK);
}

void VideoPlayer::pause()
{
  if (mState == VideoPlayerState::PAUSED)
  {
    return;
  }
  mState = VideoPlayerState::PAUSED;
  mVideoSource->setState(VideoPlayerState::PAUSED);
}

void VideoPlayer::playStatic()
{
  if (mState == VideoPlayerState::STATIC)
  {
    return;
  }
  mState = VideoPlayerState::STATIC;
  mVideoSource->setState(VideoPlayerState::STATIC);
}




// double buffer the dma drawing otherwise we get corruption
uint16_t *dmaBuffer[2] = {NULL, NULL};
int dmaBufferIndex = 0;
int _doDraw(JPEGDRAW *pDraw)
{
  VideoPlayer *player = (VideoPlayer *)pDraw->pUser;
  player->mDisplay.drawPixels(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);
  return 1;
}

static unsigned short x = 12345, y = 6789, z = 42, w = 1729;

unsigned short xorshift16()
{
  unsigned short t = x ^ (x << 5);
  x = y;
  y = z;
  z = w;
  w = w ^ (w >> 1) ^ t ^ (t >> 3);
  return w & 0xFFFF;
}

VideoPlayerState VideoPlayer::getState()
{
  return mState;
}

void VideoPlayer::framePlayerTask()
{
  uint16_t *staticBuffer = NULL;
  uint8_t *jpegBuffer = NULL;
  size_t jpegBufferLength = 0;
  size_t jpegLength = 0;

  // used for calculating frame rate
  std::list<int> frameTimes;

  while (true)
  {
    // Paused or Stopped:
    if (mState == VideoPlayerState::STOPPED || mState == VideoPlayerState::PAUSED)
    {
      // nothing to do - just wait
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }

    // Static:
    if (mState == VideoPlayerState::STATIC || (showPrePlayStatic)) 
    {
      // draw random pixels to the screen to simulate static
      // we'll do this 8 rows of pixels at a time to save RAM
      int width = mDisplay.width();
      int height = 8;
      if (staticBuffer == NULL)
      {
        staticBuffer = (uint16_t *)malloc(width * height * 2);
      }
      mDisplay.startWrite();
      for (int i = 0; i < mDisplay.height(); i++)
      {
        for (int p = 0; p < width * height; p++)
        {
          int grey = xorshift16() >> 8;
          staticBuffer[p] = mDisplay.color565(grey, grey, grey);
        }
        mDisplay.drawPixels(0, i * height, width, height, staticBuffer);
      }
    

      if (showPrePlayStatic) {
        if (millis() - prePlayStaticTime > 1000) {
          showPrePlayStatic=false;
          mChannelVisible = millis();
        } else {
          mDisplay.drawTuningText();
        }

        mDisplay.endWrite();
      }      
      
      vTaskDelay(50 / portTICK_PERIOD_MS);
      continue;
    }

    // Playing:
    // get the next frame
    if (!mVideoSource->getVideoFrame(&jpegBuffer, jpegBufferLength, jpegLength))
    {
      // no frame ready yet
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }
    frameTimes.push_back(millis());
    // keep the frame rate elapsed time to 5 seconds
    while(frameTimes.size() > 0 && frameTimes.back() - frameTimes.front() > 5000) {
      frameTimes.pop_front();
    }
    mDisplay.startWrite();
    if (mJpeg.openRAM(jpegBuffer, jpegLength, _doDraw))
    {
      mJpeg.setUserPointer(this);
      #ifdef LED_MATRIX
      mJpeg.setPixelType(RGB565_LITTLE_ENDIAN);
      #else
      mJpeg.setPixelType(RGB565_BIG_ENDIAN);
      #endif
      mJpeg.decode(0, 0, 0);
      mJpeg.close();
    }

    // show channel indicator 
    if (millis() - mChannelVisible < 2000) {
      mDisplay.drawChannel(mChannelData->getChannelNumber());
    }

    // show volume indicator
    if (showVolumeText) {
      if (millis() - volumeTextTime < 2000) {
        mDisplay.drawVolumeText(mAudioOutput->getVolume());
      } else {
        showVolumeText = false;
      }
    }

    //#if CORE_DEBUG_LEVEL > 0
    #ifdef SHOW_FPS
    mDisplay.drawFPS(frameTimes.size() / 5);
    #endif
    //#endif
    mDisplay.endWrite();
  }
}

void VideoPlayer::audioPlayerTask()
{
  size_t bufferLength = 16000;
  int8_t *audioData = (int8_t *)malloc(16000);
  while (true)
  {
    if (mState != VideoPlayerState::PLAYING)
    {
      // nothing to do - just wait
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }
    // get audio data to play
    int audioLength = mAudioSource->getAudioSamples(&audioData, bufferLength, mCurrentAudioSample);
    // have we reached the end of the channel?
    if (audioLength == 0) {
      // we want to loop the video so reset the channel data and start again
      stop();
      setChannel(mChannelData->getChannelNumber());
      play();
      continue;
    }
    if (audioLength > 0) {
      // play the audio
      for(int i=0; i<audioLength; i+=1000) {
        mAudioOutput->write(audioData + i, min(1000, audioLength - i));
        mCurrentAudioSample += min(1000, audioLength - i);
        if (mState != VideoPlayerState::PLAYING)
        {
          mCurrentAudioSample = 0;
          mVideoSource->updateAudioTime(0);
          break;
        }
        mVideoSource->updateAudioTime(1000 * mCurrentAudioSample / 16000);
      }
    }
    else
    {
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
  }
}