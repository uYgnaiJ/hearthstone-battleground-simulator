#pragma once
#include "raylib.h"
#include <vector>
#include <cmath>
#include <random>
struct GameAudio {
 Sound coin{},hit{},summon{},click{},shield{};bool ready=false;float volume=.4f;
 Sound make(int kind){int n=kind==2?10000:kind==1?5000:6000;std::vector<float> samples(n);unsigned rng=143;for(int i=0;i<n;i++){float t=i/22050.f,envelope=expf(-t*(kind==2?7:16));rng=1664525*rng+1013904223;float noise=((rng>>16)/32768.f-1);float v=0;
 if(kind==0)v=(sinf(2*PI*1100*t)+.45f*sinf(2*PI*1648*t))*.22f;
 if(kind==1)v=(sinf(2*PI*(100-80*t)*t)*.6f+noise*.4f)*.4f;
 if(kind==2)v=(sinf(2*PI*(440+700*t)*t)+sinf(2*PI*660*t)*.4f)*.19f;
 if(kind==3)v=noise*.15f+sinf(2*PI*250*t)*.18f;
 if(kind==4)v=noise*.32f+sinf(2*PI*2800*t)*.1f;
 samples[i]=v*envelope;
 }Wave w{(unsigned)n,22050,32,1,samples.data()};return LoadSoundFromWave(w);}
 void init(){InitAudioDevice();ready=IsAudioDeviceReady();if(ready){coin=make(0);hit=make(1);summon=make(2);click=make(3);shield=make(4);}}
 void play(int kind){if(!ready||volume<=0)return;Sound s=kind==0?coin:kind==1?hit:kind==2?summon:kind==4?shield:click;SetSoundVolume(s,volume);PlaySound(s);}
 void close(){if(ready){UnloadSound(coin);UnloadSound(hit);UnloadSound(summon);UnloadSound(click);UnloadSound(shield);CloseAudioDevice();}}
};
