#ifndef CIRCULAR_BUFFER_WRITER_H
#define CIRCULAR_BUFFER_WRITER_H

#include "MultiFrameGaussianSplat.h"
#include <vector>
#include <functional>

namespace ply2gltf {

// Writer for circular buffers containing multi-frame gaussian splat data
// Implements MPEG-I Scene Description spec with interleaved buffer frames
class CircularBufferWriter {
public:
    CircularBufferWriter();
    ~CircularBufferWriter();
    
    // Set the output buffer to write to
    void setBuffer(std::vector<uint8_t>* buffer) { 
        buffer_ = buffer; 
        currentOffset_ = 0;
    }
    
    // Get current offset
    size_t getCurrentOffset() const { return currentOffset_; }
    
    // Write all frames in interleaved format
    // For each frame 1..N, writes 7 buffer frames (one for each attribute)
    // Returns total bytes written
    size_t writeInterleavedFrames(const MultiFrameGaussianSplatData& data, bool progressive = false);
    
private:
    // Output buffer pointer
    std::vector<uint8_t>* buffer_;
    size_t currentOffset_;
    
    // Write a single buffer frame (header + data)
    size_t writeBufferFrame(uint8_t frameIndex, 
                           uint64_t timestamp,
                           const void* data, 
                           size_t dataSize);
    
    // Write raw data to buffer
    size_t writeData(const void* data, size_t size);
    
    // Extract attribute data from a frame
    void extractPositionData(const GaussianSplatData& frame, std::vector<float>& out);
    void extractColorData(const GaussianSplatData& frame, std::vector<float>& out);
    void extractOrientationData(const GaussianSplatData& frame, std::vector<float>& out);
    void extractScaleData(const GaussianSplatData& frame, std::vector<float>& out);
    void extractSHFirstData(const GaussianSplatData& frame, std::vector<float>& out, bool progressive = false);
    void extractSHSecondData(const GaussianSplatData& frame, std::vector<float>& out, bool progressive = false);
    void extractSHThirdData(const GaussianSplatData& frame, std::vector<float>& out, bool progressive = false);
    
    // Convert timestamp from float seconds to 64-bit format
    uint64_t convertTimestamp(float seconds);
};

} // namespace ply2gltf

#endif // CIRCULAR_BUFFER_WRITER_H