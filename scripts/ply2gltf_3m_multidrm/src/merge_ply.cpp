#include "PlyReader.h"
#include "GaussianSplat.h"
#include <iostream>
#include <fstream>
#include <cmath>
#include <array>

namespace ply2gltf {

// Simple quaternion structure
struct Quaternion {
    float x, y, z, w;
    Quaternion() : x(0), y(0), z(0), w(1) {}
    Quaternion(float w, float x, float y, float z) : x(x), y(y), z(z), w(w) {}
    
    Quaternion normalize() const {
        float len = std::sqrt(x*x + y*y + z*z + w*w);
        if (len < 1e-8f) return Quaternion(1, 0, 0, 0);
        return Quaternion(w/len, x/len, y/len, z/len);
    }
};

// Quaternion multiplication
static Quaternion operator*(const Quaternion& a, const Quaternion& b) {
    return Quaternion(
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z,
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w
    );
}

// Helper function to convert Euler angles (degrees) to quaternion
static Quaternion eulerToQuaternion(float rx, float ry, float rz) {
    const float PI = 3.14159265358979323846f;
    float x = rx * PI / 180.0f;
    float y = ry * PI / 180.0f;
    float z = rz * PI / 180.0f;
    
    // ZYX order (common in glTF)
    float cx = std::cos(x * 0.5f);
    float sx = std::sin(x * 0.5f);
    float cy = std::cos(y * 0.5f);
    float sy = std::sin(y * 0.5f);
    float cz = std::cos(z * 0.5f);
    float sz = std::sin(z * 0.5f);
    
    float qx = sx * cy * cz - cx * sy * sz;
    float qy = cx * sy * cz + sx * cy * sz;
    float qz = cx * cy * sz - sx * sy * cz;
    float qw = cx * cy * cz + sx * sy * sz;
    
    return Quaternion(qw, qx, qy, qz);
}

// Apply transformation matrix to a point
static void transformPoint(std::array<float, 3>& pos,
                           const std::array<float, 3>& translation,
                           const Quaternion& rotation,
                           const std::array<float, 3>& scale) {
    // Convert quaternion to rotation matrix
    float qx = rotation.x, qy = rotation.y, qz = rotation.z, qw = rotation.w;
    float xx = qx * qx, yy = qy * qy, zz = qz * qz;
    float xy = qx * qy, xz = qx * qz, yz = qy * qz;
    float wx = qw * qx, wy = qw * qy, wz = qw * qz;
    
    // Rotation matrix (column-major)
    float m[16] = {
        1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz), 2.0f * (xz - wy), 0.0f,
        2.0f * (xy - wz), 1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx), 0.0f,
        2.0f * (xz + wy), 2.0f * (yz - wx), 1.0f - 2.0f * (xx + yy), 0.0f,
        translation[0], translation[1], translation[2], 1.0f
    };
    
    // Apply scale
    m[0] *= scale[0]; m[4] *= scale[1]; m[8] *= scale[2];
    m[1] *= scale[0]; m[5] *= scale[1]; m[9] *= scale[2];
    m[2] *= scale[0]; m[6] *= scale[1]; m[10] *= scale[2];
    
    // Transform point
    float x = pos[0], y = pos[1], z = pos[2];
    pos[0] = m[0] * x + m[4] * y + m[8] * z + m[12];
    pos[1] = m[1] * x + m[5] * y + m[9] * z + m[13];
    pos[2] = m[2] * x + m[6] * y + m[10] * z + m[14];
}

// Apply transformation to a Gaussian splat
static void applyTransform(GaussianSplat& splat, 
                          const std::array<float, 3>& translation,
                          const std::array<float, 3>& rotation,
                          const std::array<float, 3>& scale) {
    // Convert rotation to quaternion
    Quaternion rotQuat = eulerToQuaternion(rotation[0], rotation[1], rotation[2]);
    
    // Transform position
    transformPoint(splat.position, translation, rotQuat, scale);
    
    // Transform rotation quaternion: node rotation * splat rotation
    Quaternion splatRot(splat.rotation[3], splat.rotation[0], splat.rotation[1], splat.rotation[2]);
    splatRot = (rotQuat * splatRot).normalize();
    splat.rotation[0] = splatRot.x;
    splat.rotation[1] = splatRot.y;
    splat.rotation[2] = splatRot.z;
    splat.rotation[3] = splatRot.w;
    
    // Transform scale
    splat.scale[0] *= scale[0];
    splat.scale[1] *= scale[1];
    splat.scale[2] *= scale[2];
}

