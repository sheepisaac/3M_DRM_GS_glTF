#ifndef GAUSSIAN_SPLAT_H
#define GAUSSIAN_SPLAT_H

#include <vector>
#include <array>
#include <cstdint>
#include <string>
#include <algorithm>
// ## DRM MODIFIED: ADDED HEADERS FOR HELPER FUNCTION ##
#include <set>
#include <sstream>


namespace ply2gltf {

// ## MULTIDRM: DRM System Types ##
enum class DRMSystem {
    SIMPLE_XOR,     // Simple XOR encryption (for testing)
    WIDEVINE,       // Google Widevine
    PLAYREADY       // Microsoft PlayReady
};

// ## MULTIDRM: DRM System Configuration ##
struct DRMConfig {
    DRMSystem system = DRMSystem::SIMPLE_XOR;
    std::string schemeIdUri;      // DRM scheme URI (e.g., "urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd5121ed8" for Widevine)
    std::string keyId;            // Key ID (hex string)
    std::string key;             // Encryption key (for simple XOR, or key material for commercial DRM)
    std::string pssh;             // PSSH box data (base64 encoded, for Widevine/PlayReady)
    std::string licenseUrl;      // License server URL (optional)
    
    DRMConfig() = default;
    DRMConfig(DRMSystem sys, const std::string& uri, const std::string& kid, const std::string& k = "")
        : system(sys), schemeIdUri(uri), keyId(kid), key(k) {}
};

// Structure representing a single Gaussian splat
struct GaussianSplat {
    // Position
    std::array<float, 3> position;
    
    // Normals (unused but present in PLY)
    std::array<float, 3> normal;
    
    // Spherical harmonics DC components (base color)
    std::array<float, 3> sh_dc;
    
    // Spherical harmonics rest components (45 values)
    std::array<float, 45> sh_rest;
    
    // Opacity
    float opacity;
    
    // Scale (in log space)
    std::array<float, 3> scale;
    
    // Rotation quaternion
    std::array<float, 4> rotation;
};

// Container for all Gaussian splats
struct GaussianSplatData {
    std::vector<GaussianSplat> splats;
    
    // Bounding box
    std::array<float, 3> min_bounds;
    std::array<float, 3> max_bounds;
    
    // Statistics
    size_t count() const { return splats.size(); }
    
    void computeBounds() {
        if (splats.empty()) return;
        
        min_bounds = splats[0].position;
        max_bounds = splats[0].position;
        
        for (const auto& splat : splats) {
            for (int i = 0; i < 3; ++i) {
                min_bounds[i] = std::min(min_bounds[i], splat.position[i]);
                max_bounds[i] = std::max(max_bounds[i], splat.position[i]);
            }
        }
    }
};

// Options for conversion
struct ConversionOptions {
    bool progressive = false;      // Organize data for progressive loading
    bool verbose = false;          // Enable verbose output
    bool stats = false;            // Display detailed file structure statistics
    bool binary = true;            // Output as GLB (vs separate glTF + bin)
    bool basic_pointcloud = false; // Export only POSITION and COLOR_0 (no GS extension)
    bool packed = false;           // Use tightly packed data instead of interleaved (default: interleaved)
    std::string input_file;        // Input PLY file
    std::string output_file;       // Output glTF/GLB file
    std::string viewpoint_file;    // Optional viewpoint file to embed camera

    // ## MULTIDRM: MULTI-DRM CONFIGURATION ##
    bool drm_enabled = false;
    std::vector<DRMConfig> drm_configs;  // Multiple DRM systems support
    std::string drm_encrypted_attributes;  // Comma-separated list of attributes to encrypt
    
    // Legacy single DRM support (for backward compatibility)
    std::string drm_key;      // Deprecated: use drm_configs instead
    std::string drm_key_id;   // Deprecated: use drm_configs instead

    // ## MULTIDRM: HELPER FUNCTION ##
    // Helper function to check if an attribute should be encrypted
    bool isAttributeEncrypted(const std::string& attrName) const {
        if (!drm_enabled) {
            return false;
        }
        // If drm_encrypted_attributes is empty, encrypt all attributes by default
        if (drm_encrypted_attributes.empty()) {
            return true;  // Default: encrypt all attributes when DRM is enabled
        }
        // Use stringstream to split the comma-separated string
        std::stringstream ss(drm_encrypted_attributes);
        std::string item;
        while (std::getline(ss, item, ',')) {
            // Trim leading/trailing whitespace if any
            item.erase(0, item.find_first_not_of(" \t\n\r"));
            item.erase(item.find_last_not_of(" \t\n\r") + 1);
            if (item == attrName) {
                return true;
            }
        }
        return false;
    }
    
