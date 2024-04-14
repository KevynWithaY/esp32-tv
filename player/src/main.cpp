#include <Arduino.h>
#include <WiFi.h>
#include "Displays/TFT.h"
#include "Displays/Matrix.h"
#include "RemoteInput.h"
#include "VideoPlayer.h"
#include "AudioOutput/I2SOutput.h"
#include "AudioOutput/DACOutput.h"
#include "AudioOutput/PDMTimerOutput.h"
#include "AudioOutput/PDMOutput.h"
#include "ChannelData/NetworkChannelData.h"
#include "ChannelData/SDCardChannelData.h"
#include "AudioSource/NetworkAudioSource.h"
#include "VideoSource/NetworkVideoSource.h"
#include "AudioSource/SDCardAudioSource.h"
#include "VideoSource/SDCardVideoSource.h"
#include "AVIParser/AVIParser.h"
#include "SDCard.h"
#include "PowerUtils.h"
#include "Button.h"
#include "driver/sdmmc_types.h"

const char *WIFI_SSID = "CMGResearch";
const char *WIFI_PASSWORD = "02087552867";
const char *FRAME_URL = "http://192.168.1.229:8123:8123/frame";
const char *AUDIO_URL = "http://192.168.1.229:8123/audio";
const char *CHANNEL_INFO_URL = "http://192.168.1.229:8123/channel_info";

#ifdef HAS_IR_REMOTE
RemoteInput *remoteInput = NULL;
#else
#ifndef HAS_BUTTONS
#warning "No Remote Input - Will default to playing channel 0"
#endif
#endif

#ifndef USE_DMA
#warning "No DMA - Drawing may be slower"
#endif

VideoSource *videoSource = NULL;
AudioSource *audioSource = NULL;
VideoPlayer *videoPlayer = NULL;
AudioOutput *audioOutput = NULL;
ChannelData *channelData = NULL;
#ifdef LED_MATRIX
Matrix display;
#else
TFT display;
#endif

