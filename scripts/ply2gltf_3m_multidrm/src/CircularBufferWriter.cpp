#include "CircularBufferWriter.h"
#include <cstring>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace ply2gltf {

CircularBufferWriter::CircularBufferWriter() 
    : buffer_(nullptr), currentOffset_(0) {
}

CircularBufferWriter::~CircularBufferWriter() = default;

size_t CircularBufferWriter::writeInterleavedFrames(const MultiFrameGaussianSplatData& data, bool progressive) {
    if (!buffer_ || data.frames.empty()) {
        std::cerr << "[CircularBufferWriter] Error: No buffer or no frames to write" << std::endl;
        return 0;
    }
    
    // Debug: Check header size
    std::cout << "[CircularBufferWriter] BufferFrameHeader size: " << sizeof(BufferFrameHeader) << " bytes (expected 14)" << std::endl;
    
    size_t startOffset = currentOffset_;
    
    // Skip frame 0 (it's written separately as static data)
    // Write frames 1..N as interleaved buffer frames
    for (size_t frameIdx = 1; frameIdx < data.frameCount; ++frameIdx) {
        const auto& frame = data.frames[frameIdx];
        uint8_t bufferFrameIndex = static_cast<uint8_t>(frameIdx - 1); // 0-based for circular buffer
        uint64_t timestamp = convertTimestamp(data.frameTimes[frameIdx]);
        
        std::cout << "[CircularBufferWriter] Writing frame " << frameIdx 
                  << " (buffer frame " << (int)bufferFrameIndex << ") with " 
                  << frame.count() << " splats" << std::endl;
        
        // Prepare temporary buffers for each attribute
        std::vector<float> tempData;
        
        // 1. Position buffer frame
        extractPositionData(frame, tempData);
        writeBufferFrame(bufferFrameIndex, timestamp, tempData.data(), 
                        tempData.size() * sizeof(float));
        
        // 2. Color buffer frame
        extractColorData(frame, tempData);
        writeBufferFrame(bufferFrameIndex, timestamp, tempData.data(), 
                        tempData.size() * sizeof(float));
        
        // 3. Orientation buffer frame
        extractOrientationData(frame, tempData);
        writeBufferFrame(bufferFrameIndex, timestamp, tempData.data(), 
                        tempData.size() * sizeof(float));
        
        // 4. Scale buffer frame
        extractScaleData(frame, tempData);
        writeBufferFrame(bufferFrameIndex, timestamp, tempData.data(), 
                        tempData.size() * sizeof(float));
        
        // 5. SH First buffer frame (9 coefficients for progressive, 15 for R/G/B)
        extractSHFirstData(frame, tempData, progressive);
        writeBufferFrame(bufferFrameIndex, timestamp, tempData.data(), 
                        tempData.size() * sizeof(float));
        
        // 6. SH Second buffer frame (15 coefficients for both formats)
        extractSHSecondData(frame, tempData, progressive);
        writeBufferFrame(bufferFrameIndex, timestamp, tempData.data(), 
                        tempData.size() * sizeof(float));
        
        // 7. SH Third buffer frame (21 coefficients for progressive, 15 for R/G/B)
        extractSHThirdData(frame, tempData, progressive);
        writeBufferFrame(bufferFrameIndex, timestamp, tempData.data(), 
                        tempData.size() * sizeof(float));
    }
    
    size_t totalBytesWritten = currentOffset_ - startOffset;
    std::cout << "[CircularBufferWriter] Wrote " << totalBytesWritten << " bytes for " 
              << (data.frameCount - 1) << " circular frames" << std::endl;
    
    return totalBytesWritten;
}

size_t CircularBufferWriter::writeBufferFrame(uint8_t frameIndex, 
                                              uint64_t timestamp,
                                              const void* data, 
                                              size_t dataSize) {
    // Create buffer frame header
    BufferFrameHeader header(frameIndex, timestamp, static_cast<uint32_t>(dataSize));
    
    // Debug: Show what we're writing
    static int bufferFrameCount = 0;
    if (bufferFrameCount < 14) {  // Show first 14 buffer frames (first 2 GS frames)
        printf("[CircularBufferWriter] Writing buffer frame %d: index=%u, timestamp=%llu, length=%u, offset=%zu\n",
               bufferFrameCount, header.index, (unsigned long long)header.timestamp, 
               header.length, currentOffset_);
        bufferFrameCount++;
    }
    
    // Write header
    size_t bytesWritten = writeData(&header, sizeof(BufferFrameHeader));
    
    // Write data
    bytesWritten += writeData(data, dataSize);
    
    return bytesWritten;
}

size_t CircularBufferWriter::writeData(const void* data, size_t size) {
    if (!buffer_ || size == 0) {
        return 0;
    }
    
    // Ensure buffer has enough capacity
    if (buffer_->size() < currentOffset_ + size) {
        buffer_->resize(currentOffset_ + size);
    }
    
    std::memcpy(buffer_->data() + currentOffset_, data, size);
    currentOffset_ += size;
    return size;
}