// Write PLY file
static bool writePly(const std::string& filename, const GaussianSplatData& data) {
    std::ofstream out(filename, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot open output file: " << filename << std::endl;
        return false;
    }
    
    // Write PLY header
    out << "ply\n";
    out << "format binary_little_endian 1.0\n";
    out << "element vertex " << data.splats.size() << "\n";
    out << "property float x\n";
    out << "property float y\n";
    out << "property float z\n";
    out << "property float nx\n";
    out << "property float ny\n";
    out << "property float nz\n";
    out << "property float f_dc_0\n";
    out << "property float f_dc_1\n";
    out << "property float f_dc_2\n";
    for (int i = 0; i < 45; ++i) {
        out << "property float f_rest_" << i << "\n";
    }
    out << "property float opacity\n";
    out << "property float scale_0\n";
    out << "property float scale_1\n";
    out << "property float scale_2\n";
    out << "property float rot_0\n";
    out << "property float rot_1\n";
    out << "property float rot_2\n";
    out << "property float rot_3\n";
    out << "end_header\n";
    
    // Write binary data
    for (const auto& splat : data.splats) {
        // Position
        out.write(reinterpret_cast<const char*>(splat.position.data()), sizeof(float) * 3);
        // Normal
        out.write(reinterpret_cast<const char*>(splat.normal.data()), sizeof(float) * 3);
        // SH DC
        out.write(reinterpret_cast<const char*>(splat.sh_dc.data()), sizeof(float) * 3);
        // SH rest
        out.write(reinterpret_cast<const char*>(splat.sh_rest.data()), sizeof(float) * 45);
        // Opacity
        out.write(reinterpret_cast<const char*>(&splat.opacity), sizeof(float));
        // Scale
        out.write(reinterpret_cast<const char*>(splat.scale.data()), sizeof(float) * 3);
        // Rotation
        out.write(reinterpret_cast<const char*>(splat.rotation.data()), sizeof(float) * 4);
    }
    
    out.close();
    return true;
}

} // namespace ply2gltf

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <output.ply> [base_path]" << std::endl;
        std::cout << "This program merges multiple PLY files with transformations." << std::endl;
        std::cout << "base_path: Base directory for PLY files (default: ../data/gs_static)" << std::endl;
        return 1;
    }
    
    std::string outputFile = argv[1];
    std::string basePath = (argc >= 3) ? argv[2] : "../data/gs_static";
    
    // Define objects and their transformations
    struct ObjectInfo {
        std::string name;
        std::string file;
        std::array<float, 3> translation;
        std::array<float, 3> rotation;
        std::array<float, 3> scale;
    };
    
    std::vector<ObjectInfo> objects = {
        {"Library", basePath + "/library/library.ply", {0, 0, 0}, {0, 0, 180}, {1, 1, 1}},
        {"Cricket", basePath + "/cricket_player/cricket_player.ply", {0.6, -0.07, -0.8}, {0, 0, 0}, {0.08, 0.08, 0.08}},
        {"Tango duo", basePath + "/tango_duo/tango_duo.ply", {0.52, 0.025, -0.92}, {0, 0, 0}, {0.043, 0.043, 0.043}},
        {"Tennis", basePath + "/tennis_player/tennis_player.ply", {0.5, -0.07, -0.8}, {0, 0, 0}, {0.08, 0.08, 0.08}},
        {"bugatti", basePath + "/bugatti/bugatti.ply", {-0.15, -0.06, -1.05}, {0, 62.56, 180}, {0.08, 0.08, 0.08}},
        {"ferrari", basePath + "/ferrari/ferrari.ply", {0.61, -0.07, -1.33}, {180, -13.85, 0}, {0.055, 0.055, 0.055}}
    };
    
    ply2gltf::GaussianSplatData mergedData;
    ply2gltf::PlyReader reader;
    
    std::cout << "Merging PLY files..." << std::endl;
    
    for (size_t i = 0; i < objects.size(); ++i) {
        const auto& obj = objects[i];
        std::cout << "[" << (i+1) << "/" << objects.size() << "] Processing " << obj.name 
                  << " from " << obj.file << std::endl;
        
        ply2gltf::GaussianSplatData data;
        if (!reader.read(obj.file, data, true)) {
            std::cerr << "Error reading " << obj.file << ": " << reader.getError() << std::endl;
            continue;
        }
        
        std::cout << "  Loaded " << data.splats.size() << " splats" << std::endl;
        
        // Apply transformation
        for (auto& splat : data.splats) {
            ply2gltf::applyTransform(splat, obj.translation, obj.rotation, obj.scale);
        }
        
        std::cout << "  Applied transformation: T=(" << obj.translation[0] << "," << obj.translation[1] << "," << obj.translation[2] 
                  << ") R=(" << obj.rotation[0] << "," << obj.rotation[1] << "," << obj.rotation[2] 
                  << ") S=(" << obj.scale[0] << "," << obj.scale[1] << "," << obj.scale[2] << ")" << std::endl;
        
        // Merge into combined data
        size_t beforeSize = mergedData.splats.size();
        mergedData.splats.insert(mergedData.splats.end(), data.splats.begin(), data.splats.end());
        size_t afterSize = mergedData.splats.size();
        
        std::cout << "  Merged: " << (afterSize - beforeSize) << " splats (total: " << afterSize << ")" << std::endl;
    }
    
    // Compute bounds
    mergedData.computeBounds();
    
    std::cout << "\nTotal splats: " << mergedData.splats.size() << std::endl;
    std::cout << "Bounds: [" << mergedData.min_bounds[0] << "," << mergedData.min_bounds[1] << "," << mergedData.min_bounds[2] 
              << "] to [" << mergedData.max_bounds[0] << "," << mergedData.max_bounds[1] << "," << mergedData.max_bounds[2] << "]" << std::endl;
    
    // Write merged PLY file
    std::cout << "\nWriting merged PLY file: " << outputFile << std::endl;
    if (!ply2gltf::writePly(outputFile, mergedData)) {
        std::cerr << "Error writing PLY file" << std::endl;
        return 1;
    }
    
    std::cout << "Successfully created " << outputFile << std::endl;
    return 0;
}

