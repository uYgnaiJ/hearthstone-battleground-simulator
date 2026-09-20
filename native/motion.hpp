#pragma once
#include <algorithm>
#include <cmath>
namespace motion {
inline constexpr float impact=.36f;
inline constexpr float phaseDuration=1.12f;
struct CornerFlip{float angle;bool incoming;};
inline CornerFlip cornerFlip(float clock,int corner,bool toCombat){
 float delay=(corner%2)*.045f+(corner/2)*.065f;
 float t=std::clamp((clock-delay)/.90f,0.f,1.f);
 float eased=t*t*(3-2*t);
 bool incoming=eased>=.5f;
 // Turn around a horizontal axle. The reverse face stays upright.
 float direction=(corner<2?1.f:-1.f)*(toCombat?1.f:-1.f);
 return {direction*(eased-(incoming?1.f:0.f))*3.14159265f,incoming};
}
inline float smooth(float t){t=std::clamp(t,0.f,1.f);return t*t*(3-2*t);}
// Anticipation, fast contact, a short impact hold, then a slower return.
inline float attackTravel(float t){if(t<.18f)return -.075f*smooth(t/.18f);if(t<impact){float k=(t-.18f)/(impact-.18f);return -.075f+.88f*k*k*k;}if(t<.47f)return .805f;if(t<.90f)return .805f*(1-smooth((t-.47f)/.43f));return 0;}
inline float recoil(float t){if(t<impact||t>.72f)return 0;float k=(t-impact)/(.72f-impact);return std::sin(k*3.14159265f)*std::exp(-k*3);}
}
