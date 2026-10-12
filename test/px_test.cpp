#include "Arduino.h"
unsigned long g_ms=0; int g_pwm[64]={0}; void (*g_isr[2])()={nullptr,nullptr}; MockSerial Serial;
void sim_step_1ms(){g_ms++;}
int digitalRead(int){return 0;}
#include "../firmware/px_merged_test/px_merged_test.ino"
void send(const char*s){ for(;*s;s++) Serial.in.push_back(*s); }
void run(unsigned long ms){ unsigned long e=g_ms+ms; while(g_ms<e){ loop(); sim_step_1ms(); } }
int main(){ setup(); Serial.out.clear();
  int sh=pixels.shows; run(1000); printf("rainbow running: %d strip updates in 1 s\n", pixels.shows-sh);
  unsigned long t=g_ms; send("#255,120,0#"); loop(); printf("custom colour handled after %lu ms -> pixel0=0x%06X (expect 0xFF7800)\n", g_ms-t, (unsigned)pixels.px[0]);
  sh=pixels.shows; run(1000); printf("strip stays custom (no rainbow updates): %d updates, pixel0=0x%06X\n", pixels.shows-sh,(unsigned)pixels.px[0]);
  send("#300,0,0#"); send("#abc#"); send("#1,2#"); send("#1,2,3,4#"); run(10);
  send("#RAINBOW#"); sh=pixels.shows; run(500); printf("back to rainbow: %d updates in 0.5 s\n", pixels.shows-sh);
  // motors: sample the H-bridge pins over 8 s
  printf("motor cycle (IN1 IN2 IN3 IN4) at t=0.5,2.5,4.5,6.5 s from now:");
  for(int k=0;k<4;k++){ run(k==0?500:2000); printf("  [%d%d%d%d]", g_pwm[8]>0,g_pwm[9]>0,g_pwm[10]>0,g_pwm[11]>0); }
  printf("\nserial log:\n%s", Serial.out.c_str());
}
