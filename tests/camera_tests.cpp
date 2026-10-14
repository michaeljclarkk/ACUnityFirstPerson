#include "camera_math.h"
#include <cstdio>
#include <limits>
#include <cstdlib>
void Check(bool ok,const char* message) {
    if(!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
bool Near(float a,float b) {return std::abs(a-b)<0.0001f;}
int main() {
    fp::Vec3 out{};
    Check(fp::EyePosition({10,20,30},{0,0,0,1},1.65f,0.25f,out),"identity accepted");
    Check(Near(out.x,10)&&Near(out.y,20.25f)&&Near(out.z,31.65f),"identity uses Y forward and Z up");
    const float h=std::sqrt(0.5f);
    Check(fp::EyePosition({0,0,0},{0,0,h,h},1.65f,0.25f,out),"yaw accepted");
    Check(Near(out.x,-0.25f)&&Near(out.y,0)&&Near(out.z,1.65f),"90 degree yaw follows camera");
    Check(fp::EyePosition({0,0,0},{h,0,0,h},1.65f,0.25f,out),"pitch accepted");
    Check(Near(out.y,0)&&Near(out.z,1.9f),"pitch rotates forward offset");
    Check(!fp::EyePosition({0,0,0},{0,0,0,0},1.65f,0.25f,out),"zero quaternion rejected");
    Check(!fp::EyePosition({std::numeric_limits<float>::quiet_NaN(),0,0},{0,0,0,1},1,0,out),"NaN root rejected");
    Check(!fp::EyePosition({0,0,0},{0,0,0,std::numeric_limits<float>::infinity()},1,0,out),"infinite quaternion rejected");
    Check(fp::LevelEyePosition({10,20,30},{h,0,0,h},0.12f,0.16f,out),"vertical look accepted");
    Check(Near(out.z,30.12f),"looking down/up never lowers the eye into the neck");
    const float tilt=0.6f;
    Check(fp::LevelEyePosition({0,0,0},{-std::sin(tilt),0,0,std::cos(tilt)},0.12f,0.16f,out),"downward pitch accepted");
    Check(Near(out.z,0.12f)&&Near(out.y,0.16f),"downward pitch preserves forward neck clearance");
    Check(fp::LevelEyePosition({0,0,0},{0,0,h,h},0.12f,0.16f,out),"level yaw accepted");
    Check(Near(out.x,-0.16f)&&Near(out.y,0)&&Near(out.z,0.12f),"horizontal clearance follows yaw");
    Check(!fp::LevelEyePosition({0,0,0},{0,0,0,0},0.12f,0.16f,out),"invalid eye rotation rejected");
    std::puts("Camera geometry and invalid-input checks passed.");
}
