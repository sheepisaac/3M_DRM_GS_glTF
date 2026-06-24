#ifndef STREAMING_MPEG_GLTF_WRITER_H
#define STREAMING_MPEG_GLTF_WRITER_H

#include "GltfWriter.h"
#include "StreamingMultiFrameReader.h"
#include "StreamingCircularBufferWriter.h"
#include "rapidjson/document.h"
#include <string>
#include <vector>
#include <memory>

namespace ply2gltf {

// MPEG glTF writer that processes frames in streaming fashion
class StreamingMPEGGltfWriter : public GltfWriter {
public:
    StreamingMPEGGltfWriter();
    ~StreamingMPEGGltfWriter();
    
    // Write multi-frame sequence using streaming approach
    bool writeSequenceStreaming(const std::vector<std::string>& filenames,
                               const std::string& outputFile,
                               const SequenceConversionOptions& options);
    
protected:
    // Override base class method
    virtual std::string createJSON(const GaussianSplatData& data, 
                                  const ConversionOptions& options) override;
    
private:
    // Streaming components
    std::unique_ptr<StreamingMultiFrameReader> reader_;
    std::unique_ptr<StreamingCircularBufferWriter> bufferWriter_;
    std::unique_ptr<StreamingMultiFrameData> metadata_;
    
    // Buffer view and accessor indices
    struct BufferViewIndices {
        int position = -1;
        int color = -1;
        int orientation = -1;
        int scale = -1;
        int shFirst = -1;
        int shSecond = -1;
        int shThird = -1;
    };
    
    struct AccessorIndices {
        int position = -1;
        int color = -1;
        int orientation = -1;
        int scale = -1;
        int shFirst = -1;
        int shSecond = -1;
        int shThird = -1;
    };
    
    BufferViewIndices bufferViewIndices_;
    AccessorIndices accessorIndices_;
    
    // Current indices for glTF arrays
    int currentBufferIndex_ = 0;
    int currentBufferViewIndex_ = 0;
    int currentAccessorIndex_ = 0;
    
    // Create JSON for streaming sequence
    std::string createStreamingJSON(const StreamingMultiFrameData& metadata,
                                   const SequenceConversionOptions& options);
    
    // Add MPEG extensions
    void addMPEGMediaExtension(rapidjson::Document& doc,
                              rapidjson::Document::AllocatorType& allocator,
                              const StreamingMultiFrameData& metadata);
    
    void addMPEGSceneDynamicExtension(rapidjson::Document& doc,
                                     rapidjson::Document::AllocatorType& allocator);
    
    // Add circular buffers
    void addCircularBuffers(rapidjson::Document& doc,
                           rapidjson::Document::AllocatorType& allocator,
                           const StreamingMultiFrameData& metadata,
                           bool progressive);
    
    // Add buffer views for circular buffers
    void addCircularBufferViews(rapidjson::Document& doc,
                               rapidjson::Document::AllocatorType& allocator,
                               const StreamingMultiFrameData& metadata);
    
    // Add timed accessors
    void addTimedAccessors(rapidjson::Document& doc,
                          rapidjson::Document::AllocatorType& allocator,
                          const StreamingMultiFrameData& metadata);
    
    // Add Gaussian splat primitive with timed data
    void addGaussianSplatPrimitive(rapidjson::Document& doc,
                                  rapidjson::Document::AllocatorType& allocator,
                                  const StreamingMultiFrameData& metadata,
                                  bool progressive);
    
    // Combine buffers from streaming writer
    void combineBuffers();
    
    // Display statistics
    void displayStatistics(const StreamingMultiFrameData& metadata,
                          const SequenceConversionOptions& options) const;
};

} // namespace ply2gltf

#endif // STREAMING_MPEG_GLTF_WRITER_H