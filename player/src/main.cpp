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
#include <Wire.h>

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

// MPR121 touch sensor for touch buttons
#ifdef HAS_MPR121

#include "Adafruit_MPR121.h"

#ifndef _BV
#define _BV(bit) (1 << (bit)) 
#endif

// You can have up to 4 on one i2c bus but one is enough for testing!
Adafruit_MPR121 cap = Adafruit_MPR121();

// Keeps track of the last pins touched
// so we know when buttons are 'released'
uint16_t lasttouched = 0;
uint16_t currtouched = 0;
bool touchChanged = false;

#ifdef MPR121_INTERRUPT_PIN
void IRAM_ATTR mpr121_isr() {
	//Serial.println("mpr121 interrupt");
  touchChanged = true;
}
#endif

void mpr121Init() {
  Serial.println("Adafruit MPR121 Capacitive Touch sensor"); 
  
  // Default address is 0x5A, if tied to 3.3V its 0x5B
  // If tied to SDA its 0x5C and if SCL then 0x5D
  if (!cap.begin(0x5A)) {
    Serial.println("MPR121 not found, check wiring?");
    while (1);
  }
  Serial.println("MPR121 found!");

  #ifdef MPR121_INTERRUPT_PIN
  pinMode(MPR121_INTERRUPT_PIN, INPUT_PULLUP);
	attachInterrupt(MPR121_INTERRUPT_PIN, mpr121_isr, FALLING);
  #else
  touchChanged = true; // always check touch status
  #endif
}

#endif
// -- End of MPR121 touch sensor

// -- SSD1306 OLED display --
#ifdef HAS_SSD1306

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#endif
// -- End of SSD1306 OLED display --

int channel = 0;
int volume = 0;
bool channelChanged = false;
bool volumeChanged = false;
bool useOled = false;

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

  // 1-bit mode uses CLK, CMD (MOSI), D0 (MISO) 
  SDCard *card = new SDCard(SD_CARD_CLK, SD_CARD_CMD, SD_CARD_D0);

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

  #ifdef HAS_MPR121
  mpr121Init();
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

#ifdef HAS_SSD1306
  // initialize and clear display
  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
  } else {
    // Clear the display buffer.
    oled.clearDisplay();
    // Set the text size.
    oled.setTextSize(2);
    // Set the text color.
    oled.setTextColor(SSD1306_WHITE);
    // Set the cursor position.
    oled.setCursor(0, 0);
    // Print the text.
    oled.println("Tuning...");
    oled.display();
    useOled = true;
    channelChanged = true;

    //pinMode(35, INPUT);
  }
#endif

// ----------- Start Playback -------------

//#ifndef HAS_IR_REMOTE

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
  volume = audioOutput->getVolume();

//#endif

  // #ifdef M5CORE2
  // audioOutput->setVolume(4);
  // #endif
}



void volumeUp() {
  audioOutput->volumeUp();
  volume = audioOutput->getVolume();
  videoPlayer->volumeUpdated();
  delay(100);
  Serial.println("VOLUME_UP");
  volumeChanged = true;
}

void volumeDown() {
  audioOutput->volumeDown();
  volume = audioOutput->getVolume();
  videoPlayer->volumeUpdated();
  delay(100);
  Serial.println("VOLUME_DOWN");
  volumeChanged = true;
}

void channelDown() {
  // This works
  int newChannel = channel - 1;
  if (newChannel < 0) {
    newChannel = channelData->getChannelCount() - 1;
  }
  videoPlayer->stop();
  display.fillScreen(0x0000);
  delay(100);
  display.drawTuningText();
  channel = newChannel;
  videoPlayer->setChannel(channel);
  
  delay(200);
  videoPlayer->play();
  Serial.printf("CHANNEL_DOWN %d\n", channel);
  channelChanged = true;
}

void channelUp() {
  // This works
  int newChannel = (channel + 1) % channelData->getChannelCount();
  videoPlayer->stop();
  display.fillScreen(0x0000);
  delay(100);
  display.drawTuningText();
  channel = newChannel;
  videoPlayer->setChannel(channel);
  delay(200);
  videoPlayer->play();
  Serial.printf("CHANNEL_UP %d\n", channel);
  channelChanged = true;
}

bool doDelay = true;

int timeStart = 0;

void loop()
{
  if (timeStart = 0) {
    timeStart = millis();
  }
#ifdef HAS_SSD1306
  // if (millis() - timeStart > 1000) {
  //   oled.clearDisplay();
  //   oled.setCursor(0,0);
  //   oled.println(analogRead(35));
  //   timeStart = millis();
  // }
  if (useOled && (channelChanged || volumeChanged)) {
    // Clear the display buffer.
    oled.clearDisplay();
    // Set the text size.
    oled.setTextSize(2);
    // Set the text color.
    oled.setTextColor(SSD1306_WHITE);
    // Set the cursor position.
    oled.setCursor(0, 0);
    // Print the text.
    oled.println("CHAN: " + String(channel) + " | VOL: " + String(volume));
    // Display the content of the display buffer.
    oled.display();
  }
#endif

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

// Handle MPR121 touch buttons:
#ifdef HAS_MPR121
  doDelay = false;

  if (touchChanged) {

  // Get the currently touched pads
  currtouched = cap.touched();
  
  for (uint8_t i=0; i<4; i++) {
    // if it *is* touched and *wasnt* touched before, alert!
    if ((currtouched & _BV(i)) && !(lasttouched & _BV(i)) ) {
      Serial.print(i); Serial.println(" touched");
      switch(i) {
        case MPR121_CHANNEL_UP_KEY:
          Serial.println("CHANNEL UP");
          channelUp();
          break;
        case MPR121_CHANNEL_DOWN_KEY:
          Serial.println("CHANNEL DOWN");
          channelDown();
          break;
        case MPR121_VOL_UP_KEY:
          Serial.println("VOL UP");
          volumeUp();
          break;
        case MPR121_VOL_DOWN_KEY:
          Serial.println("VOL DOWN");
          volumeDown();
          break;        
      }
    }
    // // if it *was* touched and now *isnt*, alert!
    // if (!(currtouched & _BV(i)) && (lasttouched & _BV(i)) ) {
    //   Serial.print(i); Serial.println(" released");
    // }
  }

  // reset our state
  lasttouched = currtouched;

  #ifdef MPR121_INTERRUPT_PIN
  touchChanged = false;
  #endif

  }
#endif

// Handle button presses:
#ifdef HAS_BUTTONS
  doDelay = false;

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

#endif

//if (doDelay)
if (true)
{
  // important this needs to stay otherwise we are constantly polling the IR Remote
  // and there's no time for anything else to run.
  delay(200);
}

}
