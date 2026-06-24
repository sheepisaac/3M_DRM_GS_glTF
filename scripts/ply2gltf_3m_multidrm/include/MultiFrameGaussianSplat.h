#ifndef MULTI_FRAME_GAUSSIAN_SPLAT_H
#define MULTI_FRAME_GAUSSIAN_SPLAT_H

#include "GaussianSplat.h"
#include <vector>
#include <string>
#include <cstdint>

namespace ply2gltf {

// Buffer frame header as per MPEG-I Scene Description spec Table 25
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
struct BufferFrameHeader {
    uint8_t index;              // Frame index (0 to count-1)
    uint64_t timestamp;         // 64-bit timestamp (32 bits seconds, 32 bits fraction)
    uint32_t length;            // Length of data for this buffer frame
    uint8_t extra_frame_info_flags;  // Additional flags (0 for Gaussian Splats)
    // If flags & 0x01, width and height would follow (not used for GS)
    
    BufferFrameHeader() : index(0), timestamp(0), length(0), extra_frame_info_flags(0) {}
    
    BufferFrameHeader(uint8_t idx, uint64_t ts, uint32_t len) 
        : index(idx), timestamp(ts), length(len), extra_frame_info_flags(0) {}
}
#ifdef _MSC_VER
#pragma pack(pop)
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((packed))
#endif
;  // Ensure no padding

// Legacy structure - not used in new implementation
struct TimedAccessorInfoHeader {
    float timestampDelta;       // Time difference from start
    uint32_t componentType;     // Dynamic component type (if immutable=false)
    uint8_t type;              // Dynamic type (if immutable=false)
    uint8_t normalized;        // Dynamic normalized flag (if immutable=false)
    uint8_t reserved[2];       // Reserved for alignment
    uint32_t byteOffset;       // Dynamic offset in buffer
    uint32_t count;            // Dynamic element count (variable splat count!)
    // Min/max arrays would follow based on component count
    uint32_t bufferViewByteOffset;  // Dynamic buffer view offset
    uint32_t bufferViewByteLength;  // Dynamic buffer view length
    uint32_t bufferViewByteStride;  // Dynamic buffer view stride
};

// Circular buffer information for each attribute
struct CircularBufferInfo {
    size_t actualSize;      // Actual size of data written (no padding)
    size_t frameCount;      // Number of frames stored
    size_t componentCount;  // Number of components per element
    size_t stride;          // Stride between elements
    bool isDynamic;         // Whether this buffer uses dynamic headers
    
    CircularBufferInfo() : actualSize(0), frameCount(0), 
                           componentCount(0), stride(0), isDynamic(true) {}
    
    void setup(size_t components, size_t componentSize, size_t numFrames) {
        componentCount = components;
        stride = components * componentSize;
        frameCount = numFrames;
        isDynamic = true;
        actualSize = 0;  // Will be set after actual write
    }
};

// Multi-frame Gaussian splat data container
struct MultiFrameGaussianSplatData {
    // Frame-specific data
    std::vector<GaussianSplatData> frames;
    
    // Temporal metadata
    float frameRate = 30.0f;
    float duration = 0.0f;
    size_t frameCount = 0;
    size_t maxSplatCount = 0;  // Maximum splats across all frames
    
    // Per-frame splat counts for dynamic sizing
    std::vector<size_t> frameSplatCounts;
    
    // Attribute-specific circular buffer info
    CircularBufferInfo positionBuffer;
    CircularBufferInfo colorBuffer;
    CircularBufferInfo orientationBuffer;
    CircularBufferInfo scaleBuffer;
    CircularBufferInfo shFirstBuffer;   // 9 coefficients (1st order)
    CircularBufferInfo shSecondBuffer;  // 15 coefficients (2nd order)
    CircularBufferInfo shThirdBuffer;   // 21 coefficients (3rd order)
    
    // Frame timing information
    std::vector<float> frameTimes;  // Presentation time for each frame
    
    // Setup buffer information based on loaded frames
    void setupBuffers() {
        if (frames.empty()) return;
        
        // Collect per-frame splat counts and find max
        frameSplatCounts.clear();
        frameSplatCounts.reserve(frames.size());
        maxSplatCount = 0;
        for (const auto& frame : frames) {
            size_t count = frame.count();
            frameSplatCounts.push_back(count);
            maxSplatCount = std::max(maxSplatCount, count);
        }
        
        // Calculate duration and frame count
        frameCount = frames.size();
        duration = frameCount / frameRate;
        
        // Setup buffer info for each attribute
        positionBuffer.setup(3, sizeof(float), frameCount);    // Vec3
        colorBuffer.setup(4, sizeof(float), frameCount);       // Vec4 (RGB + opacity)
        orientationBuffer.setup(4, sizeof(float), frameCount); // Vec4 (quaternion)
        scaleBuffer.setup(3, sizeof(float), frameCount);       // Vec3
        shFirstBuffer.setup(9, sizeof(float), frameCount);     // 9 SH coeffs
        shSecondBuffer.setup(15, sizeof(float), frameCount);   // 15 SH coeffs
        shThirdBuffer.setup(21, sizeof(float), frameCount);    // 21 SH coeffs
        
        // Calculate frame times
        frameTimes.clear();
        frameTimes.reserve(frameCount);
        for (size_t i = 0; i < frameCount; ++i) {
            frameTimes.push_back(static_cast<float>(i) / frameRate);
        }
    }
    
    // Update actual sizes after buffers are written
    void updateActualSizes(size_t posSize, size_t colSize, size_t oriSize, size_t scaleSize,
                          size_t sh1Size, size_t sh2Size, size_t sh3Size) {
        positionBuffer.actualSize = posSize;
        colorBuffer.actualSize = colSize;
        orientationBuffer.actualSize = oriSize;
        scaleBuffer.actualSize = scaleSize;
        shFirstBuffer.actualSize = sh1Size;
        shSecondBuffer.actualSize = sh2Size;
        shThirdBuffer.actualSize = sh3Size;
    }
    
    // For compatibility - old function name
    void calculateBufferSizes() {
        setupBuffers();
    }
};

// Legacy frame header structure (not used in new implementation)
struct FrameHeader {
    uint32_t frameIndex;
    uint32_t splatCount;
    float timestamp;
    uint32_t reserved;
};

} // namespace ply2gltf

#endif // MULTI_FRAME_GAUSSIAN_SPLAT_H