    // ## MULTIDRM: Get primary DRM key (for backward compatibility) ##
    std::string getPrimaryDRMKey() const {
        if (!drm_configs.empty() && !drm_configs[0].key.empty()) {
            return drm_configs[0].key;
        }
        return drm_key;  // Fallback to legacy key
    }
    
    // ## MULTIDRM: Get primary DRM key ID (for backward compatibility) ##
    std::string getPrimaryDRMKeyId() const {
        if (!drm_configs.empty() && !drm_configs[0].keyId.empty()) {
            return drm_configs[0].keyId;
        }
        return drm_key_id;  // Fallback to legacy key ID
    }
};

// ## MULTI-OBJECT: Object Transformation Info ##
struct ObjectTransform {
    std::string name;                  // Object name/identifier
    std::array<float, 3> translation = {0.0f, 0.0f, 0.0f};  // x, y, z translation
    std::array<float, 3> rotation = {0.0f, 0.0f, 0.0f};    // x, y, z rotation in degrees
    std::array<float, 3> scale = {1.0f, 1.0f, 1.0f};        // x, y, z scale
    
    // ## MULTIDRM: Per-object DRM configuration ##
    bool drm_enabled = false;          // Enable DRM for this object
    DRMConfig drm_config;              // Legacy primary DRM configuration for this object
    std::vector<DRMConfig> drm_configs; // Multiple DRM systems for this object
    std::string drm_encrypted_attributes;  // Comma-separated list of attributes to encrypt (e.g., "position,color,rotation")
    
    ObjectTransform() = default;
    ObjectTransform(const std::string& n, float tx, float ty, float tz, 
                   float rx, float ry, float rz, float sx, float sy, float sz)
        : name(n), translation({tx, ty, tz}), rotation({rx, ry, rz}), scale({sx, sy, sz}) {}

    const DRMConfig& primaryDRMConfig() const {
        if (!drm_configs.empty()) {
            return drm_configs.front();
        }
        return drm_config;
    }
    
    // Helper function to check if an attribute should be encrypted for this object
    bool isAttributeEncrypted(const std::string& attrName) const {
        if (!drm_enabled) {
            return false;
        }
        // If drm_encrypted_attributes is empty, encrypt all attributes by default
        if (drm_encrypted_attributes.empty()) {
            return true;  // Default: encrypt all attributes when DRM is enabled
        }
        // Check if the attribute is in the list
        std::string lowerAttr = attrName;
        std::transform(lowerAttr.begin(), lowerAttr.end(), lowerAttr.begin(), ::tolower);
        std::string lowerList = drm_encrypted_attributes;
        std::transform(lowerList.begin(), lowerList.end(), lowerList.begin(), ::tolower);
        return lowerList.find(lowerAttr) != std::string::npos;
    }
};

struct LayerInfo {
    std::string id;
    std::string role = "enhancement";
    std::string ply;
    std::string composition = "additive";
    std::vector<std::string> dependsOn;

    bool drm_enabled = false;
    bool has_drm_policy = false;
    std::vector<DRMConfig> drm_configs;
    std::string drm_encrypted_attributes;

    bool isAttributeEncrypted(const std::string& attrName) const {
        if (!drm_enabled) {
            return false;
        }
        if (drm_encrypted_attributes.empty()) {
            return true;
        }
        std::string lowerAttr = attrName;
        std::transform(lowerAttr.begin(), lowerAttr.end(), lowerAttr.begin(), ::tolower);
        std::string lowerList = drm_encrypted_attributes;
        std::transform(lowerList.begin(), lowerList.end(), lowerList.begin(), ::tolower);
        return lowerList.find(lowerAttr) != std::string::npos;
    }

    const DRMConfig& primaryDRMConfig() const {
        return drm_configs.front();
    }
};

struct LayeredObjectInfo {
    ObjectTransform transform;
    std::vector<LayerInfo> layers;
};

// Options for sequence conversion (extends ConversionOptions)
struct SequenceConversionOptions : public ConversionOptions {
    bool isSequence = false;           // Is this a sequence conversion
    bool streaming = false;            // Use memory-efficient streaming mode
    bool useChunks = false;            // Write frames 1..N as SPLT chunks in GLB
    float frameRate = 30.0f;           // Frame rate for sequence playback
    std::vector<std::string> inputFiles; // Input PLY files for sequence
    
    // ## MULTI-OBJECT: Multi-object scene support ##
    bool isMultiObject = false;        // Is this a multi-object scene
    std::vector<std::string> objectFiles;  // Input PLY files for each object
    std::vector<ObjectTransform> objectTransforms;  // Transformation for each object

    bool isLayered = false;           // Is this a manifest-driven multi-layer scene
    std::string manifest_file;        // Manifest for objects/layers/DRM policies
};

} // namespace ply2gltf

#endif // GAUSSIAN_SPLAT_H
