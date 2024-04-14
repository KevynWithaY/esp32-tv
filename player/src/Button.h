#include <Arduino.h>
#ifdef M5CORE2
#include "M5Touch.h"
M5Touch Touch;
TouchPoint_t pos;   
#endif

int _btn_left=-1;
int _btn_right=-1;
int _btn_up=-1;
int _btn_down=-1;

bool buttonRight() {
#ifdef M5CORE2
  if (pos.x > 200 && pos.y > 90 && pos.y < 180) return true;
#endif

#ifdef BUTTON_R
  return (_btn_right == 0);
#endif
  return false;
}

bool buttonLeft(){
#ifdef M5CORE2
  if (pos.x > 0 && pos.x < 100 && pos.y > 90 && pos.y < 180) return true;
#endif

#ifdef BUTTON_L
  return (_btn_left == 0);
#endif
  return false;
}

bool buttonUp(){
#ifdef M5CORE2
  if (pos.x > 106 && pos.x < 212 && pos.y > 0 && pos.y < 90) return true;
#endif

#ifdef BUTTON_UP
  return (_btn_up == 0);
#endif
  return false;
}

bool buttonDown(){
#ifdef M5CORE2
  if (pos.x > 106 && pos.x < 212 && pos.y > 180 && pos.y < 280) return true;
#endif

#ifdef BUTTON_DOWN
  return (_btn_down == 0);
#endif
  return false;
}

bool buttonPowerOff() {
  // Press channel up/down buttons at same time to power off
#ifdef BUTTON_L
  #ifdef BUTTON_R
    return false; //return (_btn_left == 0 && _btn_right == 0);
  #endif
#endif
  return false;
}

void buttonInit(){

#ifdef BUTTON_L
  #ifdef BUTTON_R
  _btn_left = 1;
  _btn_right = 1;
  //pinMode(BUTTON_L, INPUT_PULLUP);
  //pinMode(BUTTON_R, INPUT_PULLUP);
  //pinMode(BUTTON_L, INPUT); // touch
  //pinMode(BUTTON_R, INPUT); // touch
  #endif
#endif

#ifdef BUTTON_UP
  _btn_up = 1;
  pinMode(BUTTON_UP, INPUT_PULLUP);
#endif

#ifdef BUTTON_DOWN
  _btn_down = 1;
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
#endif

#ifdef M5CORE2
  Touch.begin();
#endif
}

void buttonLoop(){
  static uint_fast64_t buttonTimeStamp = 0;
  if (millis() - buttonTimeStamp > 50) {
    buttonTimeStamp = millis();

    #ifdef M5CORE2
    pos = Touch.getPressPoint();
    // Serial.printf("(%i,%i)",pos.x,pos.y);
    #endif

    #ifdef BUTTON_L
      #ifdef BUTTON_R
      //_btn_right = digitalRead(BUTTON_R);
      //_btn_left = digitalRead(BUTTON_L);
      //Serial.println("button left: " + touchRead(BUTTON_L));
      //Serial.println("button right: " + touchRead(BUTTON_R));
      if (touchRead(BUTTON_L) < 50) _btn_left = 0;
      else _btn_left = 1;
      if (touchRead(BUTTON_R) < 50) _btn_right = 0;
      else _btn_right = 1;  
      #endif
    #endif

    #ifdef BUTTON_UP
    _btn_up = digitalRead(BUTTON_UP);
    #endif
    #ifdef BUTTON_DOWN
    _btn_down = digitalRead(BUTTON_DOWN);
    #endif
  }
}


