#include "StreamingCircularBufferWriter.h"
#include <cstring>
#include <cmath>
#include <iostream>
#include <algorithm>

namespace ply2gltf {

StreamingCircularBufferWriter::StreamingCircularBufferWriter() = default;
StreamingCircularBufferWriter::~StreamingCircularBufferWriter() = default;

bool StreamingCircularBufferWriter::initialize(const StreamingMultiFrameData& metadata,
                                              bool progressive) {
    // Store metadata
    metadata_ = std::make_unique<StreamingMultiFrameData>(metadata);
    progressive_ = progressive;
    
    // Pre-allocate buffers based on circular buffer sizes
    positionBuffer_.resize(metadata.positionBuffer.totalSize);
    colorBuffer_.resize(metadata.colorBuffer.totalSize);
    orientationBuffer_.resize(metadata.orientationBuffer.totalSize);
    scaleBuffer_.resize(metadata.scaleBuffer.totalSize);
    
    if (progressive) {
        shFirstBuffer_.resize(metadata.shFirstBuffer.totalSize);
        shSecondBuffer_.resize(metadata.shSecondBuffer.totalSize);
        shThirdBuffer_.resize(metadata.shThirdBuffer.totalSize);
    }
    
    // Reset sizes
    positionBufferSize_ = 0;
    colorBufferSize_ = 0;
    orientationBufferSize_ = 0;
    scaleBufferSize_ = 0;
    shFirstBufferSize_ = 0;
    shSecondBufferSize_ = 0;
    shThirdBufferSize_ = 0;
    
    return true;
}

bool StreamingCircularBufferWriter::writeFrame(size_t frameIndex,
                                              const GaussianSplatData& frameData,
                                              const FrameMetadata& metadata) {
    if (!metadata_) {
        return false;
    }
    
    // Write each attribute for this frame
    bool success = true;
    
    success &= writeAttributeToBuffer(positionBuffer_, positionBufferSize_,
                                     metadata_->positionBuffer, frameIndex,
                                     frameData, metadata, extractPosition);
    
    success &= writeAttributeToBuffer(colorBuffer_, colorBufferSize_,
                                     metadata_->colorBuffer, frameIndex,
                                     frameData, metadata, extractColor);
    
    success &= writeAttributeToBuffer(orientationBuffer_, orientationBufferSize_,
                                     metadata_->orientationBuffer, frameIndex,
                                     frameData, metadata, extractOrientation);
    
    success &= writeAttributeToBuffer(scaleBuffer_, scaleBufferSize_,
                                     metadata_->scaleBuffer, frameIndex,
                                     frameData, metadata, extractScale);
    
    if (progressive_) {
        success &= writeAttributeToBuffer(shFirstBuffer_, shFirstBufferSize_,
                                         metadata_->shFirstBuffer, frameIndex,
                                         frameData, metadata, extractSHFirst);
        
        success &= writeAttributeToBuffer(shSecondBuffer_, shSecondBufferSize_,
                                         metadata_->shSecondBuffer, frameIndex,
                                         frameData, metadata, extractSHSecond);
        
        success &= writeAttributeToBuffer(shThirdBuffer_, shThirdBufferSize_,
                                         metadata_->shThirdBuffer, frameIndex,
                                         frameData, metadata, extractSHThird);
    }
    
    return success;
}

template<typename ExtractorFunc>
bool StreamingCircularBufferWriter::writeAttributeToBuffer(
    std::vector<uint8_t>& buffer,
    size_t& bufferSize,
    const CircularBufferInfo& bufferInfo,
    size_t frameIndex,
    const GaussianSplatData& frameData,
    const FrameMetadata& metadata,
    ExtractorFunc extractor) {
    
    // Calculate where this frame should go (sequential storage)
    size_t frameOffset = frameIndex * bufferInfo.maxFrameSize;
    
    // Ensure we don't write past the buffer
    if (frameOffset >= buffer.size()) {
        return false;
    }
    
    size_t offset = frameOffset;
    
    // Write timed accessor header if dynamic
    if (bufferInfo.isDynamic) {
        TimedAccessorInfoHeader header = createTimedHeader(
            frameIndex, 
            frameData.count(),
            metadata.timestamp,
            bufferInfo
        );
        
        std::memcpy(buffer.data() + offset, &header, sizeof(header));
        offset += sizeof(header);
    }
    
    // Write splat data
    std::vector<float> tempData(bufferInfo.componentCount);
    for (const auto& splat : frameData.splats) {
        extractor(splat, tempData.data());
        std::memcpy(buffer.data() + offset, tempData.data(), bufferInfo.stride);
        offset += bufferInfo.stride;
    }
    
    // Update total buffer size (only grows, never shrinks in circular buffer)
    size_t endOfFrame = frameOffset + bufferInfo.maxFrameSize;
    bufferSize = std::max(bufferSize, endOfFrame);
    
    return true;
}

TimedAccessorInfoHeader StreamingCircularBufferWriter::createTimedHeader(
    size_t frameIndex,
    size_t splatCount,
    float timestamp,
    const CircularBufferInfo& bufferInfo) {
    
    TimedAccessorInfoHeader header;
    header.timestampDelta = timestamp;
    
    // For dynamic accessors (immutable = false)
    header.componentType = 5126; // FLOAT
    header.type = static_cast<uint8_t>(
        bufferInfo.componentCount == 1 ? 1 :  // SCALAR
        bufferInfo.componentCount == 2 ? 2 :  // VEC2
        bufferInfo.componentCount == 3 ? 3 :  // VEC3
        bufferInfo.componentCount == 4 ? 4 :  // VEC4
        bufferInfo.componentCount == 9 ? 9 :  // MAT3
        16  // MAT4 for larger
    );
    header.normalized = 0;
    header.reserved[0] = 0;
    header.reserved[1] = 0;
    
    // Dynamic offset starts after header
    header.byteOffset = sizeof(TimedAccessorInfoHeader);
    header.count = static_cast<uint32_t>(splatCount);
    
    // Buffer view information for this frame (sequential storage)
    header.bufferViewByteOffset = static_cast<uint32_t>(frameIndex * bufferInfo.maxFrameSize);
    header.bufferViewByteLength = sizeof(TimedAccessorInfoHeader) + 
                                  static_cast<uint32_t>(splatCount * bufferInfo.stride);
    header.bufferViewByteStride = static_cast<uint32_t>(bufferInfo.stride);
    
    return header;
}

// Attribute extractors
void StreamingCircularBufferWriter::extractPosition(const GaussianSplat& splat, float* out) {
    out[0] = splat.position[0];
    out[1] = splat.position[1];
    out[2] = splat.position[2];
}

void StreamingCircularBufferWriter::extractColor(const GaussianSplat& splat, float* out) {
    // Convert DC spherical harmonics to RGB + opacity
    const float SH_C0 = 0.28209479f;
    out[0] = splat.sh_dc[0] * SH_C0 + 0.5f;  // R
    out[1] = splat.sh_dc[1] * SH_C0 + 0.5f;  // G
    out[2] = splat.sh_dc[2] * SH_C0 + 0.5f;  // B
    out[3] = 1.0f / (1.0f + std::exp(-splat.opacity));  // A (sigmoid)
}

void StreamingCircularBufferWriter::extractOrientation(const GaussianSplat& splat, float* out) {
    // Normalize quaternion
    float norm = std::sqrt(splat.rotation[0] * splat.rotation[0] +
                          splat.rotation[1] * splat.rotation[1] +
                          splat.rotation[2] * splat.rotation[2] +
                          splat.rotation[3] * splat.rotation[3]);
    if (norm > 0) {
        out[0] = splat.rotation[0] / norm;
        out[1] = splat.rotation[1] / norm;
        out[2] = splat.rotation[2] / norm;
        out[3] = splat.rotation[3] / norm;
    } else {
        out[0] = 1.0f;
        out[1] = 0.0f;
        out[2] = 0.0f;
        out[3] = 0.0f;
    }
}

void StreamingCircularBufferWriter::extractScale(const GaussianSplat& splat, float* out) {
    // Scale is already in log space
    out[0] = splat.scale[0];
    out[1] = splat.scale[1];
    out[2] = splat.scale[2];
}

void StreamingCircularBufferWriter::extractSHFirst(const GaussianSplat& splat, float* out) {
    // First 9 SH coefficients (1st order)
    std::memcpy(out, splat.sh_rest.data(), 9 * sizeof(float));
}

void StreamingCircularBufferWriter::extractSHSecond(const GaussianSplat& splat, float* out) {
    // Next 15 SH coefficients (2nd order)
    std::memcpy(out, splat.sh_rest.data() + 9, 15 * sizeof(float));
}

void StreamingCircularBufferWriter::extractSHThird(const GaussianSplat& splat, float* out) {
    // Last 21 SH coefficients (3rd order)
    std::memcpy(out, splat.sh_rest.data() + 24, 21 * sizeof(float));
}

} // namespace ply2gltf