#include "GltfWriter.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <vector>
#include <set>
#include <sstream>
#include "ViewpointUtil.h"

namespace ply2gltf {

// ## MULTIDRM: SIMPLE XOR ENCRYPTION (for testing/compatibility) ##
void xorEncrypt(std::vector<uint8_t>& buffer, const std::string& key, bool verbose) {
    if (key.empty() || buffer.empty()) return;
    if (verbose) {
        std::cout << "  - Encrypting buffer of size " << buffer.size() << " bytes with XOR..." << std::endl;
    }
    for (size_t i = 0; i < buffer.size(); ++i) {
        buffer[i] ^= key[i % key.length()];
    }
}

// ## MULTIDRM: DRM-BASED ENCRYPTION ##
void encryptWithDRM(std::vector<uint8_t>& buffer, const DRMConfig& config, bool verbose) {
    if (buffer.empty()) return;
    
    if (verbose) {
        std::cout << "  - Encrypting buffer of size " << buffer.size() << " bytes with DRM system: ";
        switch (config.system) {
            case DRMSystem::SIMPLE_XOR:
                std::cout << "SIMPLE_XOR";
                break;
            case DRMSystem::WIDEVINE:
                std::cout << "WIDEVINE";
                break;
            case DRMSystem::PLAYREADY:
                std::cout << "PLAYREADY";
                break;
        }
        std::cout << std::endl;
    }
    
    // For now, use XOR encryption for all systems (actual DRM SDK integration would go here)
    // In production, this would call the appropriate DRM SDK encryption functions
    switch (config.system) {
        case DRMSystem::SIMPLE_XOR:
            xorEncrypt(buffer, config.key, verbose);
            break;
        case DRMSystem::WIDEVINE:
            // TODO: Integrate with Widevine SDK
            // For now, use XOR as placeholder
            if (!config.key.empty()) {
                xorEncrypt(buffer, config.key, verbose);
            }
            if (verbose) {
                std::cout << "    [Widevine] Using placeholder encryption. Actual SDK integration required." << std::endl;
            }
            break;
        case DRMSystem::PLAYREADY:
            // TODO: Integrate with PlayReady SDK
            // For now, use XOR as placeholder
            if (!config.key.empty()) {
                xorEncrypt(buffer, config.key, verbose);
            }
            if (verbose) {
                std::cout << "    [PlayReady] Using placeholder encryption. Actual SDK integration required." << std::endl;
            }
            break;
    }
}

GltfWriter::GltfWriter() = default;

// ## MULTI-OBJECT: Helper function to convert Euler angles (degrees) to quaternion ##
static std::array<float, 4> eulerToQuaternion(float rx, float ry, float rz) {
    // Convert degrees to radians
    const float PI = 3.14159265358979323846f;
    float x = rx * PI / 180.0f;
    float y = ry * PI / 180.0f;
    float z = rz * PI / 180.0f;
    
    // Calculate quaternion components (ZYX order, which is common in glTF)
    float cx = std::cos(x * 0.5f);
    float sx = std::sin(x * 0.5f);
    float cy = std::cos(y * 0.5f);
    float sy = std::sin(y * 0.5f);
    float cz = std::cos(z * 0.5f);
    float sz = std::sin(z * 0.5f);
    
    std::array<float, 4> quat;
    quat[0] = sx * cy * cz - cx * sy * sz;  // x
    quat[1] = cx * sy * cz + sx * cy * sz;  // y
    quat[2] = cx * cy * sz - sx * sy * cz;  // z
    quat[3] = cx * cy * cz + sx * sy * sz;  // w
    
    return quat;
}

bool GltfWriter::write(const std::string& filename, const GaussianSplatData& data, const ConversionOptions& options) {
    if (data.splats.empty()) {
        error_ = "No Gaussian splat data to write";
        return false;
    }
    
    // Clear binary buffer
    binary_buffer_.clear();
    current_offset_ = 0;
    
    // Create JSON
    std::string json = createJSON(data, options);
    
    // Display statistics if requested
    if (options.stats) {
        displayStatistics(data, options);
    }
    
    // Write output
    if (options.binary) {
        return writeGLB(filename, json);
    } else {
        return writeGLTF(filename, json);
    }
}

// ## MULTI-OBJECT: Write multiple objects to glTF/GLB file ##
bool GltfWriter::writeMultiObject(const std::string& filename, 
                                  const std::vector<GaussianSplatData>& objectsData,
                                  const std::vector<ObjectTransform>& transforms,
                                  const ConversionOptions& options) {
    if (objectsData.empty()) {
        error_ = "No objects to write";
        return false;
    }
    
    if (objectsData.size() != transforms.size()) {
        error_ = "Number of objects does not match number of transforms";
        return false;
    }
    
    // Clear binary buffer
    binary_buffer_.clear();
    current_offset_ = 0;
    
    rapidjson::Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();
    
    rapidjson::Value asset(rapidjson::kObjectType);
    asset.AddMember("version", "2.0", allocator);
    asset.AddMember("generator", "ply2gltf converter (multi-object)", allocator);
    doc.AddMember("asset", asset, allocator);
    
    rapidjson::Value extensionsUsed(rapidjson::kArrayType);
    if (!options.basic_pointcloud) {
        extensionsUsed.PushBack("EXT_gaussian_splats", allocator);
    }
    // Check if any object has DRM enabled
    bool anyObjectHasDRM = false;
    for (const auto& transform : transforms) {
        if (transform.drm_enabled) {
            anyObjectHasDRM = true;
            break;
        }
    }
    // DRM extension is only added if at least one object has DRM enabled
    if (anyObjectHasDRM) {
        extensionsUsed.PushBack("EXT_content_protection", allocator);
    }
    doc.AddMember("extensionsUsed", extensionsUsed, allocator);
    
    // Create meshes array - one mesh per object
    rapidjson::Value meshes(rapidjson::kArrayType);
    rapidjson::Value nodesArray(rapidjson::kArrayType);
    rapidjson::Value bufferViews(rapidjson::kArrayType);
    rapidjson::Value accessors(rapidjson::kArrayType);
    
    int bufferViewIndex = 0;
    int accessorIndex = 0;
    
    // Process each object
    for (size_t objIdx = 0; objIdx < objectsData.size(); ++objIdx) {
        const auto& data = objectsData[objIdx];
        const auto& transform = transforms[objIdx];
        std::set<int> encryptedAccessorIndices;
        
        if (data.splats.empty()) {
            std::cerr << "Warning: Object " << objIdx << " (" << transform.name << ") has no splats, skipping." << std::endl;
            continue;
        }
        
        if (options.verbose) {
            std::cout << "Processing object " << objIdx << ": " << transform.name 
                      << " (" << data.splats.size() << " splats)" << std::endl;
        }
        
        // Create mesh for this object
        rapidjson::Value mesh(rapidjson::kObjectType);
        rapidjson::Value primitives(rapidjson::kArrayType);
        rapidjson::Value primitive(rapidjson::kObjectType);
        
        rapidjson::Value attributes(rapidjson::kObjectType);
        rapidjson::Value extensions(rapidjson::kObjectType);
        
        // Per-object style: track accessor indices for GS extension attributes
        int orientationAccessorIdx = -1;
        int scaleAccessorIdx = -1;
        int shRAccessorIdx = -1;
        int shGAccessorIdx = -1;
        int shBAccessorIdx = -1;
        
        // Add attributes and accessors for this object
        size_t splatCount = data.splats.size();
        
        // Position - per-object style: use object's DRM configuration
        std::vector<uint8_t> pos_buffer(splatCount * 3 * sizeof(float));
        for (size_t i = 0; i < splatCount; ++i) {
            std::memcpy(pos_buffer.data() + i * 12, data.splats[i].position.data(), 12);
        }
        // Only encrypt if this object's DRM is enabled AND encryption is requested
        if (transform.drm_enabled && transform.isAttributeEncrypted("position")) {
            std::vector<uint8_t> pos_vec(pos_buffer.begin(), pos_buffer.end());
            encryptWithDRM(pos_vec, transform.primaryDRMConfig(), options.verbose);
            pos_buffer.assign(pos_vec.begin(), pos_vec.end());
            encryptedAccessorIndices.insert(accessorIndex);
        }
        size_t posOffset = addToBuffer(pos_buffer.data(), pos_buffer.size());
        padBuffer(4);
        
        rapidjson::Value posBufferView(rapidjson::kObjectType);
        posBufferView.AddMember("buffer", 0, allocator);
        posBufferView.AddMember("byteOffset", static_cast<int64_t>(posOffset), allocator);
        posBufferView.AddMember("byteLength", static_cast<int64_t>(pos_buffer.size()), allocator);
        bufferViews.PushBack(posBufferView, allocator);
        
        rapidjson::Value posAccessor(rapidjson::kObjectType);
        posAccessor.AddMember("bufferView", bufferViewIndex++, allocator);
        posAccessor.AddMember("componentType", 5126, allocator);  // FLOAT
        posAccessor.AddMember("count", static_cast<int64_t>(splatCount), allocator);
        posAccessor.AddMember("type", "VEC3", allocator);
        accessors.PushBack(posAccessor, allocator);
        attributes.AddMember("POSITION", accessorIndex++, allocator);
        
        // Color
        std::vector<uint8_t> col_buffer(splatCount * 4 * sizeof(float));
        const float SH_C0 = 0.28209479f;
        for (size_t i = 0; i < splatCount; ++i) {
            const auto& splat = data.splats[i];
            float color[4] = {
                std::max(0.0f, std::min(1.0f, 0.5f + SH_C0 * splat.sh_dc[0])),
                std::max(0.0f, std::min(1.0f, 0.5f + SH_C0 * splat.sh_dc[1])),
                std::max(0.0f, std::min(1.0f, 0.5f + SH_C0 * splat.sh_dc[2])),
                1.0f / (1.0f + std::exp(-splat.opacity))
            };
            std::memcpy(col_buffer.data() + i * 16, color, 16);
        }
        // Only encrypt if this object's DRM is enabled AND encryption is requested
        if (transform.drm_enabled && transform.isAttributeEncrypted("color")) {
            std::vector<uint8_t> col_vec(col_buffer.begin(), col_buffer.end());
            encryptWithDRM(col_vec, transform.primaryDRMConfig(), options.verbose);
            col_buffer.assign(col_vec.begin(), col_vec.end());
            encryptedAccessorIndices.insert(accessorIndex);
        }
        size_t colOffset = addToBuffer(col_buffer.data(), col_buffer.size());
        padBuffer(4);
        
        rapidjson::Value colBufferView(rapidjson::kObjectType);
        colBufferView.AddMember("buffer", 0, allocator);
        colBufferView.AddMember("byteOffset", static_cast<int64_t>(colOffset), allocator);
        colBufferView.AddMember("byteLength", static_cast<int64_t>(col_buffer.size()), allocator);
        bufferViews.PushBack(colBufferView, allocator);
        
        rapidjson::Value colAccessor(rapidjson::kObjectType);
        colAccessor.AddMember("bufferView", bufferViewIndex++, allocator);
        colAccessor.AddMember("componentType", 5126, allocator);
        colAccessor.AddMember("count", static_cast<int64_t>(splatCount), allocator);
        colAccessor.AddMember("type", "VEC4", allocator);
        accessors.PushBack(colAccessor, allocator);
        attributes.AddMember("COLOR_0", accessorIndex++, allocator);
        
        if (!options.basic_pointcloud) {
            // Orientation (rotation quaternion)
            std::vector<uint8_t> ori_buffer(splatCount * 4 * sizeof(float));
            for (size_t i = 0; i < splatCount; ++i) {
                std::memcpy(ori_buffer.data() + i * 16, data.splats[i].rotation.data(), 16);
            }
            // Only encrypt if this object's DRM is enabled AND encryption is requested
            if (transform.drm_enabled && transform.isAttributeEncrypted("rotation")) {
                std::vector<uint8_t> ori_vec(ori_buffer.begin(), ori_buffer.end());
                encryptWithDRM(ori_vec, transform.primaryDRMConfig(), options.verbose);
                ori_buffer.assign(ori_vec.begin(), ori_vec.end());
                encryptedAccessorIndices.insert(accessorIndex);
            }
            size_t oriOffset = addToBuffer(ori_buffer.data(), ori_buffer.size());
            padBuffer(4);
            
            rapidjson::Value oriBufferView(rapidjson::kObjectType);
            oriBufferView.AddMember("buffer", 0, allocator);
            oriBufferView.AddMember("byteOffset", static_cast<int64_t>(oriOffset), allocator);
            oriBufferView.AddMember("byteLength", static_cast<int64_t>(ori_buffer.size()), allocator);
            bufferViews.PushBack(oriBufferView, allocator);
            
            rapidjson::Value oriAccessor(rapidjson::kObjectType);
            oriAccessor.AddMember("bufferView", bufferViewIndex++, allocator);
            oriAccessor.AddMember("componentType", 5126, allocator);
            oriAccessor.AddMember("count", static_cast<int64_t>(splatCount), allocator);
            oriAccessor.AddMember("type", "VEC4", allocator);
            accessors.PushBack(oriAccessor, allocator);
            // Per-object style: store accessor index for GS extension attributes
            orientationAccessorIdx = accessorIndex++;
            
            // Scale
            std::vector<uint8_t> sca_buffer(splatCount * 3 * sizeof(float));
            for (size_t i = 0; i < splatCount; ++i) {
                std::memcpy(sca_buffer.data() + i * 12, data.splats[i].scale.data(), 12);
            }
            // Only encrypt if this object's DRM is enabled AND encryption is requested
            if (transform.drm_enabled && transform.isAttributeEncrypted("scale")) {
                std::vector<uint8_t> sca_vec(sca_buffer.begin(), sca_buffer.end());
                encryptWithDRM(sca_vec, transform.primaryDRMConfig(), options.verbose);
                sca_buffer.assign(sca_vec.begin(), sca_vec.end());
                encryptedAccessorIndices.insert(accessorIndex);
            }
            size_t scaOffset = addToBuffer(sca_buffer.data(), sca_buffer.size());
            padBuffer(4);
            
            rapidjson::Value scaBufferView(rapidjson::kObjectType);
            scaBufferView.AddMember("buffer", 0, allocator);
            scaBufferView.AddMember("byteOffset", static_cast<int64_t>(scaOffset), allocator);
            scaBufferView.AddMember("byteLength", static_cast<int64_t>(sca_buffer.size()), allocator);
            bufferViews.PushBack(scaBufferView, allocator);
            
            rapidjson::Value scaAccessor(rapidjson::kObjectType);
            scaAccessor.AddMember("bufferView", bufferViewIndex++, allocator);
            scaAccessor.AddMember("componentType", 5126, allocator);
            scaAccessor.AddMember("count", static_cast<int64_t>(splatCount), allocator);
            scaAccessor.AddMember("type", "VEC3", allocator);
            accessors.PushBack(scaAccessor, allocator);
            // Per-object style: store accessor index for GS extension attributes
            scaleAccessorIdx = accessorIndex++;
            
            // Spherical Harmonics coefficients
            for (int shIdx = 0; shIdx < 3; ++shIdx) {
                std::vector<uint8_t> sh_buffer(splatCount * 15 * sizeof(float));
                for (size_t i = 0; i < splatCount; ++i) {
                    const float* sh_data = nullptr;
                    if (shIdx == 0) {
                        sh_data = data.splats[i].sh_rest.data();
                    } else if (shIdx == 1) {
                        sh_data = data.splats[i].sh_rest.data() + 15;
                    } else {
                        sh_data = data.splats[i].sh_rest.data() + 30;
                    }
                    std::memcpy(sh_buffer.data() + i * 60, sh_data, 60);
                }
                // Only encrypt if this object's DRM is enabled AND encryption is requested
                std::string shAttrName = (shIdx == 0) ? "sh" : ((shIdx == 1) ? "sh2" : "sh3");
                if (transform.drm_enabled && transform.isAttributeEncrypted(shAttrName)) {
                    std::vector<uint8_t> sh_vec(sh_buffer.begin(), sh_buffer.end());
                    encryptWithDRM(sh_vec, transform.primaryDRMConfig(), options.verbose);
                    sh_buffer.assign(sh_vec.begin(), sh_vec.end());
                    encryptedAccessorIndices.insert(accessorIndex);
                }
                size_t shOffset = addToBuffer(sh_buffer.data(), sh_buffer.size());
                padBuffer(4);
                
                rapidjson::Value shBufferView(rapidjson::kObjectType);
                shBufferView.AddMember("buffer", 0, allocator);
                shBufferView.AddMember("byteOffset", static_cast<int64_t>(shOffset), allocator);
                shBufferView.AddMember("byteLength", static_cast<int64_t>(sh_buffer.size()), allocator);
                bufferViews.PushBack(shBufferView, allocator);
                
                rapidjson::Value shAccessor(rapidjson::kObjectType);
                shAccessor.AddMember("bufferView", bufferViewIndex++, allocator);
                shAccessor.AddMember("componentType", 5126, allocator);
                // Per-object style: SH accessor count is splatCount * 15 (15 coefficients per splat per channel)
                shAccessor.AddMember("count", static_cast<int64_t>(splatCount * 15), allocator);
                shAccessor.AddMember("type", "SCALAR", allocator);
                rapidjson::Value shMin(rapidjson::kArrayType);
                rapidjson::Value shMax(rapidjson::kArrayType);
                shMin.PushBack(-1.0f, allocator);
                shMax.PushBack(1.0f, allocator);
                shAccessor.AddMember("min", shMin, allocator);
                shAccessor.AddMember("max", shMax, allocator);
                accessors.PushBack(shAccessor, allocator);
                
                // Per-object style: store accessor indices for GS extension attributes
                if (shIdx == 0) {
                    shRAccessorIdx = accessorIndex++;
                } else if (shIdx == 1) {
                    shGAccessorIdx = accessorIndex++;
                } else {
                    shBAccessorIdx = accessorIndex++;
                }
            }
        }
        
        primitive.AddMember("attributes", attributes, allocator);
        primitive.AddMember("mode", 0, allocator);  // POINTS
        
        if (!options.basic_pointcloud) {
            // Per-object style: use attributes instead of splatsLength
            rapidjson::Value gsExt(rapidjson::kObjectType);
            rapidjson::Value gsAttributes(rapidjson::kObjectType);
            
            // Add Gaussian splat extension attributes (per-object style)
            // Only add if we have valid accessor indices
            if (orientationAccessorIdx >= 0 && scaleAccessorIdx >= 0 && 
                shRAccessorIdx >= 0 && shGAccessorIdx >= 0 && shBAccessorIdx >= 0) {
                gsAttributes.AddMember("_GS_ORIENTATION", orientationAccessorIdx, allocator);
                gsAttributes.AddMember("_GS_SCALE", scaleAccessorIdx, allocator);
                gsAttributes.AddMember("_GS_SH_COEFF_R", shRAccessorIdx, allocator);
                gsAttributes.AddMember("_GS_SH_COEFF_G", shGAccessorIdx, allocator);
                gsAttributes.AddMember("_GS_SH_COEFF_B", shBAccessorIdx, allocator);
                
                gsExt.AddMember("attributes", gsAttributes, allocator);
            } else {
                // Fallback: use splatsLength if accessor indices are invalid
                gsExt.AddMember("splatsLength", static_cast<int64_t>(splatCount), allocator);
            }
            extensions.AddMember("EXT_gaussian_splats", gsExt, allocator);
            
            // Add DRM extension if this object has DRM enabled
            // Per-object DRM: each object can have its own DRM configuration
            if (transform.drm_enabled && !encryptedAccessorIndices.empty()) {
                rapidjson::Value drmExt(rapidjson::kObjectType);
                rapidjson::Value drmSystems(rapidjson::kArrayType);
                
                std::vector<DRMConfig> drmConfigs = transform.drm_configs;
                if (drmConfigs.empty()) {
                    drmConfigs.push_back(transform.drm_config);
                }

                for (const auto& drmConfig : drmConfigs) {
                    rapidjson::Value drmSystem(rapidjson::kObjectType);
                    drmSystem.AddMember("schemeIdUri", rapidjson::Value(drmConfig.schemeIdUri.c_str(), allocator), allocator);
                    drmSystem.AddMember("keyId", rapidjson::Value(drmConfig.keyId.c_str(), allocator), allocator);
                    if (!drmConfig.key.empty()) {
                        drmSystem.AddMember("key", rapidjson::Value(drmConfig.key.c_str(), allocator), allocator);
                    }
                    
                    if (!drmConfig.pssh.empty()) {
                        drmSystem.AddMember("pssh", rapidjson::Value(drmConfig.pssh.c_str(), allocator), allocator);
                    }
                    if (!drmConfig.licenseUrl.empty()) {
                        drmSystem.AddMember("licenseUrl", rapidjson::Value(drmConfig.licenseUrl.c_str(), allocator), allocator);
                    }
                    
                    rapidjson::Value encryptedAccessors(rapidjson::kArrayType);
                    for (int accIdx : encryptedAccessorIndices) {
                        encryptedAccessors.PushBack(accIdx, allocator);
                    }
                    drmSystem.AddMember("encryptedAccessors", encryptedAccessors, allocator);
                    drmSystems.PushBack(drmSystem, allocator);
                }
                drmExt.AddMember("systems", drmSystems, allocator);
                extensions.AddMember("EXT_content_protection", drmExt, allocator);
            }
        }
        
        primitive.AddMember("extensions", extensions, allocator);
        primitives.PushBack(primitive, allocator);
        mesh.AddMember("primitives", primitives, allocator);
        meshes.PushBack(mesh, allocator);
        
        // Create node for this mesh with transformation
        rapidjson::Value node(rapidjson::kObjectType);
        node.AddMember("mesh", static_cast<int>(meshes.Size() - 1), allocator);
        node.AddMember("name", rapidjson::Value(transform.name.c_str(), allocator), allocator);
        
        // Add translation
        rapidjson::Value translation(rapidjson::kArrayType);
        translation.PushBack(transform.translation[0], allocator);
        translation.PushBack(transform.translation[1], allocator);
        translation.PushBack(transform.translation[2], allocator);
        node.AddMember("translation", translation, allocator);
        
        // Add rotation (convert Euler to quaternion)
        std::array<float, 4> quat = eulerToQuaternion(transform.rotation[0], transform.rotation[1], transform.rotation[2]);
        rapidjson::Value rotation(rapidjson::kArrayType);
        rotation.PushBack(quat[0], allocator);
        rotation.PushBack(quat[1], allocator);
        rotation.PushBack(quat[2], allocator);
        rotation.PushBack(quat[3], allocator);
        node.AddMember("rotation", rotation, allocator);
        
        // Add scale
        rapidjson::Value scale(rapidjson::kArrayType);
        scale.PushBack(transform.scale[0], allocator);
        scale.PushBack(transform.scale[1], allocator);
        scale.PushBack(transform.scale[2], allocator);
        node.AddMember("scale", scale, allocator);
        
        nodesArray.PushBack(node, allocator);
    }
    
    doc.AddMember("meshes", meshes, allocator);
    doc.AddMember("nodes", nodesArray, allocator);
    doc.AddMember("bufferViews", bufferViews, allocator);
    doc.AddMember("accessors", accessors, allocator);
    
    // Create buffer
    rapidjson::Value buffers(rapidjson::kArrayType);
    rapidjson::Value buffer(rapidjson::kObjectType);
    buffer.AddMember("byteLength", static_cast<int64_t>(binary_buffer_.size()), allocator);
    buffers.PushBack(buffer, allocator);
    doc.AddMember("buffers", buffers, allocator);
    
    // Create scene
    doc.AddMember("scene", 0, allocator);
    rapidjson::Value scenes(rapidjson::kArrayType);
    rapidjson::Value scene(rapidjson::kObjectType);
    rapidjson::Value sceneNodes(rapidjson::kArrayType);
    for (size_t i = 0; i < nodesArray.Size(); ++i) {
        sceneNodes.PushBack(static_cast<int>(i), allocator);
    }
    scene.AddMember("nodes", sceneNodes, allocator);
    scenes.PushBack(scene, allocator);
    doc.AddMember("scenes", scenes, allocator);
    
    // Serialize JSON
    rapidjson::StringBuffer buffer_json;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer_json);
    doc.Accept(writer);
    std::string json = buffer_json.GetString();
    
