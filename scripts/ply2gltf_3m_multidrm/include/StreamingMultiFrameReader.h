#ifndef STREAMING_MULTI_FRAME_READER_H
#define STREAMING_MULTI_FRAME_READER_H

#include "PlyReader.h"
#include "MultiFrameGaussianSplat.h"
#include <vector>
#include <string>
#include <functional>
#include <memory>

namespace ply2gltf {

// Metadata for a single frame without the actual splat data
struct FrameMetadata {
    std::string filename;
    size_t frameIndex;
    size_t splatCount;
    float timestamp;
    std::array<float, 3> minBounds;
    std::array<float, 3> maxBounds;
};

// Streaming reader that processes frames one at a time
class StreamingMultiFrameReader {
public:
    StreamingMultiFrameReader();
    ~StreamingMultiFrameReader();
    
    // First pass: Read metadata only
    bool readMetadata(const std::vector<std::string>& filenames,
                     std::vector<FrameMetadata>& metadata,
                     bool verbose = false);
    
    // Read a single frame
    bool readFrame(const std::string& filename,
                  GaussianSplatData& frameData,
                  bool verbose = false);
    
    // Process frames with a callback (streaming)
    using FrameProcessor = std::function<bool(size_t frameIndex, 
                                            const GaussianSplatData& frameData,
                                            const FrameMetadata& metadata)>;
    
    bool processFrames(const std::vector<std::string>& filenames,
                      FrameProcessor processor,
                      bool verbose = false);
    
    // Validate frame consistency based on metadata
    bool validateMetadataConsistency(const std::vector<FrameMetadata>& metadata);
    
    const std::string& getError() const { return error_; }
    
private:
    std::string error_;
    std::unique_ptr<PlyReader> plyReader_;
    
    // Read only header information from PLY
    bool readPlyHeader(const std::string& filename, 
                      FrameMetadata& metadata,
                      bool verbose);
};

// Streaming data structure that doesn't hold all frames in memory
struct StreamingMultiFrameData {
    // Metadata for all frames (lightweight)
    std::vector<FrameMetadata> frameMetadata;
    
    // Temporal metadata
    float frameRate = 30.0f;
    float duration = 0.0f;
    size_t frameCount = 0;
    size_t maxSplatCount = 0;
    
    // Attribute-specific circular buffer info (same as before)
    CircularBufferInfo positionBuffer;
    CircularBufferInfo colorBuffer;
    CircularBufferInfo orientationBuffer;
    CircularBufferInfo scaleBuffer;
    CircularBufferInfo shFirstBuffer;
    CircularBufferInfo shSecondBuffer;
    CircularBufferInfo shThirdBuffer;
    
    // Calculate buffer sizes based on metadata
    void calculateBufferSizes() {
        if (frameMetadata.empty()) return;
        
        // Find max splat count from metadata
        maxSplatCount = 0;
        for (const auto& meta : frameMetadata) {
            maxSplatCount = std::max(maxSplatCount, meta.splatCount);
        }
        
        // Calculate duration and frame count
        frameCount = frameMetadata.size();
        duration = frameCount / frameRate;
        
        // Calculate buffer sizes for each attribute (storing ALL frames)
        positionBuffer.calculate(maxSplatCount, 3, sizeof(float), frameCount);
        colorBuffer.calculate(maxSplatCount, 4, sizeof(float), frameCount);
        orientationBuffer.calculate(maxSplatCount, 4, sizeof(float), frameCount);
        scaleBuffer.calculate(maxSplatCount, 3, sizeof(float), frameCount);
        shFirstBuffer.calculate(maxSplatCount, 9, sizeof(float), frameCount);
        shSecondBuffer.calculate(maxSplatCount, 15, sizeof(float), frameCount);
        shThirdBuffer.calculate(maxSplatCount, 21, sizeof(float), frameCount);
    }
    
    // Note: Buffer counts are now automatically set to frameCount
    // All frames must be stored sequentially
};

} // namespace ply2gltf

#endif // STREAMING_MULTI_FRAME_READER_H