void CircularBufferWriter::extractPositionData(const GaussianSplatData& frame, std::vector<float>& out) {
    out.clear();
    out.reserve(frame.count() * 3);
    for (const auto& splat : frame.splats) {
        out.push_back(splat.position[0]);
        out.push_back(splat.position[1]);
        out.push_back(splat.position[2]);
    }
}

void CircularBufferWriter::extractColorData(const GaussianSplatData& frame, std::vector<float>& out) {
    out.clear();
    out.reserve(frame.count() * 4);
    const float SH_C0 = 0.28209479f;
    for (const auto& splat : frame.splats) {
        out.push_back(splat.sh_dc[0] * SH_C0 + 0.5f);  // R
        out.push_back(splat.sh_dc[1] * SH_C0 + 0.5f);  // G
        out.push_back(splat.sh_dc[2] * SH_C0 + 0.5f);  // B
        out.push_back(1.0f / (1.0f + std::exp(-splat.opacity)));  // A (sigmoid)
    }
}

void CircularBufferWriter::extractOrientationData(const GaussianSplatData& frame, std::vector<float>& out) {
    out.clear();
    out.reserve(frame.count() * 4);
    for (const auto& splat : frame.splats) {
        // Normalize quaternion
        float norm = std::sqrt(splat.rotation[0] * splat.rotation[0] +
                              splat.rotation[1] * splat.rotation[1] +
                              splat.rotation[2] * splat.rotation[2] +
                              splat.rotation[3] * splat.rotation[3]);
        if (norm > 0) {
            out.push_back(splat.rotation[0] / norm);
            out.push_back(splat.rotation[1] / norm);
            out.push_back(splat.rotation[2] / norm);
            out.push_back(splat.rotation[3] / norm);
        } else {
            out.push_back(1.0f);
            out.push_back(0.0f);
            out.push_back(0.0f);
            out.push_back(0.0f);
        }
    }
}

void CircularBufferWriter::extractScaleData(const GaussianSplatData& frame, std::vector<float>& out) {
    out.clear();
    out.reserve(frame.count() * 3);
    for (const auto& splat : frame.splats) {
        out.push_back(splat.scale[0]);
        out.push_back(splat.scale[1]);
        out.push_back(splat.scale[2]);
    }
}

void CircularBufferWriter::extractSHFirstData(const GaussianSplatData& frame, std::vector<float>& out, bool progressive) {
    out.clear();
    if (progressive) {
        // Progressive format: first 9 coefficients (3x3 RGB interleaved)
        out.reserve(frame.count() * 9);
        for (const auto& splat : frame.splats) {
            for (int i = 0; i < 9; ++i) {
                out.push_back(splat.sh_rest[i]);
            }
        }
    } else {
        // R/G/B format: R channel (15 coefficients)
        out.reserve(frame.count() * 15);
        for (const auto& splat : frame.splats) {
            for (int i = 0; i < 15; ++i) {
                out.push_back(splat.sh_rest[i * 3 + 0]); // Extract R component
            }
        }
    }
}

void CircularBufferWriter::extractSHSecondData(const GaussianSplatData& frame, std::vector<float>& out, bool progressive) {
    out.clear();
    out.reserve(frame.count() * 15);
    if (progressive) {
        // Progressive format: next 15 coefficients (5x3 RGB interleaved)
        for (const auto& splat : frame.splats) {
            for (int i = 9; i < 24; ++i) {  // indices 9-23 (15 coefficients)
                out.push_back(splat.sh_rest[i]);
            }
        }
    } else {
        // R/G/B format: G channel (15 coefficients)
        for (const auto& splat : frame.splats) {
            for (int i = 0; i < 15; ++i) {
                out.push_back(splat.sh_rest[i * 3 + 1]); // Extract G component
            }
        }
    }
}

void CircularBufferWriter::extractSHThirdData(const GaussianSplatData& frame, std::vector<float>& out, bool progressive) {
    out.clear();
    if (progressive) {
        // Progressive format: last 21 coefficients (7x3 RGB interleaved)
        out.reserve(frame.count() * 21);
        for (const auto& splat : frame.splats) {
            for (int i = 24; i < 45; ++i) {  // indices 24-44 (21 coefficients)
                out.push_back(splat.sh_rest[i]);
            }
        }
    } else {
        // R/G/B format: B channel (15 coefficients)
        out.reserve(frame.count() * 15);
        for (const auto& splat : frame.splats) {
            for (int i = 0; i < 15; ++i) {
                out.push_back(splat.sh_rest[i * 3 + 2]); // Extract B component
            }
        }
    }
}

uint64_t CircularBufferWriter::convertTimestamp(float seconds) {
    // Convert float seconds to 64-bit timestamp
    // Upper 32 bits: integer seconds
    // Lower 32 bits: fractional seconds
    uint32_t sec = static_cast<uint32_t>(seconds);
    uint32_t frac = static_cast<uint32_t>((seconds - sec) * 4294967296.0); // 2^32
    return (static_cast<uint64_t>(sec) << 32) | frac;
}

} // namespace ply2gltf