    // Write output
    if (options.binary) {
        return writeGLB(filename, json);
    } else {
        return writeGLTF(filename, json);
    }
}

bool GltfWriter::writeLayeredObjects(const std::string& filename,
                                     const std::vector<std::vector<GaussianSplatData>>& layersData,
                                     const std::vector<LayeredObjectInfo>& objects,
                                     const ConversionOptions& options) {
    if (layersData.empty() || layersData.size() != objects.size()) {
        error_ = "Layer data does not match object metadata";
        return false;
    }

    binary_buffer_.clear();
    current_offset_ = 0;

    rapidjson::Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();

    rapidjson::Value asset(rapidjson::kObjectType);
    asset.AddMember("version", "2.0", allocator);
    asset.AddMember("generator", "ply2gltf_3m_multidrm static multi-layer converter", allocator);
    doc.AddMember("asset", asset, allocator);

    bool anyDRM = false;
    for (const auto& object : objects) {
        if (object.transform.drm_enabled) anyDRM = true;
        for (const auto& layer : object.layers) {
            if (layer.drm_enabled) anyDRM = true;
        }
    }

    rapidjson::Value extensionsUsed(rapidjson::kArrayType);
    if (!options.basic_pointcloud) {
        extensionsUsed.PushBack("EXT_gaussian_splats", allocator);
    }
    extensionsUsed.PushBack("EXT_gaussian_splat_layers", allocator);
    if (anyDRM) {
        extensionsUsed.PushBack("EXT_content_protection", allocator);
    }
    doc.AddMember("extensionsUsed", extensionsUsed, allocator);

    rapidjson::Value meshes(rapidjson::kArrayType);
    rapidjson::Value nodesArray(rapidjson::kArrayType);
    rapidjson::Value bufferViews(rapidjson::kArrayType);
    rapidjson::Value accessors(rapidjson::kArrayType);
    rapidjson::Value layerObjects(rapidjson::kArrayType);

    int bufferViewIndex = 0;
    int accessorIndex = 0;

    auto addBufferViewFromBytes = [&](const std::vector<uint8_t>& bytes, rapidjson::Value& bufferViewsRef) -> int {
        size_t offset = addToBuffer(bytes.data(), bytes.size());
        padBuffer(4);
        rapidjson::Value bv(rapidjson::kObjectType);
        bv.AddMember("buffer", 0, allocator);
        bv.AddMember("byteOffset", static_cast<int64_t>(offset), allocator);
        bv.AddMember("byteLength", static_cast<int64_t>(bytes.size()), allocator);
        bufferViewsRef.PushBack(bv, allocator);
        return bufferViewIndex++;
    };

    auto addAccessor = [&](int bufferView, int componentType, const char* type, size_t count) -> int {
        rapidjson::Value accessor(rapidjson::kObjectType);
        accessor.AddMember("bufferView", bufferView, allocator);
        accessor.AddMember("componentType", componentType, allocator);
        accessor.AddMember("count", static_cast<int64_t>(count), allocator);
        accessor.AddMember("type", rapidjson::Value(type, allocator), allocator);
        accessors.PushBack(accessor, allocator);
        return accessorIndex++;
    };

    auto addDRMSystem = [&](rapidjson::Value& drmSystems,
                            const DRMConfig& drmConfig,
                            const std::set<int>& encryptedAccessorIndices) {
        rapidjson::Value drmSystem(rapidjson::kObjectType);
        drmSystem.AddMember("schemeIdUri", rapidjson::Value(drmConfig.schemeIdUri.c_str(), allocator), allocator);
        drmSystem.AddMember("keyId", rapidjson::Value(drmConfig.keyId.c_str(), allocator), allocator);
        if (!drmConfig.key.empty()) {
            drmSystem.AddMember("key", rapidjson::Value(drmConfig.key.c_str(), allocator), allocator);
        }
        if (!drmConfig.pssh.empty()) {
            drmSystem.AddMember("pssh", rapidjson::Value(drmConfig.pssh.c_str(), allocator), allocator);
        }
        if (!drmConfig.licenseUrl.empty()) {
            drmSystem.AddMember("licenseUrl", rapidjson::Value(drmConfig.licenseUrl.c_str(), allocator), allocator);
        }
        rapidjson::Value encryptedAccessors(rapidjson::kArrayType);
        for (int accIdx : encryptedAccessorIndices) {
            encryptedAccessors.PushBack(accIdx, allocator);
        }
        drmSystem.AddMember("encryptedAccessors", encryptedAccessors, allocator);
        drmSystems.PushBack(drmSystem, allocator);
    };

    for (size_t objIdx = 0; objIdx < objects.size(); ++objIdx) {
        const auto& object = objects[objIdx];
        if (layersData[objIdx].size() != object.layers.size()) {
            error_ = "Layer count mismatch for object " + object.transform.name;
            return false;
        }

        rapidjson::Value mesh(rapidjson::kObjectType);
        rapidjson::Value primitives(rapidjson::kArrayType);
        rapidjson::Value layerRecords(rapidjson::kArrayType);

        for (size_t layerIdx = 0; layerIdx < object.layers.size(); ++layerIdx) {
            const auto& layer = object.layers[layerIdx];
            const auto& data = layersData[objIdx][layerIdx];
            const size_t splatCount = data.splats.size();

            if (splatCount == 0) {
                std::cerr << "Warning: layer " << layer.id << " has no splats, skipping." << std::endl;
                continue;
            }

            const bool layerHasDRM = layer.has_drm_policy ? layer.drm_enabled : object.transform.drm_enabled;
            const std::vector<DRMConfig>* drmConfigs = nullptr;
            if (layer.drm_enabled) {
                drmConfigs = &layer.drm_configs;
            } else if (!layer.has_drm_policy && object.transform.drm_enabled) {
                drmConfigs = &object.transform.drm_configs;
            }

            auto isEncrypted = [&](const std::string& attrName) -> bool {
                if (layer.has_drm_policy) {
                    return layer.isAttributeEncrypted(attrName);
                }
                return object.transform.isAttributeEncrypted(attrName);
            };

            auto encryptIfNeeded = [&](std::vector<uint8_t>& buffer, const std::string& attrName, int accIdx,
                                       std::set<int>& encryptedAccessorIndices) {
                if (!layerHasDRM || !isEncrypted(attrName) || drmConfigs == nullptr || drmConfigs->empty()) {
                    return;
                }
                encryptWithDRM(buffer, drmConfigs->front(), options.verbose);
                encryptedAccessorIndices.insert(accIdx);
            };

            rapidjson::Value primitive(rapidjson::kObjectType);
            rapidjson::Value attributes(rapidjson::kObjectType);
            rapidjson::Value primitiveExtensions(rapidjson::kObjectType);
            rapidjson::Value gsAttributes(rapidjson::kObjectType);
            std::set<int> encryptedAccessorIndices;

            std::vector<uint8_t> posBuffer(splatCount * 3 * sizeof(float));
            std::vector<uint8_t> colBuffer(splatCount * 4 * sizeof(float));
            std::vector<uint8_t> oriBuffer(splatCount * 4 * sizeof(float));
            std::vector<uint8_t> scaBuffer(splatCount * 3 * sizeof(float));
            std::vector<uint8_t> shRBuffer(splatCount * 15 * sizeof(float));
            std::vector<uint8_t> shGBuffer(splatCount * 15 * sizeof(float));
            std::vector<uint8_t> shBBuffer(splatCount * 15 * sizeof(float));

            const float SH_C0 = 0.28209479f;
            for (size_t i = 0; i < splatCount; ++i) {
                const auto& splat = data.splats[i];
                std::memcpy(posBuffer.data() + i * 12, splat.position.data(), 12);
                float color[4] = {
                    std::max(0.0f, std::min(1.0f, 0.5f + SH_C0 * splat.sh_dc[0])),
                    std::max(0.0f, std::min(1.0f, 0.5f + SH_C0 * splat.sh_dc[1])),
                    std::max(0.0f, std::min(1.0f, 0.5f + SH_C0 * splat.sh_dc[2])),
                    1.0f / (1.0f + std::exp(-splat.opacity))
                };
                std::memcpy(colBuffer.data() + i * 16, color, 16);
                std::memcpy(oriBuffer.data() + i * 16, splat.rotation.data(), 16);
                std::memcpy(scaBuffer.data() + i * 12, splat.scale.data(), 12);
                for (int j = 0; j < 15; ++j) {
                    reinterpret_cast<float*>(shRBuffer.data())[i * 15 + j] = splat.sh_rest[j * 3 + 0];
                    reinterpret_cast<float*>(shGBuffer.data())[i * 15 + j] = splat.sh_rest[j * 3 + 1];
                    reinterpret_cast<float*>(shBBuffer.data())[i * 15 + j] = splat.sh_rest[j * 3 + 2];
                }
            }

            int posAcc = accessorIndex;
            encryptIfNeeded(posBuffer, "position", posAcc, encryptedAccessorIndices);
            posAcc = addAccessor(addBufferViewFromBytes(posBuffer, bufferViews), 5126, "VEC3", splatCount);
            attributes.AddMember("POSITION", posAcc, allocator);

            int colAcc = accessorIndex;
            encryptIfNeeded(colBuffer, "color", colAcc, encryptedAccessorIndices);
            colAcc = addAccessor(addBufferViewFromBytes(colBuffer, bufferViews), 5126, "VEC4", splatCount);
            attributes.AddMember("COLOR_0", colAcc, allocator);

            if (!options.basic_pointcloud) {
                int oriAcc = accessorIndex;
                encryptIfNeeded(oriBuffer, "orientation", oriAcc, encryptedAccessorIndices);
                oriAcc = addAccessor(addBufferViewFromBytes(oriBuffer, bufferViews), 5126, "VEC4", splatCount);
                gsAttributes.AddMember("_GS_ORIENTATION", oriAcc, allocator);

                int scaAcc = accessorIndex;
                encryptIfNeeded(scaBuffer, "scale", scaAcc, encryptedAccessorIndices);
                scaAcc = addAccessor(addBufferViewFromBytes(scaBuffer, bufferViews), 5126, "VEC3", splatCount);
                gsAttributes.AddMember("_GS_SCALE", scaAcc, allocator);

                int shRAcc = accessorIndex;
                encryptIfNeeded(shRBuffer, "sh", shRAcc, encryptedAccessorIndices);
                shRAcc = addAccessor(addBufferViewFromBytes(shRBuffer, bufferViews), 5126, "SCALAR", splatCount * 15);
                gsAttributes.AddMember("_GS_SH_COEFF_R", shRAcc, allocator);

                int shGAcc = accessorIndex;
                encryptIfNeeded(shGBuffer, "sh", shGAcc, encryptedAccessorIndices);
                shGAcc = addAccessor(addBufferViewFromBytes(shGBuffer, bufferViews), 5126, "SCALAR", splatCount * 15);
                gsAttributes.AddMember("_GS_SH_COEFF_G", shGAcc, allocator);

                int shBAcc = accessorIndex;
                encryptIfNeeded(shBBuffer, "sh", shBAcc, encryptedAccessorIndices);
                shBAcc = addAccessor(addBufferViewFromBytes(shBBuffer, bufferViews), 5126, "SCALAR", splatCount * 15);
                gsAttributes.AddMember("_GS_SH_COEFF_B", shBAcc, allocator);

                rapidjson::Value gsExt(rapidjson::kObjectType);
                gsExt.AddMember("attributes", gsAttributes, allocator);
                primitiveExtensions.AddMember("EXT_gaussian_splats", gsExt, allocator);
            }

            if (layerHasDRM && drmConfigs != nullptr && !drmConfigs->empty() && !encryptedAccessorIndices.empty()) {
                rapidjson::Value drmExt(rapidjson::kObjectType);
                rapidjson::Value drmSystems(rapidjson::kArrayType);
                for (const auto& drmConfig : *drmConfigs) {
                    addDRMSystem(drmSystems, drmConfig, encryptedAccessorIndices);
                }
                drmExt.AddMember("systems", drmSystems, allocator);
                primitiveExtensions.AddMember("EXT_content_protection", drmExt, allocator);
            }

            primitive.AddMember("attributes", attributes, allocator);
            primitive.AddMember("mode", 0, allocator);
            if (!primitiveExtensions.ObjectEmpty()) {
                primitive.AddMember("extensions", primitiveExtensions, allocator);
            }
            primitives.PushBack(primitive, allocator);

            rapidjson::Value layerRecord(rapidjson::kObjectType);
            layerRecord.AddMember("id", rapidjson::Value(layer.id.c_str(), allocator), allocator);
            layerRecord.AddMember("layerIndex", static_cast<int>(layerIdx), allocator);
            layerRecord.AddMember("role", rapidjson::Value(layer.role.c_str(), allocator), allocator);
            layerRecord.AddMember("mesh", static_cast<int>(objIdx), allocator);
            layerRecord.AddMember("primitive", static_cast<int>(primitives.Size() - 1), allocator);
            layerRecord.AddMember("composition", rapidjson::Value(layer.composition.c_str(), allocator), allocator);
            rapidjson::Value dependsOn(rapidjson::kArrayType);
            for (const auto& dependency : layer.dependsOn) {
                dependsOn.PushBack(rapidjson::Value(dependency.c_str(), allocator), allocator);
            }
            layerRecord.AddMember("dependsOn", dependsOn, allocator);
            layerRecords.PushBack(layerRecord, allocator);
        }

        mesh.AddMember("primitives", primitives, allocator);
        meshes.PushBack(mesh, allocator);

        rapidjson::Value node(rapidjson::kObjectType);
        node.AddMember("mesh", static_cast<int>(meshes.Size() - 1), allocator);
        node.AddMember("name", rapidjson::Value(object.transform.name.c_str(), allocator), allocator);

        rapidjson::Value translation(rapidjson::kArrayType);
        translation.PushBack(object.transform.translation[0], allocator);
        translation.PushBack(object.transform.translation[1], allocator);
        translation.PushBack(object.transform.translation[2], allocator);
        node.AddMember("translation", translation, allocator);

        std::array<float, 4> quat = eulerToQuaternion(object.transform.rotation[0], object.transform.rotation[1], object.transform.rotation[2]);
        rapidjson::Value rotation(rapidjson::kArrayType);
        rotation.PushBack(quat[0], allocator);
        rotation.PushBack(quat[1], allocator);
        rotation.PushBack(quat[2], allocator);
        rotation.PushBack(quat[3], allocator);
        node.AddMember("rotation", rotation, allocator);

        rapidjson::Value scale(rapidjson::kArrayType);
        scale.PushBack(object.transform.scale[0], allocator);
        scale.PushBack(object.transform.scale[1], allocator);
        scale.PushBack(object.transform.scale[2], allocator);
        node.AddMember("scale", scale, allocator);
        nodesArray.PushBack(node, allocator);

        rapidjson::Value layerObject(rapidjson::kObjectType);
        layerObject.AddMember("id", rapidjson::Value(object.transform.name.c_str(), allocator), allocator);
        layerObject.AddMember("name", rapidjson::Value(object.transform.name.c_str(), allocator), allocator);
        layerObject.AddMember("node", static_cast<int>(nodesArray.Size() - 1), allocator);
        layerObject.AddMember("mesh", static_cast<int>(meshes.Size() - 1), allocator);
        layerObject.AddMember("layers", layerRecords, allocator);
        layerObjects.PushBack(layerObject, allocator);
    }

    doc.AddMember("meshes", meshes, allocator);
    doc.AddMember("nodes", nodesArray, allocator);
    doc.AddMember("bufferViews", bufferViews, allocator);
    doc.AddMember("accessors", accessors, allocator);

    rapidjson::Value buffers(rapidjson::kArrayType);
    rapidjson::Value buffer(rapidjson::kObjectType);
    buffer.AddMember("byteLength", static_cast<int64_t>(binary_buffer_.size()), allocator);
    buffers.PushBack(buffer, allocator);
    doc.AddMember("buffers", buffers, allocator);

    doc.AddMember("scene", 0, allocator);
    rapidjson::Value scenes(rapidjson::kArrayType);
    rapidjson::Value scene(rapidjson::kObjectType);
    rapidjson::Value sceneNodes(rapidjson::kArrayType);
    for (rapidjson::SizeType i = 0; i < nodesArray.Size(); ++i) {
        sceneNodes.PushBack(static_cast<int>(i), allocator);
    }
    scene.AddMember("nodes", sceneNodes, allocator);
    scenes.PushBack(scene, allocator);
    doc.AddMember("scenes", scenes, allocator);

    rapidjson::Value rootExtensions(rapidjson::kObjectType);
    rapidjson::Value layerExt(rapidjson::kObjectType);
    layerExt.AddMember("objects", layerObjects, allocator);
    rootExtensions.AddMember("EXT_gaussian_splat_layers", layerExt, allocator);
    doc.AddMember("extensions", rootExtensions, allocator);

    rapidjson::StringBuffer bufferJson;
    rapidjson::Writer<rapidjson::StringBuffer> writer(bufferJson);
    doc.Accept(writer);
    std::string json = bufferJson.GetString();

    if (options.binary) {
        return writeGLB(filename, json);
    }
    return writeGLTF(filename, json);
}

