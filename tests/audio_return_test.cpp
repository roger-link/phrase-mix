#include "../Source/AudioReturn.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

int main(){
 float left[]{0.1f,-0.2f,0.9f};
 float right[]{-0.1f,0.2f,-0.9f};
 const float inputLeft[]{0.4f,0.6f,1.0f};
 const float inputRight[]{-0.4f,-0.6f,-1.0f};
 const float peak=mixStereoReturn(left,right,inputLeft,inputRight,3,0.5f);
 assert(std::abs(peak-1.0f)<0.0001f);
 assert(std::abs(left[0]-0.3f)<0.0001f&&std::abs(right[0]+0.3f)<0.0001f);
 assert(std::abs(left[1]-0.1f)<0.0001f&&std::abs(right[1]+0.1f)<0.0001f);
 assert(left[2]==1.0f&&right[2]==-1.0f);

 float dryLeft[]{0.25f};
 float dryRight[]{-0.25f};
 const float invalid[]{std::numeric_limits<float>::quiet_NaN()};
 mixStereoReturn(dryLeft,dryRight,invalid,invalid,1,1.0f);
 assert(dryLeft[0]==0.25f&&dryRight[0]==-0.25f);
 std::cout<<"Stereo audio return checks passed\n";
}
