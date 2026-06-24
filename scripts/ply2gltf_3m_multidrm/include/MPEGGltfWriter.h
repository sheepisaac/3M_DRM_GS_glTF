#ifndef MPEG_GLTF_WRITER_H
#define MPEG_GLTF_WRITER_H

#include "GltfWriter.h"
#include "MultiFrameGaussianSplat.h"
#include "CircularBufferWriter.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#include "rapidjson/document.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <string>
#include <vector>

namespace ply2gltf {

class MPEGGltfWriter : public GltfWriter {
public:
    MPEGGltfWriter();
    ~MPEGGltfWriter();
    
    // Write multi-frame sequence
    bool writeSequence(const std::string& filename,
                       const MultiFrameGaussianSplatData& data,
                       const SequenceConversionOptions& options);

    // Write multi-frame sequence in chunked GLB format (BIN=frame0, SPLT for frames 1..N)
    bool writeSequenceChunked(const std::string& filename,
                              const MultiFrameGaussianSplatData& data,
                              const SequenceConversionOptions& options);
    
protected:
    // Override base class method to add MPEG extensions
    virtual std::string createJSON(const GaussianSplatData& data, 
                                   const ConversionOptions& options) override;
    
    // Create JSON for sequence
    std::string createSequenceJSON(const MultiFrameGaussianSplatData& data,
                                   const SequenceConversionOptions& options);
    
private:
    // Binary data for separate attribute buffers
    std::vector<uint8_t> positionBuffer_;
    std::vector<uint8_t> colorBuffer_;
    std::vector<uint8_t> orientationBuffer_;
    std::vector<uint8_t> scaleBuffer_;
    std::vector<uint8_t> shFirstBuffer_;
    std::vector<uint8_t> shSecondBuffer_;
    std::vector<uint8_t> shThirdBuffer_;
    std::vector<uint8_t> headerBuffer_;
    
    // Static buffers for first frame (legacy compatibility)
    std::vector<uint8_t> staticPositionBuffer_;
    std::vector<uint8_t> staticColorBuffer_;
    std::vector<uint8_t> staticOrientationBuffer_;
    std::vector<uint8_t> staticScaleBuffer_;
    std::vector<uint8_t> staticShFirstBuffer_;
    std::vector<uint8_t> staticShSecondBuffer_;
    std::vector<uint8_t> staticShThirdBuffer_;
    
    // Buffer writer
    CircularBufferWriter bufferWriter_;
    
    // Add MPEG extensions
    void addMPEGMediaExtension(rapidjson::Document& doc,
                               rapidjson::Document::AllocatorType& allocator,
                               const MultiFrameGaussianSplatData& data);
    
    void addMPEGSceneDynamicExtension(rapidjson::Document& doc,
                                      rapidjson::Document::AllocatorType& allocator);
    
    // Add circular buffers
    void addCircularBuffers(rapidjson::Document& doc,
                            rapidjson::Document::AllocatorType& allocator,
                            const MultiFrameGaussianSplatData& data,
                            bool progressive);
    
    // Add buffer views for circular buffers
    // ## FINAL FIX: ADDED 'options' PARAMETER ##
    void addCircularBufferViews(rapidjson::Document& doc,
                                rapidjson::Document::AllocatorType& allocator,
                                const MultiFrameGaussianSplatData& data,
                                const SequenceConversionOptions& options);
    
    // Add timed accessors
    void addTimedAccessors(rapidjson::Document& doc,
                           rapidjson::Document::AllocatorType& allocator,
                           const MultiFrameGaussianSplatData& data,
                           bool progressive);
    
    // Add Gaussian splat primitive with timed data
    void addGaussianSplatPrimitive(rapidjson::Document& doc,
                                   rapidjson::Document::AllocatorType& allocator,
                                   const MultiFrameGaussianSplatData& data,
                                   bool progressive);
    
    // Helper to create buffer info
    struct BufferViewIndices {
        // Static buffer views (frame 0)
        int position = -1;
        int color = -1;
        int orientation = -1;
        int scale = -1;
        int shFirst = -1;
        int shSecond = -1;
        int shThird = -1;
        
        // Circular buffer views (frames 1..N)
        int positionCircular = -1;
        int colorCircular = -1;
        int orientationCircular = -1;
        int scaleCircular = -1;
        int shFirstCircular = -1;
        int shSecondCircular = -1;
        int shThirdCircular = -1;
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
    
    // Virtual buffer mapping - maps virtual buffer index to offset in main binary chunk
    struct VirtualBufferInfo {
        size_t offset;      // Offset in the main binary chunk
        size_t size;        // Size of this virtual buffer
        int bufferIndex;    // Virtual buffer index in glTF
    };
    std::vector<VirtualBufferInfo> virtualBufferMap_;
    
    // Track circular buffer info for frames 1..N
    MultiFrameGaussianSplatData circularBufferInfo_;
    
    // Write all attribute buffers
    void writeAttributeBuffers(const MultiFrameGaussianSplatData& data,
                               const SequenceConversionOptions& options);
    
    // Write static buffers for first frame (legacy compatibility)
    void writeStaticFrameBuffers(const GaussianSplatData& frameData,
                                 const SequenceConversionOptions& options);
    
    // Combine all buffers into binary_buffer_
    void combineBuffers();

    // Combine only frame-0 static buffers into binary_buffer_
    void combineStaticBuffers();
    
    // Calculate total buffer size
    size_t calculateTotalBufferSize(const MultiFrameGaussianSplatData& data) const;
    
    // Display detailed statistics about buffer layout
    void displayStatistics(const MultiFrameGaussianSplatData& data,
                           const SequenceConversionOptions& options) const;

    // Create JSON for chunked sequence (frame0 static only in buffers)
    std::string createSequenceJSONChunked(const MultiFrameGaussianSplatData& data,
                                          const SequenceConversionOptions& options);
};

} // namespace ply2gltf

#endif // MPEG_GLTF_WRITER_H