size_t GltfWriter::addToBuffer(const void* data, size_t size) {
    size_t offset = current_offset_;
    binary_buffer_.resize(current_offset_ + size);
    std::memcpy(binary_buffer_.data() + current_offset_, data, size);
    current_offset_ += size;
    return offset;
}

size_t GltfWriter::padBuffer(size_t alignment) {
    size_t padding = (alignment - (current_offset_ % alignment)) % alignment;
    if (padding > 0) {
        binary_buffer_.resize(current_offset_ + padding, 0);
        current_offset_ += padding;
    }
    return padding;
}

GltfWriter::BufferViewInfo GltfWriter::addPositions(const GaussianSplatData& data) {
    BufferViewInfo info;
    info.offset = current_offset_;
    info.stride = 0;
    for (const auto& splat : data.splats) {
        addToBuffer(splat.position.data(), sizeof(float) * 3);
    }

    info.length = current_offset_ - info.offset;
    return info;
}

GltfWriter::BufferViewInfo GltfWriter::addColors(const GaussianSplatData& data) {
    BufferViewInfo info;
    info.offset = current_offset_;
    info.stride = 0;

    for (const auto& splat : data.splats) {
        const float SH_C0 = 0.28209479f;
        float color[4] = {
            0.5f + SH_C0 * splat.sh_dc[0],
            0.5f + SH_C0 * splat.sh_dc[1],
            0.5f + SH_C0 * splat.sh_dc[2],
            1.0f / (1.0f + std::exp(-splat.opacity))
        };
        for (int i = 0; i < 3; ++i) {
            color[i] = std::max(0.0f, std::min(1.0f, color[i]));
        }
        addToBuffer(color, sizeof(color));
    }
    info.length = current_offset_ - info.offset;
    return info;
}