void setup()
{
  Serial.begin(115200);
  Serial.printf("Total heap: %d\n", ESP.getHeapSize());
  Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
  Serial.printf("Total PSRAM: %d\n", ESP.getPsramSize());
  Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());
  powerInit();
  buttonInit();

  #ifdef USE_SDCARD
  Serial.println("Using SD Card");

  // power on the SD card
  #ifdef SD_CARD_PWR
  if (SD_CARD_PWR != GPIO_NUM_NC) {
    pinMode(SD_CARD_PWR, OUTPUT);
    digitalWrite(SD_CARD_PWR, SD_CARD_PWR_ON);
  }
  #endif

  #ifdef USE_SDIO

    // 1-bit mode uses CLK, CMD, D0 while 4-bit mode uses CLK, CMD, D0, D1, D2, D3

    // Didn't need to pull this up in Arduino SD_MMC example, but ESP-IDF says:
    // CLK is GPIO 14, 10k pullup in SD mode
    // On DevKitC v4, this pin has a weak pull up, but it's not enabled by default.
    //pinMode(SD_CARD_CLK, INPUT_PULLUP); // Pin 14, CLK is CLK in SPI mode

    // CMD (MOSI in SPI mode) is GPIO 15, 10k pullup in SD mode
    // On DevKitC v4, this pin has a weak pull up, but it's not enabled by default.
    //pinMode(SD_CARD_CMD, INPUT_PULLUP); // Pin 15, CMD is MOSI in SPI mode

    // D0 (MISO in SPI mode) is GPIO 2, 10k pullup in SD mode, pull low to go into download mode (see Note about GPIO2 below!)
    // Internal pull-up available on DevKitC v4
    //pinMode(SD_CARD_D0, INPUT_PULLUP); // Pin 2, D0 is MISO in SPI mode

    // D3 (CS in SPI mode) is GPIO 13, 10k pullup in SD mode
    // Internal pull-up available on DevKitC v4
    //pinMode(SD_CARD_D3, INPUT_PULLUP); // Pin 13, D3 is CS in SPI mode
    
  // #ifdef USE_4BIT_MODE
  //   // D1 is GPIO 4, 10k pullup in SD mode
  //   // Internal pull-up available on DevKitC v4
  //   pinMode(SD_CARD_D1, INPUT_PULLUP); // Pin 4

  //   // D2 is GPIO 12, 10k pullup in SD mode
  //   // Internal pull-up available on DevKitC v4
  //   // Note: GPIO12 is used as a bootstrapping pin, so it must be low at reset, so must disconnect pull-up resistor to flash it.
  //   pinMode(SD_CARD_D2, INPUT_PULLUP); // Pin 12

  //   // D3 (CS in SPI mode) is GPIO 13, 10k pullup in SD mode
  //   // Internal pull-up available on DevKitC v4
  //   pinMode(SD_CARD_D3, INPUT_PULLUP); // Pin 13
  // #endif

    // Regarding GPIO 12:
    // https://github.com/espressif/esp-idf/tree/master/examples/storage/sd_card/sdmmc
    // On boards which use the internal regulator and a 3.3V flash chip, GPIO12 must be low at reset. This is incompatible with SD card operation.
    // In most cases, external pullup can be omitted and an internal pullup can be enabled using a gpio_pullup_en(GPIO_NUM_12); call. Most SD cards work fine when an internal pullup on GPIO12 line is enabled. Note that if ESP32 experiences a power-on reset while the SD card is sending data, high level on GPIO12 can be latched into the bootstrapping register, and ESP32 will enter a boot loop until external reset with correct GPIO12 level is applied.
    // Another option is to burn the flash voltage selection efuses. This will permanently select 3.3V output voltage for the internal regulator, and GPIO12 will not be used as a bootstrapping pin. Then it is safe to connect a pullup resistor to GPIO12. This option is suggested for production use.

  SDCard *card = new SDCard(SD_CARD_CLK, SD_CARD_CMD, SD_CARD_D0); //, SD_CARD_D1, SD_CARD_D2, SD_CARD_D3);
  // #else
  // pinMode(SD_CARD_CS, INPUT_PULLUP);
  // pinMode(SD_CARD_MISO, INPUT_PULLUP);
  // pinMode(SD_CARD_MOSI, INPUT_PULLUP);
  // pinMode(SD_CARD_CLK, INPUT_PULLUP);
  // SDCard *card = new SDCard(SD_CARD_MISO, SD_CARD_MOSI, SD_CARD_CLK, SD_CARD_CS);
  #endif

  channelData = new SDCardChannelData(card, "/");
  audioSource = new SDCardAudioSource((SDCardChannelData *) channelData);
  videoSource = new SDCardVideoSource((SDCardChannelData *) channelData);

  // #else

  // WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  // while (WiFi.status() != WL_CONNECTED)
  // {
  //   delay(500);
  //   Serial.print(".");
  // }
  // WiFi.setSleep(false);
  // WiFi.setTxPower(WIFI_POWER_19_5dBm);
  // Serial.println("");
  // // disable WiFi power saving for speed
  // Serial.println("WiFi connected");
  // channelData = new NetworkChannelData(CHANNEL_INFO_URL, FRAME_URL, AUDIO_URL);
  // videoSource = new NetworkVideoSource((NetworkChannelData *) channelData);
  // audioSource = new NetworkAudioSource((NetworkChannelData *) channelData);

  #endif

#ifdef HAS_IR_REMOTE
  remoteInput = new RemoteInput(IR_RECV_PIN, IR_RECV_PIN, IR_RECV_GND, IR_RECV_IND);
  remoteInput->start();
#endif

#ifdef USE_DAC_AUDIO
  audioOutput = new DACOutput(I2S_NUM_0);
  audioOutput->start(16000);
#endif
#ifdef PDM_GPIO_NUM
  // i2s speaker pins
  i2s_pin_config_t i2s_speaker_pins = {
      .bck_io_num = I2S_PIN_NO_CHANGE,
      .ws_io_num = GPIO_NUM_0,
      .data_out_num = PDM_GPIO_NUM,
      .data_in_num = I2S_PIN_NO_CHANGE};
  audioOutput = new PDMOutput(I2S_NUM_0, i2s_speaker_pins);
  audioOutput->start(16000);
#endif

#ifdef I2S_SPEAKER_SERIAL_CLOCK
#ifdef SPK_MODE
  pinMode(SPK_MODE, OUTPUT);
  digitalWrite(SPK_MODE, HIGH);
