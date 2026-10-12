#pragma once
#include "Arduino.h"
#define NEO_GRB 0
#define NEO_KHZ800 0
struct Adafruit_NeoPixel { uint32_t px[8]={0}; int shows=0;
  Adafruit_NeoPixel(int,int,int){} void begin(){} void show(){shows++;}
  static uint32_t Color(uint8_t r,uint8_t g,uint8_t b){return ((uint32_t)r<<16)|((uint32_t)g<<8)|b;}
  void setPixelColor(int i,uint32_t c){px[i]=c;} void setPixelColor(int i,int r,int g,int b){px[i]=Color(r,g,b);}
  uint32_t ColorHSV(uint16_t h){return 0x100000|h;} uint32_t gamma32(uint32_t c){return c;} int numPixels(){return 8;} };