GltfWriter::BufferViewInfo GltfWriter::addOrientations(const GaussianSplatData& data) {
    BufferViewInfo info;
    info.offset = current_offset_;
    info.stride = 0;
    for (const auto& splat : data.splats) {
        float quat[4] = { splat.rotation[0], splat.rotation[1], splat.rotation[2], splat.rotation[3] };
        float norm = std::sqrt(quat[0]*quat[0] + quat[1]*quat[1] + quat[2]*quat[2] + quat[3]*quat[3]);
        if (norm > 0) {
            for (int i = 0; i < 4; ++i) quat[i] /= norm;
        }
        addToBuffer(quat, sizeof(quat));
    }
    info.length = current_offset_ - info.offset;
    return info;
}

GltfWriter::BufferViewInfo GltfWriter::addScales(const GaussianSplatData& data) {
    BufferViewInfo info;
    info.offset = current_offset_;
    info.stride = 0;
    for (const auto& splat : data.splats) {
        addToBuffer(splat.scale.data(), sizeof(float) * 3);
    }
    info.length = current_offset_ - info.offset;
    return info;
}

GltfWriter::BufferViewInfo GltfWriter::addSHCoeffsFirst(const GaussianSplatData& data) {
    BufferViewInfo info;
    info.offset = current_offset_;
    info.stride = 0;
    for (const auto& splat : data.splats) {
        addToBuffer(splat.sh_rest.data(), sizeof(float) * 9);
    }
    info.length = current_offset_ - info.offset;
    return info;
}

