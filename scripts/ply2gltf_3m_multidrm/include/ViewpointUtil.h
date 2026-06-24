// Simple helpers to load a viewpoint file and compute camera position from quaternion
#pragma once
#include <string>
#include <fstream>

namespace ply2gltf {

inline bool loadViewpointFile(const std::string& path, float quat[4], float center[3], float& dist) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    double qw,qx,qy,qz,cx,cy,cz,d;
    if (!(f>>qw>>qx>>qy>>qz>>cx>>cy>>cz>>d)) return false;
    // glTF quaternion order is [x,y,z,w]
    quat[0]=(float)qx; quat[1]=(float)qy; quat[2]=(float)qz; quat[3]=(float)qw;
    center[0]=(float)cx; center[1]=(float)cy; center[2]=(float)cz;
    dist=(float)d; return true;
}

inline void rotateVectorByQuat(const float q[4], const float v[3], float out[3]) {
    // q = [x,y,z,w] (unit quaternion)
    // Standard formula: t = 2 * cross(q.xyz, v); v' = v + q.w * t + cross(q.xyz, t)
    float x=q[0], y=q[1], z=q[2], w=q[3];
    float t0 = 2.0f*(y*v[2] - z*v[1]);
    float t1 = 2.0f*(z*v[0] - x*v[2]);
    float t2 = 2.0f*(x*v[1] - y*v[0]);
    // cross(q.xyz, t)
    float c0 = y*t2 - z*t1;
    float c1 = z*t0 - x*t2;
    float c2 = x*t1 - y*t0;
    out[0] = v[0] + w*t0 + c0;
    out[1] = v[1] + w*t1 + c1;
    out[2] = v[2] + w*t2 + c2;
}

} // namespace ply2gltf
