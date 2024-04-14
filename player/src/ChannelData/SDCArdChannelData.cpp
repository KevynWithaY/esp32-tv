#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "../SDCard.h"
#include "SDCardChannelData.h"
#include "../AVIParser/AVIParser.h"

SDCardChannelData::SDCardChannelData(SDCard *sdCard, const char *aviPath): mSDCard(sdCard), mAviPath(aviPath) {

}

bool SDCardChannelData::fetchChannelData() {
  Serial.printf("SDCardChannelData::fetchChannelData for path %s\n", mAviPath);
  // check the the sd card is mounted
  if (!mSDCard->isMounted()) {
    Serial.println("SD card is not mounted");
    return false;
  }
  // get the list of AVI files
  mAviFiles = mSDCard->listFiles(mAviPath, ".avi");
  if (mAviFiles.size() == 0) {
    Serial.println("No AVI files found");
    return false;
  }
  return true;
}

// int fileFPS = 0;
// int SDCardChannelData::getFileFPS() {
//   return fileFPS;
// }


void SDCardChannelData::setChannel(int channel) {
  Serial.printf("SDCardChannelData::setChannel to %d\n", channel);
  
  if (!mSDCard->isMounted()) {
    Serial.println("SD card is not mounted");
    return;
  }
  // check that the channel is valid
  if (channel < 0 || channel >= mAviFiles.size()) {
    Serial.printf("Invalid channel %d\n", channel);
    return;
  }
  // close any open AVI files
  if (mCurrentChannelAudioParser) {
    delete mCurrentChannelAudioParser;
    mCurrentChannelAudioParser = NULL;
  }
  if (mCurrentChannelVideoParser) {
    delete mCurrentChannelVideoParser;
    mCurrentChannelVideoParser = NULL;
  }
  // open the AVI file
  std::string aviFilename = mAviFiles[channel];

  // check if the filename contains "_15fps"
  if (aviFilename.find("_15fps") != std::string::npos) {
    // do something
    fileFPS = 15;
  }
  else if (aviFilename.find("_24fps") != std::string::npos) {
    // do something
    fileFPS = 24;
  }
  else if (aviFilename.find("_30fps") != std::string::npos) {
    // do something
    fileFPS = 30;
  }
  else {
    // do something
    fileFPS = 0;
  }
  Serial.printf("File FPS: %d\n", fileFPS);

  Serial.printf("Opening AVI file %s\n", aviFilename.c_str());
  mCurrentChannelAudioParser = new AVIParser(aviFilename, AVIChunkType::AUDIO);
  if (!mCurrentChannelAudioParser->open()) {
    Serial.printf("Failed to open AVI file %s\n", aviFilename.c_str());
    delete mCurrentChannelAudioParser;
    mCurrentChannelAudioParser = NULL;
  }
  mCurrentChannelVideoParser = new AVIParser(aviFilename, AVIChunkType::VIDEO);
  if (!mCurrentChannelVideoParser->open()) {
    Serial.printf("Failed to open AVI file %s\n", aviFilename.c_str());
    delete mCurrentChannelVideoParser;
    mCurrentChannelVideoParser = NULL;
    delete mCurrentChannelAudioParser;
    mCurrentChannelAudioParser = NULL;
  }
  mChannelNumber = channel;
}