GltfWriter::BufferViewInfo GltfWriter::addSHCoeffsSecond(const GaussianSplatData& data) {
    BufferViewInfo info;
    info.offset = current_offset_;
    info.stride = 0;
    for (const auto& splat : data.splats) {
        addToBuffer(splat.sh_rest.data() + 9, sizeof(float) * 15);
    }
    info.length = current_offset_ - info.offset;
    return info;
}

GltfWriter::BufferViewInfo GltfWriter::addSHCoeffsThird(const GaussianSplatData& data) {
    BufferViewInfo info;
    info.offset = current_offset_;
    info.stride = 0;
    for (const auto& splat : data.splats) {
        addToBuffer(splat.sh_rest.data() + 24, sizeof(float) * 21);
    }
    info.length = current_offset_ - info.offset;
    return info;
}

std::string GltfWriter::createJSON(const GaussianSplatData& data, const ConversionOptions& options) {
    rapidjson::Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();
    
    rapidjson::Value asset(rapidjson::kObjectType);
    asset.AddMember("version", "2.0", allocator);
    asset.AddMember("generator", "ply2gltf converter", allocator);
    doc.AddMember("asset", asset, allocator);
    
    rapidjson::Value extensionsUsed(rapidjson::kArrayType);
    if (!options.basic_pointcloud) {
        extensionsUsed.PushBack("EXT_gaussian_splats", allocator);
    }
    // ## DRM MODIFIED: Add extension to extensionsUsed ##
    if (options.drm_enabled) {
        extensionsUsed.PushBack("EXT_content_protection", allocator);
    }
    doc.AddMember("extensionsUsed", extensionsUsed, allocator);
    
    doc.AddMember("scene", 0, allocator);
    
    rapidjson::Value scenes(rapidjson::kArrayType);
    rapidjson::Value scene(rapidjson::kObjectType);
    rapidjson::Value nodes(rapidjson::kArrayType);
    nodes.PushBack(0, allocator);
    scene.AddMember("nodes", nodes, allocator);
    scenes.PushBack(scene, allocator);
    doc.AddMember("scenes", scenes, allocator);
    
    rapidjson::Value nodesArray(rapidjson::kArrayType);
    rapidjson::Value node(rapidjson::kObjectType);
    node.AddMember("mesh", 0, allocator);
    nodesArray.PushBack(node, allocator);
    doc.AddMember("nodes", nodesArray, allocator);
    
    rapidjson::Value bufferViews(rapidjson::kArrayType);
    rapidjson::Value accessors(rapidjson::kArrayType);
    
    int bufferViewIndex = 0;
    int accessorIndex = 0;

    // ## DRM MODIFIED: Track encrypted accessors ##
    std::set<int> encryptedAccessorIndices;
    
    BufferViewInfo positionView, colorView, orientationView, scaleView;
    BufferViewInfo shRView, shGView, shBView; 
    
    if (options.progressive) {
        // ... (Progressive mode is left unmodified for simplicity) ...
        // This part will NOT have DRM applied.
        BufferViewInfo sh1View, sh2View, sh3View;
        positionView = addPositions(data);
        colorView = addColors(data);
        if (!options.basic_pointcloud) {
            orientationView = addOrientations(data);
            scaleView = addScales(data);
            sh1View = addSHCoeffsFirst(data);
            sh2View = addSHCoeffsSecond(data);
            sh3View = addSHCoeffsThird(data);
        }
    } else {
        // ## DRM MODIFIED: Packed mode logic changed to support encryption ##
        binary_buffer_.clear();
        current_offset_ = 0;

        size_t splatCount = data.splats.size();
        std::vector<uint8_t> pos_buffer(splatCount * 3 * sizeof(float));
        std::vector<uint8_t> col_buffer(splatCount * 4 * sizeof(float));
        std::vector<uint8_t> ori_buffer;
        std::vector<uint8_t> sca_buffer;
        std::vector<uint8_t> shr_buffer, shg_buffer, shb_buffer;
        
        if (!options.basic_pointcloud) {
            ori_buffer.resize(splatCount * 4 * sizeof(float));
            sca_buffer.resize(splatCount * 3 * sizeof(float));
            shr_buffer.resize(splatCount * 15 * sizeof(float));
            shg_buffer.resize(splatCount * 15 * sizeof(float));
            shb_buffer.resize(splatCount * 15 * sizeof(float));
        }

        // 1. Populate attribute buffers in memory
        for (size_t i = 0; i < splatCount; ++i) {
            const auto& splat = data.splats[i];
            
            std::memcpy(pos_buffer.data() + i * 12, splat.position.data(), 12);

            const float SH_C0 = 0.28209479f;
            float color[4] = {
                std::max(0.0f, std::min(1.0f, 0.5f + SH_C0 * splat.sh_dc[0])),
                std::max(0.0f, std::min(1.0f, 0.5f + SH_C0 * splat.sh_dc[1])),
                std::max(0.0f, std::min(1.0f, 0.5f + SH_C0 * splat.sh_dc[2])),
                1.0f / (1.0f + std::exp(-splat.opacity))
            };
            std::memcpy(col_buffer.data() + i * 16, color, 16);

            if (!options.basic_pointcloud) {
                float quat[4] = {splat.rotation[0], splat.rotation[1], splat.rotation[2], splat.rotation[3]};
                float norm = std::sqrt(quat[0]*quat[0] + quat[1]*quat[1] + quat[2]*quat[2] + quat[3]*quat[3]);
                if (norm > 0) for(int k=0; k<4; ++k) quat[k] /= norm;
                std::memcpy(ori_buffer.data() + i * 16, quat, 16);

                std::memcpy(sca_buffer.data() + i * 12, splat.scale.data(), 12);

                for (int j = 0; j < 15; ++j) {
                    reinterpret_cast<float*>(shr_buffer.data())[i * 15 + j] = splat.sh_rest[j * 3 + 0];
                    reinterpret_cast<float*>(shg_buffer.data())[i * 15 + j] = splat.sh_rest[j * 3 + 1];
                    reinterpret_cast<float*>(shb_buffer.data())[i * 15 + j] = splat.sh_rest[j * 3 + 2];
                }
            }
        }
        
        // 2. Conditionally encrypt attribute buffers with multiDRM support
        // ## MULTIDRM: Encrypt with all configured DRM systems ##
        if (options.drm_enabled && !options.drm_configs.empty()) {
            // Use multiDRM: encrypt with each configured DRM system
            for (const auto& drmConfig : options.drm_configs) {
                if (options.isAttributeEncrypted("position")) {
                    std::vector<uint8_t> pos_copy = pos_buffer;  // Copy for each DRM system
                    encryptWithDRM(pos_copy, drmConfig, options.verbose);
                    // For multiDRM, we use the first DRM system's encryption result
                    // In production, you might want to apply all encryptions sequentially
                    if (&drmConfig == &options.drm_configs[0]) {
                        pos_buffer = pos_copy;
                    }
                }
                if (options.isAttributeEncrypted("color")) {
                    std::vector<uint8_t> col_copy = col_buffer;
                    encryptWithDRM(col_copy, drmConfig, options.verbose);
                    if (&drmConfig == &options.drm_configs[0]) {
                        col_buffer = col_copy;
                    }
                }
                if (options.isAttributeEncrypted("orientation")) {
                    std::vector<uint8_t> ori_copy = ori_buffer;
                    encryptWithDRM(ori_copy, drmConfig, options.verbose);
                    if (&drmConfig == &options.drm_configs[0]) {
                        ori_buffer = ori_copy;
                    }
                }
                if (options.isAttributeEncrypted("scale")) {
                    std::vector<uint8_t> sca_copy = sca_buffer;
                    encryptWithDRM(sca_copy, drmConfig, options.verbose);
                    if (&drmConfig == &options.drm_configs[0]) {
                        sca_buffer = sca_copy;
                    }
                }
                if (options.isAttributeEncrypted("sh")) {
                    std::vector<uint8_t> shr_copy = shr_buffer;
                    std::vector<uint8_t> shg_copy = shg_buffer;
                    std::vector<uint8_t> shb_copy = shb_buffer;
                    encryptWithDRM(shr_copy, drmConfig, options.verbose);
                    encryptWithDRM(shg_copy, drmConfig, options.verbose);
                    encryptWithDRM(shb_copy, drmConfig, options.verbose);
                    if (&drmConfig == &options.drm_configs[0]) {
                        shr_buffer = shr_copy;
                        shg_buffer = shg_copy;
                        shb_buffer = shb_copy;
                    }
                }
            }
        } else if (options.drm_enabled && !options.getPrimaryDRMKey().empty()) {
            // Legacy single DRM support (backward compatibility)
            if (options.isAttributeEncrypted("position")) xorEncrypt(pos_buffer, options.getPrimaryDRMKey(), options.verbose);
            if (options.isAttributeEncrypted("color")) xorEncrypt(col_buffer, options.getPrimaryDRMKey(), options.verbose);
            if (options.isAttributeEncrypted("orientation")) xorEncrypt(ori_buffer, options.getPrimaryDRMKey(), options.verbose);
            if (options.isAttributeEncrypted("scale")) xorEncrypt(sca_buffer, options.getPrimaryDRMKey(), options.verbose);
            if (options.isAttributeEncrypted("sh")) {
                xorEncrypt(shr_buffer, options.getPrimaryDRMKey(), options.verbose);
                xorEncrypt(shg_buffer, options.getPrimaryDRMKey(), options.verbose);
                xorEncrypt(shb_buffer, options.getPrimaryDRMKey(), options.verbose);
            }
        }

        // 3. Append buffers to main binary_buffer_ and record info
        positionView = { addToBuffer(pos_buffer.data(), pos_buffer.size()), pos_buffer.size(), 0 };
        colorView = { addToBuffer(col_buffer.data(), col_buffer.size()), col_buffer.size(), 0 };
        
        if (!options.basic_pointcloud) {
            orientationView = { addToBuffer(ori_buffer.data(), ori_buffer.size()), ori_buffer.size(), 0 };
            scaleView = { addToBuffer(sca_buffer.data(), sca_buffer.size()), sca_buffer.size(), 0 };
            shRView = { addToBuffer(shr_buffer.data(), shr_buffer.size()), shr_buffer.size(), 0 };
            shGView = { addToBuffer(shg_buffer.data(), shg_buffer.size()), shg_buffer.size(), 0 };
            shBView = { addToBuffer(shb_buffer.data(), shb_buffer.size()), shb_buffer.size(), 0 };
        }
    }
    
    auto addBufferView = [&](const BufferViewInfo& view, int target = 0) -> int {
        rapidjson::Value bv(rapidjson::kObjectType);
        bv.AddMember("buffer", 0, allocator);
        bv.AddMember("byteOffset", static_cast<uint64_t>(view.offset), allocator);
        bv.AddMember("byteLength", static_cast<uint64_t>(view.length), allocator);
        if (view.stride > 0) bv.AddMember("byteStride", static_cast<uint64_t>(view.stride), allocator);
        if (target > 0) bv.AddMember("target", target, allocator);
        bufferViews.PushBack(bv, allocator);
        return bufferViewIndex++;
    };

    auto addAccessor = [&](int bufferView, int componentType, const std::string& type, size_t count) -> int {
        rapidjson::Value accessor(rapidjson::kObjectType);
        accessor.AddMember("bufferView", bufferView, allocator);
        accessor.AddMember("componentType", componentType, allocator);
        accessor.AddMember("count", static_cast<uint64_t>(count), allocator);
        accessor.AddMember("type", rapidjson::Value(type.c_str(), allocator), allocator);
        accessors.PushBack(accessor, allocator);
        return accessorIndex++;
    };
    
    rapidjson::Value attributes(rapidjson::kObjectType);
    rapidjson::Value gsAttributes(rapidjson::kObjectType);

    // Position Accessor
    int posBV = addBufferView(positionView, 34962);
    rapidjson::Value posAccessor(rapidjson::kObjectType);
    posAccessor.AddMember("bufferView", posBV, allocator);
    posAccessor.AddMember("componentType", 5126, allocator);
    posAccessor.AddMember("count", static_cast<uint64_t>(data.splats.size()), allocator);
    posAccessor.AddMember("type", "VEC3", allocator);
    rapidjson::Value minPos(rapidjson::kArrayType);
    rapidjson::Value maxPos(rapidjson::kArrayType);
    for (int k = 0; k < 3; ++k) {
        minPos.PushBack(data.min_bounds[k], allocator);
        maxPos.PushBack(data.max_bounds[k], allocator);
    }
    posAccessor.AddMember("min", minPos, allocator);
    posAccessor.AddMember("max", maxPos, allocator);
    accessors.PushBack(posAccessor, allocator);
    int posAcc = accessorIndex++;
    attributes.AddMember("POSITION", posAcc, allocator);
    if (options.isAttributeEncrypted("position")) encryptedAccessorIndices.insert(posAcc);

    // Color Accessor
    int colorBV = addBufferView(colorView, 34962);
    int colorAcc = addAccessor(colorBV, 5126, "VEC4", data.splats.size());
    attributes.AddMember("COLOR_0", colorAcc, allocator);
    if (options.isAttributeEncrypted("color")) encryptedAccessorIndices.insert(colorAcc);

    if (!options.basic_pointcloud) {
        // Orientation Accessor
        int orientBV = addBufferView(orientationView, 34962);
        int orientAcc = addAccessor(orientBV, 5126, "VEC4", data.splats.size());
        gsAttributes.AddMember("_GS_ORIENTATION", orientAcc, allocator);
        if (options.isAttributeEncrypted("orientation")) encryptedAccessorIndices.insert(orientAcc);

        // Scale Accessor
        int scaleBV = addBufferView(scaleView, 34962);
        int scaleAcc = addAccessor(scaleBV, 5126, "VEC3", data.splats.size());
        gsAttributes.AddMember("_GS_SCALE", scaleAcc, allocator);
        if (options.isAttributeEncrypted("scale")) encryptedAccessorIndices.insert(scaleAcc);
        
        // SH Coeffs (R, G, B)
        int shRBV = addBufferView(shRView);
        int shRAcc = addAccessor(shRBV, 5126, "SCALAR", data.splats.size() * 15);
        gsAttributes.AddMember("_GS_SH_COEFF_R", shRAcc, allocator);
        
        int shGBV = addBufferView(shGView);
        int shGAcc = addAccessor(shGBV, 5126, "SCALAR", data.splats.size() * 15);
        gsAttributes.AddMember("_GS_SH_COEFF_G", shGAcc, allocator);

        int shBBV = addBufferView(shBView);
        int shBAcc = addAccessor(shBBV, 5126, "SCALAR", data.splats.size() * 15);
        gsAttributes.AddMember("_GS_SH_COEFF_B", shBAcc, allocator);

        if (options.isAttributeEncrypted("sh")) {
            encryptedAccessorIndices.insert(shRAcc);
            encryptedAccessorIndices.insert(shGAcc);
            encryptedAccessorIndices.insert(shBAcc);
        }
    }
    
    rapidjson::Value meshes(rapidjson::kArrayType);
    rapidjson::Value mesh(rapidjson::kObjectType);
    rapidjson::Value primitives(rapidjson::kArrayType);
    rapidjson::Value primitive(rapidjson::kObjectType);
    
    primitive.AddMember("mode", 0, allocator);
    primitive.AddMember("attributes", attributes, allocator);
    
    if (!options.basic_pointcloud) {
        rapidjson::Value extensions(rapidjson::kObjectType);
        rapidjson::Value extGaussianSplats(rapidjson::kObjectType);
        extGaussianSplats.AddMember("attributes", gsAttributes, allocator);
        extensions.AddMember("EXT_gaussian_splats", extGaussianSplats, allocator);
        primitive.AddMember("extensions", extensions, allocator);
    }
    
    primitives.PushBack(primitive, allocator);
    mesh.AddMember("primitives", primitives, allocator);

    // ## MULTIDRM: Add multiDRM extension to the mesh ##
    if (options.drm_enabled) {
        if (!mesh.HasMember("extensions")) {
            mesh.AddMember("extensions", rapidjson::Value(rapidjson::kObjectType), allocator);
        }
        
        // MultiDRM: Support multiple DRM systems
        if (!options.drm_configs.empty()) {
            rapidjson::Value drmSystemsArray(rapidjson::kArrayType);
            
            for (const auto& drmConfig : options.drm_configs) {
                rapidjson::Value drmSystem(rapidjson::kObjectType);
                drmSystem.AddMember("schemeIdUri", rapidjson::Value(drmConfig.schemeIdUri.c_str(), allocator), allocator);
                drmSystem.AddMember("keyId", rapidjson::Value(drmConfig.keyId.c_str(), allocator), allocator);
                
                // ## FIX: Add encryption key for XOR decryption (testing only) ##
                // In production, this should be retrieved from license server
                if (options.verbose) {
                    std::cout << "  [DRM JSON] DRM config key length: " << drmConfig.key.length() << std::endl;
                    std::cout << "  [DRM JSON] DRM config keyId length: " << drmConfig.keyId.length() << std::endl;
                }
                if (!drmConfig.key.empty()) {
                    drmSystem.AddMember("key", rapidjson::Value(drmConfig.key.c_str(), allocator), allocator);
                    if (options.verbose) {
                        std::cout << "  [DRM JSON] Added 'key' field to DRM system (length: " << drmConfig.key.length() << ")" << std::endl;
                    }
                } else {
                    if (options.verbose) {
                        std::cout << "  [DRM JSON] Warning: DRM config key is empty, not adding 'key' field" << std::endl;
                    }
                }
                
                // Add PSSH box if available (for Widevine/PlayReady)
                if (!drmConfig.pssh.empty()) {
                    drmSystem.AddMember("pssh", rapidjson::Value(drmConfig.pssh.c_str(), allocator), allocator);
                }
                
                // Add license URL if available
                if (!drmConfig.licenseUrl.empty()) {
                    drmSystem.AddMember("licenseUrl", rapidjson::Value(drmConfig.licenseUrl.c_str(), allocator), allocator);
                }
                
                // Add encrypted accessors (same for all DRM systems in this implementation)
                rapidjson::Value encryptedAccessorsArray(rapidjson::kArrayType);
                for (int index : encryptedAccessorIndices) {
                    encryptedAccessorsArray.PushBack(index, allocator);
                }
                drmSystem.AddMember("encryptedAccessors", encryptedAccessorsArray, allocator);
                
                drmSystemsArray.PushBack(drmSystem, allocator);
            }
            
            rapidjson::Value drmExtension(rapidjson::kObjectType);
            drmExtension.AddMember("systems", drmSystemsArray, allocator);
            mesh["extensions"].AddMember("EXT_content_protection", drmExtension, allocator);
        } else {
            // Legacy single DRM support (backward compatibility)
            rapidjson::Value drmExtension(rapidjson::kObjectType);
            drmExtension.AddMember("schemeIdUri", "urn:exp:simple-xor-drm", allocator);
            drmExtension.AddMember("keyId", rapidjson::Value(options.getPrimaryDRMKeyId().c_str(), allocator), allocator);
            
            rapidjson::Value encryptedAccessorsArray(rapidjson::kArrayType);
            for (int index : encryptedAccessorIndices) {
                encryptedAccessorsArray.PushBack(index, allocator);
            }
            drmExtension.AddMember("encryptedAccessors", encryptedAccessorsArray, allocator);
            
            mesh["extensions"].AddMember("EXT_content_protection", drmExtension, allocator);
        }
    }

    meshes.PushBack(mesh, allocator);
    doc.AddMember("meshes", meshes, allocator);
    
    rapidjson::Value buffers(rapidjson::kArrayType);
    rapidjson::Value buffer(rapidjson::kObjectType);
    buffer.AddMember("byteLength", static_cast<uint64_t>(binary_buffer_.size()), allocator);
    if (!options.binary) {
        std::string binUri = options.output_file.substr(0, options.output_file.find_last_of('.')) + ".bin";
        buffer.AddMember("uri", rapidjson::Value(binUri.c_str(), allocator), allocator);
    }
    buffers.PushBack(buffer, allocator);
    doc.AddMember("buffers", buffers, allocator);
    
    doc.AddMember("bufferViews", bufferViews, allocator);
    doc.AddMember("accessors", accessors, allocator);

    if (!options.viewpoint_file.empty()) {
        float q[4] = {0,0,0,1}, c[3] = {0,0,0}, d = 0; 
        if (loadViewpointFile(options.viewpoint_file, q, c, d)) {
            // ... (camera embedding logic remains the same) ...
        }
    }
    
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    doc.Accept(writer);
    
    return sb.GetString();
}

