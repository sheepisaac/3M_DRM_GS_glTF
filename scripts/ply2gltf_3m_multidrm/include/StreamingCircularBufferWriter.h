#ifndef STREAMING_CIRCULAR_BUFFER_WRITER_H
#define STREAMING_CIRCULAR_BUFFER_WRITER_H

#include "MultiFrameGaussianSplat.h"
#include "GaussianSplat.h"
#include "StreamingMultiFrameReader.h"  // For StreamingMultiFrameData and FrameMetadata
#include <vector>
#include <cstdint>
#include <functional>
#include <memory>
#include <array>

namespace ply2gltf {

// Writer that supports streaming frame-by-frame writes to circular buffers
class StreamingCircularBufferWriter {
public:
    StreamingCircularBufferWriter();
    ~StreamingCircularBufferWriter();
    
    // Initialize buffers based on metadata
    bool initialize(const StreamingMultiFrameData& metadata,
                   bool progressive = false);
    
    // Write a single frame to all attribute buffers
    bool writeFrame(size_t frameIndex,
                   const GaussianSplatData& frameData,
                   const FrameMetadata& metadata);
    
    // Get the completed buffers
    const std::vector<uint8_t>& getPositionBuffer() const { return positionBuffer_; }
    const std::vector<uint8_t>& getColorBuffer() const { return colorBuffer_; }
    const std::vector<uint8_t>& getOrientationBuffer() const { return orientationBuffer_; }
    const std::vector<uint8_t>& getScaleBuffer() const { return scaleBuffer_; }
    const std::vector<uint8_t>& getSHFirstBuffer() const { return shFirstBuffer_; }
    const std::vector<uint8_t>& getSHSecondBuffer() const { return shSecondBuffer_; }
    const std::vector<uint8_t>& getSHThirdBuffer() const { return shThirdBuffer_; }
    
    // Get buffer sizes for glTF
    size_t getPositionBufferSize() const { return positionBufferSize_; }
    size_t getColorBufferSize() const { return colorBufferSize_; }
    size_t getOrientationBufferSize() const { return orientationBufferSize_; }
    size_t getScaleBufferSize() const { return scaleBufferSize_; }
    size_t getSHFirstBufferSize() const { return shFirstBufferSize_; }
    size_t getSHSecondBufferSize() const { return shSecondBufferSize_; }
    size_t getSHThirdBufferSize() const { return shThirdBufferSize_; }
    
private:
    // Attribute buffers
    std::vector<uint8_t> positionBuffer_;
    std::vector<uint8_t> colorBuffer_;
    std::vector<uint8_t> orientationBuffer_;
    std::vector<uint8_t> scaleBuffer_;
    std::vector<uint8_t> shFirstBuffer_;
    std::vector<uint8_t> shSecondBuffer_;
    std::vector<uint8_t> shThirdBuffer_;
    
    // Actual sizes (may be less than allocated)
    size_t positionBufferSize_ = 0;
    size_t colorBufferSize_ = 0;
    size_t orientationBufferSize_ = 0;
    size_t scaleBufferSize_ = 0;
    size_t shFirstBufferSize_ = 0;
    size_t shSecondBufferSize_ = 0;
    size_t shThirdBufferSize_ = 0;
    
    // Metadata reference
    std::unique_ptr<StreamingMultiFrameData> metadata_;
    bool progressive_ = false;
    
    // Helper to write to a specific circular buffer slot
    template<typename ExtractorFunc>
    bool writeAttributeToBuffer(std::vector<uint8_t>& buffer,
                               size_t& bufferSize,
                               const CircularBufferInfo& bufferInfo,
                               size_t frameIndex,
                               const GaussianSplatData& frameData,
                               const FrameMetadata& metadata,
                               ExtractorFunc extractor);
    
    // Attribute extractors
    static void extractPosition(const GaussianSplat& splat, float* out);
    static void extractColor(const GaussianSplat& splat, float* out);
    static void extractOrientation(const GaussianSplat& splat, float* out);
    static void extractScale(const GaussianSplat& splat, float* out);
    static void extractSHFirst(const GaussianSplat& splat, float* out);
    static void extractSHSecond(const GaussianSplat& splat, float* out);
    static void extractSHThird(const GaussianSplat& splat, float* out);
    
    // Helper to create timed accessor header
    TimedAccessorInfoHeader createTimedHeader(size_t frameIndex,
                                             size_t splatCount,
                                             float timestamp,
                                             const CircularBufferInfo& bufferInfo);
};

} // namespace ply2gltf

#endif // STREAMING_CIRCULAR_BUFFER_WRITER_H