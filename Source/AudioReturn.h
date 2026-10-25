#pragma once
#include <algorithm>
#include <cmath>

inline float mixStereoReturn(float* left,float* right,const float* inputLeft,const float* inputRight,int samples,float gain){
 float peak=0.0f;
 gain=std::clamp(gain,0.0f,1.0f);
 for(int i=0;i<samples;++i){
  const float inLeft=std::isfinite(inputLeft[i])?inputLeft[i]:0.0f;
  const float inRight=std::isfinite(inputRight[i])?inputRight[i]:0.0f;
  peak=std::max(peak,std::max(std::abs(inLeft),std::abs(inRight)));
  left[i]=std::clamp(left[i]+inLeft*gain,-1.0f,1.0f);
  right[i]=std::clamp(right[i]+inRight*gain,-1.0f,1.0f);
 }
 return peak;
}