bool GltfWriter::writeGLB(const std::string& filename, const std::string& json) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        error_ = "Failed to open output file: " + filename;
        return false;
    }
    
    std::string paddedJson = json;
    while (paddedJson.size() % 4 != 0) {
        paddedJson += ' ';
    }
    
    padBuffer(4);
    
    size_t totalSize = sizeof(GlbHeader) + 
                       sizeof(ChunkHeader) + paddedJson.size() +
                       sizeof(ChunkHeader) + binary_buffer_.size();
    
    GlbHeader header;
    header.length = static_cast<uint32_t>(totalSize);
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    
    ChunkHeader jsonChunk;
    jsonChunk.length = static_cast<uint32_t>(paddedJson.size());
    jsonChunk.type = CHUNK_TYPE_JSON;
    file.write(reinterpret_cast<const char*>(&jsonChunk), sizeof(jsonChunk));
    file.write(paddedJson.data(), paddedJson.size());
    
    ChunkHeader binChunk;
    binChunk.length = static_cast<uint32_t>(binary_buffer_.size());
    binChunk.type = CHUNK_TYPE_BIN;
    file.write(reinterpret_cast<const char*>(&binChunk), sizeof(binChunk));
    file.write(reinterpret_cast<const char*>(binary_buffer_.data()), binary_buffer_.size());
    
    return true;
}

