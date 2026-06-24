#ifndef GLTF_WRITER_H
#define GLTF_WRITER_H

#include "GaussianSplat.h"
#include <string>
#include <vector>
#include <cstdint>

namespace ply2gltf {

// ## MULTIDRM: ENCRYPTION FUNCTION DECLARATIONS ##
// Simple XOR encryption (for testing/compatibility)
void xorEncrypt(std::vector<uint8_t>& buffer, const std::string& key, bool verbose);

// MultiDRM encryption interface
// Encrypts buffer using the specified DRM configuration
// For commercial DRM systems, this would integrate with their SDKs
void encryptWithDRM(std::vector<uint8_t>& buffer, const DRMConfig& config, bool verbose);


class GltfWriter {
public:
    GltfWriter();
    virtual ~GltfWriter() = default; // Make destructor virtual for base class
    
    // Write Gaussian splat data to glTF/GLB file
    bool write(const std::string& filename, const GaussianSplatData& data, const ConversionOptions& options);
    
    // ## MULTI-OBJECT: Write multiple objects to glTF/GLB file ##
    bool writeMultiObject(const std::string& filename, 
                         const std::vector<GaussianSplatData>& objectsData,
                         const std::vector<ObjectTransform>& transforms,
                         const ConversionOptions& options);

    bool writeLayeredObjects(const std::string& filename,
                             const std::vector<std::vector<GaussianSplatData>>& layersData,
                             const std::vector<LayeredObjectInfo>& objects,
                             const ConversionOptions& options);
    
    // Get last error message
    const std::string& getError() const { return error_; }
    
protected:
    std::string error_;
    
    // GLB file format structures
    struct GlbHeader {
        uint32_t magic = 0x46546C67;  // 'glTF'
        uint32_t version = 2;
        uint32_t length = 0;
    };
    
    struct ChunkHeader {
        uint32_t length = 0;
        uint32_t type = 0;
    };
    
    static constexpr uint32_t CHUNK_TYPE_JSON = 0x4E4F534A;  // 'JSON'
    static constexpr uint32_t CHUNK_TYPE_BIN = 0x004E4942;   // 'BIN\0'
    
    // Buffer management
    std::vector<uint8_t> binary_buffer_;
    size_t current_offset_ = 0;
    
    // Helper methods
    size_t addToBuffer(const void* data, size_t size);
    size_t padBuffer(size_t alignment);
    virtual std::string createJSON(const GaussianSplatData& data, const ConversionOptions& options);
    bool writeGLB(const std::string& filename, const std::string& json);
    bool writeGLTF(const std::string& filename, const std::string& json);
    
    // Calculate actual color bounds from data
    void calculateColorBounds(const GaussianSplatData& data, 
                              std::array<float, 4>& minColor, 
                              std::array<float, 4>& maxColor) const;
    
    // Display statistics about buffer layout
    void displayStatistics(const GaussianSplatData& data, const ConversionOptions& options) const;
    
private:
    
    // Attribute organization for progressive loading
    struct BufferViewInfo {
        size_t offset;
        size_t length;
        size_t stride;
    };
    
    BufferViewInfo addPositions(const GaussianSplatData& data);
    BufferViewInfo addColors(const GaussianSplatData& data);
    BufferViewInfo addOrientations(const GaussianSplatData& data);
    BufferViewInfo addScales(const GaussianSplatData& data);
    BufferViewInfo addSHCoeffsFirst(const GaussianSplatData& data);
    BufferViewInfo addSHCoeffsSecond(const GaussianSplatData& data);
    BufferViewInfo addSHCoeffsThird(const GaussianSplatData& data);
};

} // namespace ply2gltf

#endif // GLTF_WRITER_H
