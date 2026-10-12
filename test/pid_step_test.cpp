#include "Arduino.h"
unsigned long g_ms=0; int g_pwm[64]={0}; void (*g_isr[2])()={nullptr,nullptr}; MockSerial Serial;
// Two motor channels. Map: original sketch uses pins (5,6) and (8,9); fixed uses (5,6) and (10,11).
int MF[2],MR[2]; int ENCI[2]; // encoder interrupt per motor (-1 = none)
double pos[2]={0,0}, vel[2]={0,0}, acc_count[2]={0,0};
const double VMAX=600.0;   // deg/s at full PWM
const double TAU=0.08;     // s, motor time constant
const int STICTION=30;     // PWM below this does not move the motor
const double CPD=1.0;      // encoder counts per degree (360 counts/rev)
void sim_step_1ms(){
  g_ms++;
  for(int i=0;i<2;i++){
    int u=g_pwm[MF[i]]-g_pwm[MR[i]];
    double target = (abs(u)<STICTION)?0.0:VMAX*u/255.0;
    vel[i]+= (target-vel[i])*(0.001/TAU);
    double d=vel[i]*0.001; pos[i]+=d;
    acc_count[i]+=fabs(d)*CPD;              // single channel: pulses regardless of direction
    while(acc_count[i]>=1.0){ acc_count[i]-=1.0; if(ENCI[i]>=0&&g_isr[ENCI[i]]) g_isr[ENCI[i]](); }
  }
}
int digitalRead(int p){ if(p==4) return vel[0]>=0; if(p==7) return vel[1]>=0; return 1; }
void setup(); void loop();
#include "../firmware/lms_pid_actuator_prototype/lms_pid_actuator_prototype.ino"
struct Res{double t_settle, overshoot, final_err, final_err2; int sign_flips;};
// run until t_end (ms), record motor 1 true position
Res run_to(double sp, unsigned long t_end, bool blocking_loop){
  Res r{-1,0,0,0,0}; double start=pos[0]; double maxpast=0; int lastdir=0;
  unsigned long t0=g_ms;
  while(g_ms<t_end){
    if(blocking_loop) loop(); else { loop(); sim_step_1ms(); }
    double e=sp-pos[0];
    double past = (sp>=start)? (pos[0]-sp) : (sp-pos[0]); if(past>maxpast) maxpast=past;
    int dir = vel[0]>5?1:(vel[0]<-5?-1:0); if(dir!=0&&lastdir!=0&&dir!=lastdir) r.sign_flips++; if(dir!=0) lastdir=dir;
    if(fabs(e)<=5 && r.t_settle<0) r.t_settle=(g_ms-t0)/1000.0;
    if(fabs(e)>5) r.t_settle=-1;
  }
  r.overshoot=maxpast; r.final_err=sp-pos[0]; r.final_err2=sp-pos[1]; return r;
}
int main(){
  MF[0]=5;MR[0]=6; MF[1]=10;MR[1]=11; ENCI[0]=0; ENCI[1]=1;
  setup();
  struct {const char* in; double sp;} steps[]={{"100\n",100},{"245\n",245},{"-90\n",-90},{"0\n",0}};
  for(auto &s:steps){
    for(const char*p=s.in;*p;p++) Serial.in.push_back(*p);
    Res r=run_to(s.sp, g_ms+8000, false);
    printf("setpoint %6.1f | M1 true pos %8.1f  err %7.1f | M2 err %7.1f | overshoot %6.1f deg | direction reversals %3d | settled(<=5deg) %s\n",
      s.sp, pos[0], r.final_err, r.final_err2, r.overshoot, r.sign_flips, r.t_settle>=0? (std::to_string(r.t_settle).substr(0,4)+" s").c_str():"NO");
  }
  // what does the sketch itself think the position is?
  size_t k=Serial.out.rfind('\n', Serial.out.size()-2); printf("last serial line: %s", Serial.out.substr(k+1).c_str());
}