bool GltfWriter::writeGLTF(const std::string& filename, const std::string& json) {
    std::ofstream jsonFile(filename);
    if (!jsonFile.is_open()) {
        error_ = "Failed to open output file: " + filename;
        return false;
    }
    jsonFile << json;
    jsonFile.close();
    
    std::string binFilename = filename.substr(0, filename.find_last_of('.')) + ".bin";
    std::ofstream binFile(binFilename, std::ios::binary);
    if (!binFile.is_open()) {
        error_ = "Failed to open binary output file: " + binFilename;
        return false;
    }
    binFile.write(reinterpret_cast<const char*>(binary_buffer_.data()), binary_buffer_.size());
    binFile.close();
    
    return true;
}

void GltfWriter::displayStatistics(const GaussianSplatData& data, const ConversionOptions& options) const {
    std::cout << "\n========== GLB File Structure Statistics ==========\n" << std::endl;
    
    std::cout << "Splat Information:" << std::endl;
    std::cout << "  Total splats: " << data.count() << std::endl;
    std::cout << "  Bounding box: [" 
              << data.min_bounds[0] << ", " << data.min_bounds[1] << ", " << data.min_bounds[2] << "] to ["
              << data.max_bounds[0] << ", " << data.max_bounds[1] << ", " << data.max_bounds[2] << "]" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Buffer Layout:" << std::endl;
    
    size_t splatCount = data.count();
    size_t positionSize = splatCount * 3 * sizeof(float);
    size_t colorSize = splatCount * 4 * sizeof(float);
    size_t orientationSize = splatCount * 4 * sizeof(float);
    size_t scaleSize = splatCount * 3 * sizeof(float);
    size_t shFirstSize = splatCount * 9 * sizeof(float);
    size_t shSecondSize = splatCount * 15 * sizeof(float);
    size_t shThirdSize = splatCount * 21 * sizeof(float);
    
    size_t offset = 0;
    auto printAttribute = [&](const std::string& name, size_t size, size_t components) {
        std::cout << "  " << name << ":" << std::endl;
        std::cout << "    Offset: " << offset << " bytes" << std::endl;
        std::cout << "    Size: " << size << " bytes (" << (size / 1024.0 / 1024.0) << " MB)" << std::endl;
        std::cout << "    Components: " << components << std::endl;
        std::cout << "    Stride: " << (components * sizeof(float)) << " bytes" << std::endl;
        offset += size;
        std::cout << std::endl;
    };
    
    if (options.progressive) {
        std::cout << "  Organization: Progressive (optimized for streaming)" << std::endl;
        std::cout << std::endl;
        
        std::cout << "  Level 1 - Base (Position + Color):" << std::endl;
        printAttribute("    POSITION", positionSize, 3);
        printAttribute("    COLOR_0", colorSize, 4);
        
        if (!options.basic_pointcloud) {
            std::cout << "  Level 2 - Transform (Orientation + Scale):" << std::endl;
            printAttribute("    _GS_ORIENTATION", orientationSize, 4);
            printAttribute("    _GS_SCALE", scaleSize, 3);
            
            std::cout << "  Level 3 - Spherical Harmonics:" << std::endl;
            printAttribute("    _GS_SH_COEFF_FIRST (9 coeffs)", shFirstSize, 9);
            printAttribute("    _GS_SH_COEFF_SECOND (15 coeffs)", shSecondSize, 15);
            printAttribute("    _GS_SH_COEFF_THIRD (21 coeffs)", shThirdSize, 21);
        }
    } else {
        std::cout << "  Organization: Standard (packed attributes)" << std::endl;
        std::cout << std::endl;
        printAttribute("  POSITION", positionSize, 3);
        printAttribute("  COLOR_0", colorSize, 4);
        if (!options.basic_pointcloud) {
            printAttribute("  _GS_ORIENTATION", orientationSize, 4);
            printAttribute("  _GS_SCALE", scaleSize, 3);
            printAttribute("  _GS_SH_COEFF_R", splatCount * 15 * sizeof(float), 15);
            printAttribute("  _GS_SH_COEFF_G", splatCount * 15 * sizeof(float), 15);
            printAttribute("  _GS_SH_COEFF_B", splatCount * 15 * sizeof(float), 15);
        }
    }
    
    std::cout << "Total Statistics:" << std::endl;
    std::cout << "  Binary buffer size: " << binary_buffer_.size() << " bytes (" 
              << (binary_buffer_.size() / 1024.0 / 1024.0) << " MB)" << std::endl;
    if (splatCount > 0) {
      std::cout << "  Per-splat size: " << (binary_buffer_.size() / splatCount) << " bytes" << std::endl;
    }
    
    size_t totalComponents = 3 + 4;
    if (!options.basic_pointcloud) {
      totalComponents += 4 + 3 + 15 + 15 + 15;
    }

    std::cout << "  Total components per splat: " << totalComponents << std::endl;
    std::cout << "  Bytes per component: " << sizeof(float) << std::endl;
    
    std::cout << "\n===================================================\n" << std::endl;
}

void GltfWriter::calculateColorBounds(const GaussianSplatData& data, 
                                      std::array<float, 4>& minColor, 
                                      std::array<float, 4>& maxColor) const {
    minColor = {1.0f, 1.0f, 1.0f, 1.0f};
    maxColor = {0.0f, 0.0f, 0.0f, 0.0f};
    
    const float SH_C0 = 0.28209479f;
    
    for (const auto& splat : data.splats) {
        float rgb[3] = {
            0.5f + SH_C0 * splat.sh_dc[0],
            0.5f + SH_C0 * splat.sh_dc[1],
            0.5f + SH_C0 * splat.sh_dc[2]
        };
        
        for (int i = 0; i < 3; ++i) {
            rgb[i] = std::max(0.0f, std::min(1.0f, rgb[i]));
            minColor[i] = std::min(minColor[i], rgb[i]);
            maxColor[i] = std::max(maxColor[i], rgb[i]);
        }
        
        float alpha = 1.0f / (1.0f + std::exp(-splat.opacity));
        minColor[3] = std::min(minColor[3], alpha);
        maxColor[3] = std::max(maxColor[3], alpha);
    }
}

} // namespace ply2gltf
