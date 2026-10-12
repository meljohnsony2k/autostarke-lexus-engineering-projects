// Minimal host mock of the Arduino API + a simulated DC motor with encoder.
#pragma once
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <deque>
#include <algorithm>
#define F(x) x
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define RISING 3
using std::fabs;
inline long constrain(long x,long a,long b){return x<a?a:(x>b?b:x);}
extern unsigned long g_ms; extern int g_pwm[64]; extern void (*g_isr[2])();
void sim_step_1ms();
inline unsigned long millis(){return g_ms;}
inline void delay(unsigned long ms){for(unsigned long i=0;i<ms;i++) sim_step_1ms();}
inline void pinMode(int,int){}
int digitalRead(int p);
inline bool isPwmPin(int p){return p==3||p==5||p==6||p==9||p==10||p==11;}
inline void analogWrite(int p,int v){ if(v<0)v=0; if(v>255)v=255; g_pwm[p]= isPwmPin(p)? v : (v>=128?255:0);}
inline void digitalWrite(int p,int v){g_pwm[p]=v?255:0;}
int digitalRead(int p);
inline int digitalPinToInterrupt(int p){return p==2?0:(p==3?1:-1);}
inline void attachInterrupt(int n,void(*f)(),int){ if(n>=0&&n<2) g_isr[n]=f;}
inline void noInterrupts(){} inline void interrupts(){}
struct MockSerial {
  std::deque<char> in; std::string out;
  void begin(long){}
  int available(){return (int)in.size();}
  int read(){ if(in.empty()) return -1; char c=in.front(); in.pop_front(); return c;}
  template<class T> void print(T v){ out += std::to_string(v);} void print(const char*s){out+=s;} void print(char c){out+=c;}
  template<class T> void println(T v){ print(v); out+="\n";} void println(){out+="\n";}
  // Arduino Stream::parseFloat: skip non-numeric chars, wait up to 1000 ms for data, return 0 on timeout
  float parseFloat(){
    std::string num;
    for(;;){
      if(in.empty()){ if(!num.empty()) break; delay(1000); if(in.empty()) return 0; continue; }
      char c=in.front();
      if((c>='0'&&c<='9')||c=='-'||c=='.'){ num+=c; in.pop_front(); }
      else if(num.empty()){ in.pop_front(); }
      else break;
    }
    return (float)atof(num.c_str());
  }
};
extern MockSerial Serial;
