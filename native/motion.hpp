#pragma once
#include <algorithm>
#include <cmath>
namespace motion {
inline constexpr float impact=.36f;
inline float smooth(float t){t=std::clamp(t,0.f,1.f);return t*t*(3-2*t);}
// Anticipation, fast contact, a short impact hold, then a slower return.
inline float attackTravel(float t){if(t<.18f)return -.075f*smooth(t/.18f);if(t<impact){float k=(t-.18f)/(impact-.18f);return -.075f+.88f*k*k*k;}if(t<.47f)return .805f;if(t<.90f)return .805f*(1-smooth((t-.47f)/.43f));return 0;}
inline float recoil(float t){if(t<impact||t>.72f)return 0;float k=(t-impact)/(.72f-impact);return std::sin(k*3.14159265f)*std::exp(-k*3);}
}
