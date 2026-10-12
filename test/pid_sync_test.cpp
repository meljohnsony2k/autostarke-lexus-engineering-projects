#ifndef WEAK
#define WEAK 0.85
#endif
#include "Arduino.h"
unsigned long g_ms=0; int g_pwm[64]={0}; void (*g_isr[2])()={nullptr,nullptr}; MockSerial Serial;
double pos[2]={0,0}, vel[2]={0,0}, acc[2]={0,0}; double VMAX[2]={600,600*WEAK};
int MF[2]={5,10},MR[2]={6,11};
void sim_step_1ms(){ g_ms++; for(int i=0;i<2;i++){ int u=g_pwm[MF[i]]-g_pwm[MR[i]]; double t=(abs(u)<30)?0:VMAX[i]*u/255.0; vel[i]+=(t-vel[i])*(0.001/0.08); double d=vel[i]*0.001; pos[i]+=d; acc[i]+=fabs(d); while(acc[i]>=1){acc[i]-=1; if(g_isr[i]) g_isr[i]();} } }
int digitalRead(int p){ if(p==4) return vel[0]>=0; if(p==7) return vel[1]>=0; return 1; }
void setup(); void loop();
#include "../firmware/lms_pid_actuator_prototype/lms_pid_actuator_prototype.ino"
void runms(unsigned long ms){ unsigned long e=g_ms+ms; double maxgap=0; while(g_ms<e){ loop(); sim_step_1ms(); maxgap=std::max(maxgap,fabs(pos[0]-pos[1])); } printf("   M1 %7.1f  M2 %7.1f  max M1-M2 gap during move %5.1f deg\n",pos[0],pos[1],maxgap); }
int main(){ setup();
  for(char c: std::string("200\n")) Serial.in.push_back(c); runms(3000);
  printf("-- send a blank line (just Enter):\n"); Serial.in.push_back('\n'); runms(3000);
  for(char c: std::string("-150\n")) Serial.in.push_back(c); printf("-- send -150:\n"); runms(3000);
}