#endif
  // i2s speaker pins
  i2s_pin_config_t i2s_speaker_pins = {
      .bck_io_num = I2S_SPEAKER_SERIAL_CLOCK,
      .ws_io_num = I2S_SPEAKER_LEFT_RIGHT_CLOCK,
      .data_out_num = I2S_SPEAKER_SERIAL_DATA,
      .data_in_num = I2S_PIN_NO_CHANGE};

  audioOutput = new I2SOutput(I2S_NUM_1, i2s_speaker_pins);
  audioOutput->start(16000);
  
#endif

  // Start video player
  videoPlayer = new VideoPlayer(
    channelData,
    videoSource,
    audioSource,
    display,
    audioOutput
  );
  videoPlayer->start();

#ifndef HAS_IR_REMOTE

  // If no IR remote, just start playing channel 0
  // otherwise wait for power button on remote...

  display.drawTuningText();
  
  // get the channel info
  while(!channelData->fetchChannelData()) {
    Serial.println("Failed to fetch channel data");
    delay(1000);
  }
  
  // default to first channel
  videoPlayer->setChannel(0);
  videoPlayer->play();

#endif

  #ifdef M5CORE2
  audioOutput->setVolume(4);
  #endif
}

int channel = 0;

void volumeUp() {
  audioOutput->volumeUp();
  int volume = audioOutput->getVolume();
  videoPlayer->volumeUpdated();
  delay(100);
  Serial.println("VOLUME_UP");
}

void volumeDown() {
  audioOutput->volumeDown();
  int volume = audioOutput->getVolume();
  videoPlayer->volumeUpdated();
  delay(100);
  Serial.println("VOLUME_DOWN");
}

void channelDown() {
  // This works
  int newChannel = channel - 1;
  if (newChannel < 0) {
    newChannel = channelData->getChannelCount() - 1;
  }
  videoPlayer->stop();
  display.drawTuningText();
  channel = newChannel;
  videoPlayer->setChannel(channel);
  videoPlayer->play();
  Serial.printf("CHANNEL_DOWN %d\n", channel);
}

void channelUp() {
  // This works
  int newChannel = (channel + 1) % channelData->getChannelCount();
  videoPlayer->stop();
  display.drawTuningText();
  channel = newChannel;
  videoPlayer->setChannel(channel);
  videoPlayer->play();
  Serial.printf("CHANNEL_UP %d\n", channel);
}

void loop()
{
#ifdef HAS_IR_REMOTE
  RemoteCommands command = remoteInput->getLatestCommand();
  if (command != RemoteCommands::UNKNOWN)
  {
    switch (command)
    {
    case RemoteCommands::POWER:
      // log out RAM usage
      Serial.printf("Total heap: %d\n", ESP.getHeapSize());
      Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
      Serial.printf("Total PSRAM: %d\n", ESP.getPsramSize());
      Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());

      videoPlayer->stop();
      display.drawTuningText();
      Serial.println("POWER");
      // get the channel info
      while(!channelData->fetchChannelData()) {
        Serial.println("Failed to fetch channel data");
        delay(1000);
      }
      videoPlayer->setChannel(0);
      videoPlayer->play();
      break;
    case RemoteCommands::VOLUME_UP:
      volumeUp();
      break;
    case RemoteCommands::VOLUME_DOWN:
      volumeDown();
      break;
    case RemoteCommands::CHANNEL_UP:
      channelUp();
      break;
    case RemoteCommands::CHANNEL_DOWN:
      channelDown();
      break;
    }
    delay(100);
    remoteInput->getLatestCommand();
  }
#endif

// Handle button presses:
#ifdef HAS_BUTTONS

  if (buttonLeft()) {
    Serial.println("LEFT");
    channelDown();
  }

  if (buttonRight()) {
    Serial.println("RIGHT");
    channelUp();
  }

  if (buttonUp()) {
    Serial.println("UP");
    volumeUp();
  }

  if (buttonDown()) {
    Serial.println("DOWN");
    volumeDown();
  }

  // if (buttonPowerOff()) {
  //   Serial.println("POWER OFF");
  //   delay(500);
  //   powerDeepSeep();
  // }

  buttonLoop();
  delay(200); // todo: is this needed?

#else
    // important this needs to stay otherwise we are constantly polling the IR Remote
    // and there's no time for anything else to run.
    delay(200);
#endif
}
