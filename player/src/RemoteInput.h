#pragma once

#include <hal/gpio_types.h>

// enum for the remote control commands
enum class RemoteCommands
{
  POWER = 0x0,
  VOLUME_UP = 0x1,
  VOLUME_DOWN = 0x9,
  CHANNEL_UP = 0xA,
  CHANNEL_DOWN = 8,
  ZERO = 0xC,
  ONE = 0x10,
  TWO = 0x11,
  THREE = 0x12,
  FOUR = 0x14,
  FIVE = 0x15,
  SIX = 0x16,
  SEVEN = 0x18,
  EIGHT = 0x19,
  NINE = 0x1A,
  PLAY_PAUSE = 0x5,
  REWIND = 0x4,
  FAST_FORWARD = 0x6,
  STOP = 0x2,
  UNKNOWN = -1
};

class RemoteInput
{
  private:
    int m_input_pin;
    int m_pwr_pin;
    int m_gnd_pin;
    int m_led_pin;
  public:
    RemoteInput(int input_pin, int pwr_pin = -1, int gnd_pin = -1, int led_pin = -1);
    void start();
    void stop();
    RemoteCommands getLatestCommand();
};