#include "MPEGGltfWriter.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include "ViewpointUtil.h"
#include <sstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ply2gltf {

MPEGGltfWriter::MPEGGltfWriter() : currentBufferIndex_(0), 
                                 currentBufferViewIndex_(0), 
                                 currentAccessorIndex_(0) {
}

MPEGGltfWriter::~MPEGGltfWriter() = default;

bool MPEGGltfWriter::writeSequence(const std::string& filename,
                                 const MultiFrameGaussianSplatData& data,
                                 const SequenceConversionOptions& options) {
    if (data.frames.empty()) {
        error_ = "No frame data to write";
        return false;
    }
    
    // Clear previous data
    binary_buffer_.clear();
    positionBuffer_.clear();
    colorBuffer_.clear();
    orientationBuffer_.clear();
    scaleBuffer_.clear();
    shFirstBuffer_.clear();
    shSecondBuffer_.clear();
    shThirdBuffer_.clear();
    headerBuffer_.clear();
    staticPositionBuffer_.clear();
    staticColorBuffer_.clear();
    staticOrientationBuffer_.clear();
    staticScaleBuffer_.clear();
    staticShFirstBuffer_.clear();
    staticShSecondBuffer_.clear();
    staticShThirdBuffer_.clear();
    
    // Reset indices
    currentBufferIndex_ = 0;
    currentBufferViewIndex_ = 0;
    currentAccessorIndex_ = 0;
    
    // Write attribute buffers
    writeAttributeBuffers(data, options);
    
    // Combine all buffers
    combineBuffers();
    
    // Display statistics if requested
    if (options.stats) {
        displayStatistics(data, options);
    }
    
    // Create JSON
    std::string json = createSequenceJSON(data, options);
    
    // Write output
    if (options.binary) {
        return writeGLB(filename, json);
    } else {
        return writeGLTF(filename, json);
    }
}

// Helper to convert seconds to 64-bit timestamp (sec<<32 | frac)
static uint64_t toTimestamp64(float seconds) {
    uint32_t sec = static_cast<uint32_t>(seconds);
    double fracd = (seconds - static_cast<double>(sec)) * 4294967296.0; // 2^32
    if (fracd < 0) fracd = 0; // clamp
    uint32_t frac = static_cast<uint32_t>(fracd);
    return (static_cast<uint64_t>(sec) << 32) | frac;
}

std::string MPEGGltfWriter::createJSON(const GaussianSplatData& data, 
                                     const ConversionOptions& options) {
    // This shouldn't be called for sequences
    return GltfWriter::createJSON(data, options);
}

std::string MPEGGltfWriter::createSequenceJSON(const MultiFrameGaussianSplatData& data,
                                             const SequenceConversionOptions& options) {
    rapidjson::Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();
    
    // Asset
    rapidjson::Value asset(rapidjson::kObjectType);
    asset.AddMember("version", "2.0", allocator);
    asset.AddMember("generator", "ply2gltf MPEG sequence converter", allocator);
    doc.AddMember("asset", asset, allocator);
    
    // Extensions used
    rapidjson::Value extensionsUsed(rapidjson::kArrayType);
    extensionsUsed.PushBack("EXT_gaussian_splats", allocator);
    
    // Only add MPEG extensions if we have multiple frames
    if (data.frameCount > 1) {
        extensionsUsed.PushBack("MPEG_media", allocator);
        extensionsUsed.PushBack("MPEG_buffer_circular", allocator);
        extensionsUsed.PushBack("MPEG_accessor_timed", allocator);
        if (options.progressive) {
            extensionsUsed.PushBack("MPEG_scene_dynamic", allocator);
        }
    }
    doc.AddMember("extensionsUsed", extensionsUsed, allocator);
    
    // Add MPEG extensions only for multi-frame sequences
    if (data.frameCount > 1) {
        if (!doc.HasMember("extensions")) {
            doc.AddMember("extensions", rapidjson::Value(rapidjson::kObjectType), allocator);
        }
        addMPEGMediaExtension(doc, allocator, data);
        if (options.progressive) {
            addMPEGSceneDynamicExtension(doc, allocator);
        }
    }
    
    // Add circular buffers
    addCircularBuffers(doc, allocator, data, options.progressive);
    
    // Add buffer views
    addCircularBufferViews(doc, allocator, data, options);
    
    // Add accessors
    addTimedAccessors(doc, allocator, data, options.progressive);
    
    // Create scene structure
    rapidjson::Value scenes(rapidjson::kArrayType);
    rapidjson::Value scene(rapidjson::kObjectType);
    rapidjson::Value nodes(rapidjson::kArrayType);
    nodes.PushBack(0, allocator);
    scene.AddMember("nodes", nodes, allocator);
    scenes.PushBack(scene, allocator);
    doc.AddMember("scenes", scenes, allocator);
    doc.AddMember("scene", 0, allocator);
    
    // Add node
    rapidjson::Value nodesArray(rapidjson::kArrayType);
    rapidjson::Value node(rapidjson::kObjectType);
    node.AddMember("mesh", 0, allocator);
    node.AddMember("name", "GaussianSplatSequence", allocator);
    nodesArray.PushBack(node, allocator);
    doc.AddMember("nodes", nodesArray, allocator);
    
    // Add mesh with Gaussian splat primitive
    addGaussianSplatPrimitive(doc, allocator, data, options.progressive);
    
    // Update buffer size with actual size after all data is written
    if (doc.HasMember("buffers") && doc["buffers"].IsArray() && !doc["buffers"].Empty()) {
        doc["buffers"][0]["byteLength"] = static_cast<uint64_t>(binary_buffer_.size());
    }
    
    // Convert to string
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    
    return buffer.GetString();
}

std::string MPEGGltfWriter::createSequenceJSONChunked(const MultiFrameGaussianSplatData& data,
                                                      const SequenceConversionOptions& options) {
    rapidjson::Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();

    // Asset
    rapidjson::Value asset(rapidjson::kObjectType);
    asset.AddMember("version", "2.0", allocator);
    asset.AddMember("generator", "ply2gltf MPEG sequence converter (chunked)", allocator);
    doc.AddMember("asset", asset, allocator);

    // extensionsUsed
    rapidjson::Value extensionsUsed(rapidjson::kArrayType);
    extensionsUsed.PushBack("EXT_gaussian_splats", allocator);
    if (data.frameCount > 1) {
        extensionsUsed.PushBack("MPEG_media", allocator);
        extensionsUsed.PushBack("MPEG_buffer_circular", allocator);
        extensionsUsed.PushBack("MPEG_accessor_timed", allocator);
        if (options.progressive) {
            extensionsUsed.PushBack("MPEG_scene_dynamic", allocator);
        }
    }
    // ## DRM MODIFIED ##
    if (options.drm_enabled) {
        extensionsUsed.PushBack("EXT_content_protection", allocator);
    }
    doc.AddMember("extensionsUsed", extensionsUsed, allocator);

    // Global extensions: MPEG_media
    if (data.frameCount > 1) {
        if (!doc.HasMember("extensions")) {
            doc.AddMember("extensions", rapidjson::Value(rapidjson::kObjectType), allocator);
        }
        addMPEGMediaExtension(doc, allocator, data);
        if (options.progressive) {
            addMPEGSceneDynamicExtension(doc, allocator);
        }
    }

    // Buffers: just one buffer (frame 0), byteLength updated later
    rapidjson::Value buffers(rapidjson::kArrayType);
    rapidjson::Value mainBuffer(rapidjson::kObjectType);
    mainBuffer.AddMember("byteLength", static_cast<uint64_t>(binary_buffer_.size()), allocator);
    // MPEG_buffer_circular to announce frame count
    if (data.frameCount > 1) {
        rapidjson::Value bufferExt(rapidjson::kObjectType);
        rapidjson::Value circularExt(rapidjson::kObjectType);
        circularExt.AddMember("count", static_cast<uint64_t>(data.frameCount), allocator);
        circularExt.AddMember("media", 0, allocator);
        bufferExt.AddMember("MPEG_buffer_circular", circularExt, allocator);
        mainBuffer.AddMember("extensions", bufferExt, allocator);
    }
    buffers.PushBack(mainBuffer, allocator);
    doc.AddMember("buffers", buffers, allocator);

    // BufferViews for frame 0 static only
    rapidjson::Value bufferViews(rapidjson::kArrayType);
    size_t offset = 0;
    auto addBV = [&](size_t len, int stride) -> int {
        rapidjson::Value bv(rapidjson::kObjectType);
        bv.AddMember("buffer", 0, allocator);
        bv.AddMember("byteOffset", static_cast<uint64_t>(offset), allocator);
        bv.AddMember("byteLength", static_cast<uint64_t>(len), allocator);
        if (stride > 0) bv.AddMember("byteStride", stride, allocator);
        bufferViews.PushBack(bv, allocator);
        int idx = currentBufferViewIndex_++;
        offset += len;
        return idx;
    };
    currentBufferViewIndex_ = 0;

    int posBV = addBV(staticPositionBuffer_.size(), 0);
    int colBV = addBV(staticColorBuffer_.size(), 0);
    int oriBV = addBV(staticOrientationBuffer_.size(), 0);
    int sclBV = addBV(staticScaleBuffer_.size(), 0);
    int sh1BV = -1, sh2BV = -1, sh3BV = -1;
    if (!staticShFirstBuffer_.empty()) sh1BV = addBV(staticShFirstBuffer_.size(), 0);
    if (!staticShSecondBuffer_.empty()) sh2BV = addBV(staticShSecondBuffer_.size(), 0);
    if (!staticShThirdBuffer_.empty()) sh3BV = addBV(staticShThirdBuffer_.size(), 0);

    doc.AddMember("bufferViews", bufferViews, allocator);

    // Accessors for frame 0
    rapidjson::Value accessors(rapidjson::kArrayType);
    auto addAccessor = [&](int bv, const char* type, int compType, size_t count,
                           const std::vector<float>* minv, const std::vector<float>* maxv,
                           bool timed) -> int {
        rapidjson::Value acc(rapidjson::kObjectType);
        acc.AddMember("bufferView", bv, allocator);
        acc.AddMember("componentType", compType, allocator);
        acc.AddMember("count", static_cast<uint64_t>(count), allocator);
        acc.AddMember("type", rapidjson::Value(type, allocator), allocator);
        if (minv && maxv) {
            rapidjson::Value minA(rapidjson::kArrayType), maxA(rapidjson::kArrayType);
            for (auto v : *minv) minA.PushBack(v, allocator);
            for (auto v : *maxv) maxA.PushBack(v, allocator);
            acc.AddMember("min", minA, allocator);
            acc.AddMember("max", maxA, allocator);
        }
        if (timed) {
            rapidjson::Value ext(rapidjson::kObjectType);
            rapidjson::Value timedExt(rapidjson::kObjectType);
            timedExt.AddMember("immutable", false, allocator);
            timedExt.AddMember("componentType", compType, allocator);
            timedExt.AddMember("type", rapidjson::Value(type, allocator), allocator);
            ext.AddMember("MPEG_accessor_timed", timedExt, allocator);
            acc.AddMember("extensions", ext, allocator);
        }
        accessors.PushBack(acc, allocator);
        return currentAccessorIndex_++;
    };
    currentAccessorIndex_ = 0;

    std::set<int> encryptedAccessorIndices;

    std::vector<float> minPos(3), maxPos(3);
    for (int i=0;i<3;++i){ minPos[i]=data.frames[0].min_bounds[i]; maxPos[i]=data.frames[0].max_bounds[i]; }
    int posAcc = addAccessor(posBV, "VEC3", 5126, data.frames[0].count(), &minPos, &maxPos, data.frameCount>1);
    if (options.isAttributeEncrypted("position")) encryptedAccessorIndices.insert(posAcc);
    
    int colAcc = addAccessor(colBV, "VEC4", 5126, data.frames[0].count(), nullptr, nullptr, data.frameCount>1);
    if (options.isAttributeEncrypted("color")) encryptedAccessorIndices.insert(colAcc);

    int oriAcc = addAccessor(oriBV, "VEC4", 5126, data.frames[0].count(), nullptr, nullptr, data.frameCount>1);
    if (options.isAttributeEncrypted("orientation")) encryptedAccessorIndices.insert(oriAcc);
    
    int sclAcc = addAccessor(sclBV, "VEC3", 5126, data.frames[0].count(), nullptr, nullptr, data.frameCount>1);
    if (options.isAttributeEncrypted("scale")) encryptedAccessorIndices.insert(sclAcc);

    int sh1Acc = -1, sh2Acc = -1, sh3Acc = -1;
    if (sh1BV>=0) sh1Acc = addAccessor(sh1BV, "SCALAR", 5126, staticShFirstBuffer_.size()/sizeof(float), nullptr, nullptr, data.frameCount>1);
    if (sh2BV>=0) sh2Acc = addAccessor(sh2BV, "SCALAR", 5126, staticShSecondBuffer_.size()/sizeof(float), nullptr, nullptr, data.frameCount>1);
    if (sh3BV>=0) sh3Acc = addAccessor(sh3BV, "SCALAR", 5126, staticShThirdBuffer_.size()/sizeof(float), nullptr, nullptr, data.frameCount>1);
    
    if (options.isAttributeEncrypted("sh")) {
        if(sh1Acc != -1) encryptedAccessorIndices.insert(sh1Acc);
        if(sh2Acc != -1) encryptedAccessorIndices.insert(sh2Acc);
        if(sh3Acc != -1) encryptedAccessorIndices.insert(sh3Acc);
    }
    doc.AddMember("accessors", accessors, allocator);

    // Nodes/scene
    rapidjson::Value scenes(rapidjson::kArrayType);
    rapidjson::Value scene(rapidjson::kObjectType);
    rapidjson::Value nodes(rapidjson::kArrayType);
    nodes.PushBack(0, allocator);
    scene.AddMember("nodes", nodes, allocator);
    scenes.PushBack(scene, allocator);
    doc.AddMember("scenes", scenes, allocator);
    doc.AddMember("scene", 0, allocator);

    rapidjson::Value nodesArray(rapidjson::kArrayType);
    rapidjson::Value node(rapidjson::kObjectType);
    node.AddMember("mesh", 0, allocator);
    node.AddMember("name", "GaussianSplatSequence", allocator);
    nodesArray.PushBack(node, allocator);
    doc.AddMember("nodes", nodesArray, allocator);

    // Mesh + primitive
    rapidjson::Value meshes(rapidjson::kArrayType);
    
    rapidjson::Value primitive(rapidjson::kObjectType);
    primitive.AddMember("mode", 0, allocator);
    rapidjson::Value attrs(rapidjson::kObjectType);
    attrs.AddMember("POSITION", posAcc, allocator);
    attrs.AddMember("COLOR_0", colAcc, allocator);
    primitive.AddMember("attributes", attrs, allocator);

    rapidjson::Value primExt(rapidjson::kObjectType);
    rapidjson::Value gsExt(rapidjson::kObjectType);
    rapidjson::Value gsAttrs(rapidjson::kObjectType);
    gsAttrs.AddMember("_GS_ORIENTATION", oriAcc, allocator);
    gsAttrs.AddMember("_GS_SCALE", sclAcc, allocator);
    if (sh1Acc>=0) {
        if (options.progressive) gsAttrs.AddMember("_GS_SH_COEFF_FIRST", sh1Acc, allocator);
        else gsAttrs.AddMember("_GS_SH_COEFF_R", sh1Acc, allocator);
    }
    if (sh2Acc>=0) {
        if (options.progressive) gsAttrs.AddMember("_GS_SH_COEFF_SECOND", sh2Acc, allocator);
        else gsAttrs.AddMember("_GS_SH_COEFF_G", sh2Acc, allocator);
    }
    if (sh3Acc>=0) {
        if (options.progressive) gsAttrs.AddMember("_GS_SH_COEFF_THIRD", sh3Acc, allocator);
        else gsAttrs.AddMember("_GS_SH_COEFF_B", sh3Acc, allocator);
    }
    gsExt.AddMember("attributes", gsAttrs, allocator);
    primExt.AddMember("EXT_gaussian_splats", gsExt, allocator);
    primitive.AddMember("extensions", primExt, allocator);

    rapidjson::Value prims(rapidjson::kArrayType);
    prims.PushBack(primitive, allocator);
    rapidjson::Value meshObj(rapidjson::kObjectType);
    meshObj.AddMember("primitives", prims, allocator);
    meshObj.AddMember("name", "GaussianSplatSequence", allocator);
    
    if (options.drm_enabled) {
        if (!meshObj.HasMember("extensions")) {
            meshObj.AddMember("extensions", rapidjson::Value(rapidjson::kObjectType), allocator);
        }
        rapidjson::Value drmExtension(rapidjson::kObjectType);
        drmExtension.AddMember("schemeIdUri", "urn:exp:simple-xor-drm", allocator);
        drmExtension.AddMember("keyId", rapidjson::Value(options.drm_key_id.c_str(), allocator), allocator);
        
        rapidjson::Value encryptedAccessorsArray(rapidjson::kArrayType);
        for (int index : encryptedAccessorIndices) {
            encryptedAccessorsArray.PushBack(index, allocator);
        }
        drmExtension.AddMember("encryptedAccessors", encryptedAccessorsArray, allocator);
        
        meshObj["extensions"].AddMember("EXT_content_protection", drmExtension, allocator);
    }

    meshes.PushBack(meshObj, allocator);
    doc.AddMember("meshes", meshes, allocator);

    // Optional camera embedding
    if (!options.viewpoint_file.empty()) {
        float q[4] = {0,0,0,1}, c[3] = {0,0,0}, d = 0; if (loadViewpointFile(options.viewpoint_file, q, c, d)) {
            float v[3] = {0.0f, 0.0f, d};
            float vr[3]; rotateVectorByQuat(q, v, vr);
            float pos[3] = { c[0]+vr[0], c[1]+vr[1], c[2]+vr[2] };
            rapidjson::Value cameras(rapidjson::kArrayType);
            rapidjson::Value cam(rapidjson::kObjectType);
            cam.AddMember("type","perspective", allocator);
            rapidjson::Value persp(rapidjson::kObjectType);
            persp.AddMember("yfov", (float)(M_PI/180.0 * 45.0), allocator);
            persp.AddMember("znear", 0.5f, allocator);
            persp.AddMember("zfar", d, allocator);
            cam.AddMember("perspective", persp, allocator);
            cameras.PushBack(cam, allocator);
            doc.AddMember("cameras", cameras, allocator);
            rapidjson::Value camNode(rapidjson::kObjectType);
            camNode.AddMember("camera", 0, allocator);
            rapidjson::Value trans(rapidjson::kArrayType);
            trans.PushBack(pos[0], allocator).PushBack(pos[1], allocator).PushBack(pos[2], allocator);
            camNode.AddMember("translation", trans, allocator);
            rapidjson::Value rotA(rapidjson::kArrayType);
            rotA.PushBack(q[0], allocator).PushBack(q[1], allocator).PushBack(q[2], allocator).PushBack(q[3], allocator);
            camNode.AddMember("rotation", rotA, allocator);
            if (doc.HasMember("nodes") && doc["nodes"].IsArray()) {
                doc["nodes"].PushBack(camNode, allocator);
                if (doc.HasMember("scenes") && doc["scenes"].IsArray() && !doc["scenes"].Empty()) {
                    doc["scenes"][0]["nodes"].PushBack(static_cast<int>(doc["nodes"].Size()-1), allocator);
                }
            }
        }
    }

    // Stringify
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    doc.Accept(writer);
    return sb.GetString();
}

void MPEGGltfWriter::addMPEGMediaExtension(rapidjson::Document& doc,
                                         rapidjson::Document::AllocatorType& allocator,
                                         const MultiFrameGaussianSplatData& /*data*/) {
    if (!doc.HasMember("extensions")) {
        doc.AddMember("extensions", rapidjson::Value(rapidjson::kObjectType), allocator);
    }
    
    rapidjson::Value mpegMedia(rapidjson::kObjectType);
    rapidjson::Value mediaArray(rapidjson::kArrayType);
    
    rapidjson::Value mediaItem(rapidjson::kObjectType);
    mediaItem.AddMember("name", "gaussian_splat_sequence", allocator);
    mediaItem.AddMember("startTime", 0.0, allocator);
    
    rapidjson::Value alternatives(rapidjson::kArrayType);
    rapidjson::Value alternative(rapidjson::kObjectType);
    alternative.AddMember("mimeType", "binary/chunk", allocator);
    alternatives.PushBack(alternative, allocator);
    
    mediaItem.AddMember("alternatives", alternatives, allocator);
    mediaArray.PushBack(mediaItem, allocator);
    
    mpegMedia.AddMember("media", mediaArray, allocator);
    doc["extensions"].AddMember("MPEG_media", mpegMedia, allocator);
}

void MPEGGltfWriter::addMPEGSceneDynamicExtension(rapidjson::Document& doc,
                                                  rapidjson::Document::AllocatorType& allocator) {
    if (!doc.HasMember("scenes") || !doc["scenes"].IsArray() || doc["scenes"].Empty()) {
        return;
    }
    
    rapidjson::Value& scene = doc["scenes"][0];
    if (!scene.HasMember("extensions")) {
        scene.AddMember("extensions", rapidjson::Value(rapidjson::kObjectType), allocator);
    }
    
    rapidjson::Value sceneDynamic(rapidjson::kObjectType);
    sceneDynamic.AddMember("media", 0, allocator);
    scene["extensions"].AddMember("MPEG_scene_dynamic", sceneDynamic, allocator);
}

void MPEGGltfWriter::addCircularBuffers(rapidjson::Document& doc,
                                      rapidjson::Document::AllocatorType& allocator,
                                      const MultiFrameGaussianSplatData& data,
                                      bool /*progressive*/) {
    rapidjson::Value buffers(rapidjson::kArrayType);
    
    rapidjson::Value mainBuffer(rapidjson::kObjectType);
    mainBuffer.AddMember("byteLength", static_cast<uint64_t>(0), allocator); 
    
    if (data.frameCount > 1) {
        rapidjson::Value bufferExt(rapidjson::kObjectType);
        rapidjson::Value circularExt(rapidjson::kObjectType);
        
        const auto& circularInfo = circularBufferInfo_.frameCount > 0 ? circularBufferInfo_ : data;
        circularExt.AddMember("count", static_cast<uint64_t>(circularInfo.frameCount), allocator);
        circularExt.AddMember("media", 0, allocator);
        
        bufferExt.AddMember("MPEG_buffer_circular", circularExt, allocator);
        mainBuffer.AddMember("extensions", bufferExt, allocator);
    }
    
    buffers.PushBack(mainBuffer, allocator);
    currentBufferIndex_ = 1;
    doc.AddMember("buffers", buffers, allocator);
}

void MPEGGltfWriter::addCircularBufferViews(rapidjson::Document& doc,
                                          rapidjson::Document::AllocatorType& allocator,
                                          const MultiFrameGaussianSplatData& data,
                                          const SequenceConversionOptions& options) {
    rapidjson::Value bufferViews(rapidjson::kArrayType);
    size_t offset = 0;
    
    rapidjson::Value staticPosView(rapidjson::kObjectType);
    staticPosView.AddMember("buffer", 0, allocator);
    staticPosView.AddMember("byteOffset", static_cast<uint64_t>(offset), allocator);
    staticPosView.AddMember("byteLength", static_cast<uint64_t>(staticPositionBuffer_.size()), allocator);
    staticPosView.AddMember("byteStride", 12, allocator);
    bufferViews.PushBack(staticPosView, allocator);
    int staticPosViewIdx = currentBufferViewIndex_++;
    offset += staticPositionBuffer_.size();
    
    rapidjson::Value staticColView(rapidjson::kObjectType);
    staticColView.AddMember("buffer", 0, allocator);
    staticColView.AddMember("byteOffset", static_cast<uint64_t>(offset), allocator);
    staticColView.AddMember("byteLength", static_cast<uint64_t>(staticColorBuffer_.size()), allocator);
    staticColView.AddMember("byteStride", 16, allocator);
    bufferViews.PushBack(staticColView, allocator);
    int staticColViewIdx = currentBufferViewIndex_++;
    offset += staticColorBuffer_.size();
    
    rapidjson::Value staticOriView(rapidjson::kObjectType);
    staticOriView.AddMember("buffer", 0, allocator);
    staticOriView.AddMember("byteOffset", static_cast<uint64_t>(offset), allocator);
    staticOriView.AddMember("byteLength", static_cast<uint64_t>(staticOrientationBuffer_.size()), allocator);
    staticOriView.AddMember("byteStride", 16, allocator);
    bufferViews.PushBack(staticOriView, allocator);
    int staticOriViewIdx = currentBufferViewIndex_++;
    offset += staticOrientationBuffer_.size();
    
    rapidjson::Value staticScaleView(rapidjson::kObjectType);
    staticScaleView.AddMember("buffer", 0, allocator);
    staticScaleView.AddMember("byteOffset", static_cast<uint64_t>(offset), allocator);
    staticScaleView.AddMember("byteLength", static_cast<uint64_t>(staticScaleBuffer_.size()), allocator);
    staticScaleView.AddMember("byteStride", 12, allocator);
    bufferViews.PushBack(staticScaleView, allocator);
    int staticScaleViewIdx = currentBufferViewIndex_++;
    offset += staticScaleBuffer_.size();
    
    if (!staticShFirstBuffer_.empty()) {
        rapidjson::Value staticSh1View(rapidjson::kObjectType);
        staticSh1View.AddMember("buffer", 0, allocator);
        staticSh1View.AddMember("byteOffset", static_cast<uint64_t>(offset), allocator);
        staticSh1View.AddMember("byteLength", static_cast<uint64_t>(staticShFirstBuffer_.size()), allocator);
        bufferViews.PushBack(staticSh1View, allocator);
        bufferViewIndices_.shFirst = currentBufferViewIndex_++;
        offset += staticShFirstBuffer_.size();
    }
    
    if (!staticShSecondBuffer_.empty()) {
        rapidjson::Value staticSh2View(rapidjson::kObjectType);
        staticSh2View.AddMember("buffer", 0, allocator);
        staticSh2View.AddMember("byteOffset", static_cast<uint64_t>(offset), allocator);
        staticSh2View.AddMember("byteLength", static_cast<uint64_t>(staticShSecondBuffer_.size()), allocator);
        bufferViews.PushBack(staticSh2View, allocator);
        bufferViewIndices_.shSecond = currentBufferViewIndex_++;
        offset += staticShSecondBuffer_.size();
    }
    
    if (!staticShThirdBuffer_.empty()) {
        rapidjson::Value staticSh3View(rapidjson::kObjectType);
        staticSh3View.AddMember("buffer", 0, allocator);
        staticSh3View.AddMember("byteOffset", static_cast<uint64_t>(offset), allocator);
        staticSh3View.AddMember("byteLength", static_cast<uint64_t>(staticShThirdBuffer_.size()), allocator);
        bufferViews.PushBack(staticSh3View, allocator);
        bufferViewIndices_.shThird = currentBufferViewIndex_++;
        offset += staticShThirdBuffer_.size();
    }
    
    bufferViewIndices_.position = staticPosViewIdx;
    bufferViewIndices_.color = staticColViewIdx;
    bufferViewIndices_.orientation = staticOriViewIdx;
    bufferViewIndices_.scale = staticScaleViewIdx;
    
    if (data.frameCount > 1) {
        rapidjson::Value circularView(rapidjson::kObjectType);
        circularView.AddMember("buffer", 0, allocator);
        circularView.AddMember("byteOffset", static_cast<uint64_t>(offset), allocator);
        circularView.AddMember("byteLength", static_cast<uint64_t>(positionBuffer_.size()), allocator);
        bufferViews.PushBack(circularView, allocator);
        
        int circularBufferViewIndex = currentBufferViewIndex_++;
        bufferViewIndices_.positionCircular = circularBufferViewIndex;
        bufferViewIndices_.colorCircular = circularBufferViewIndex;  
        bufferViewIndices_.orientationCircular = circularBufferViewIndex;
        bufferViewIndices_.scaleCircular = circularBufferViewIndex;
        bufferViewIndices_.shFirstCircular = circularBufferViewIndex;
        bufferViewIndices_.shSecondCircular = circularBufferViewIndex;
        bufferViewIndices_.shThirdCircular = circularBufferViewIndex;
        
        if (options.verbose) {
            printf("[MPEGGltfWriter] Created circular buffer view at index %d\n", circularBufferViewIndex);
            printf("[MPEGGltfWriter] Circular data starts at offset %zu, length %zu\n", offset, positionBuffer_.size());
        }
    }
    
    doc.AddMember("bufferViews", bufferViews, allocator);
}


void MPEGGltfWriter::addTimedAccessors(rapidjson::Document& doc,
                                     rapidjson::Document::AllocatorType& allocator,
                                     const MultiFrameGaussianSplatData& data,
                                     bool progressive) {
    rapidjson::Value accessors(rapidjson::kArrayType);
    
    rapidjson::Value posAccessor(rapidjson::kObjectType);
    posAccessor.AddMember("bufferView", bufferViewIndices_.position, allocator);
    posAccessor.AddMember("componentType", 5126, allocator);
    posAccessor.AddMember("type", "VEC3", allocator);
    posAccessor.AddMember("count", static_cast<uint64_t>(data.frames[0].count()), allocator);
    
    rapidjson::Value minArray(rapidjson::kArrayType);
    rapidjson::Value maxArray(rapidjson::kArrayType);
    for (int i = 0; i < 3; ++i) {
        minArray.PushBack(data.frames[0].min_bounds[i], allocator);
        maxArray.PushBack(data.frames[0].max_bounds[i], allocator);
    }
    posAccessor.AddMember("min", minArray, allocator);
    posAccessor.AddMember("max", maxArray, allocator);
    
    if (data.frameCount > 1) {
        rapidjson::Value posTimedExt(rapidjson::kObjectType);
        rapidjson::Value posTimed(rapidjson::kObjectType);
        posTimed.AddMember("immutable", false, allocator);
        posTimed.AddMember("bufferView", bufferViewIndices_.positionCircular, allocator);
        posTimedExt.AddMember("MPEG_accessor_timed", posTimed, allocator);
        posAccessor.AddMember("extensions", posTimedExt, allocator);
    }
    
    accessors.PushBack(posAccessor, allocator);
    accessorIndices_.position = currentAccessorIndex_++;
    
    rapidjson::Value colAccessor(rapidjson::kObjectType);
    colAccessor.AddMember("bufferView", bufferViewIndices_.color, allocator);
    colAccessor.AddMember("componentType", 5126, allocator);
    colAccessor.AddMember("type", "VEC4", allocator);
    colAccessor.AddMember("count", static_cast<uint64_t>(data.frames[0].count()), allocator);
    
    if (data.frameCount > 1) {
        rapidjson::Value colTimedExt(rapidjson::kObjectType);
        rapidjson::Value colTimed(rapidjson::kObjectType);
        colTimed.AddMember("immutable", false, allocator);
        colTimed.AddMember("bufferView", bufferViewIndices_.colorCircular, allocator);
        colTimedExt.AddMember("MPEG_accessor_timed", colTimed, allocator);
        colAccessor.AddMember("extensions", colTimedExt, allocator);
    }
    
    accessors.PushBack(colAccessor, allocator);
    accessorIndices_.color = currentAccessorIndex_++;
    
    rapidjson::Value oriAccessor(rapidjson::kObjectType);
    oriAccessor.AddMember("bufferView", bufferViewIndices_.orientation, allocator);
    oriAccessor.AddMember("componentType", 5126, allocator);
    oriAccessor.AddMember("type", "VEC4", allocator);
    oriAccessor.AddMember("count", static_cast<uint64_t>(data.frames[0].count()), allocator);
    
    if (data.frameCount > 1) {
        rapidjson::Value oriTimedExt(rapidjson::kObjectType);
        rapidjson::Value oriTimed(rapidjson::kObjectType);
        oriTimed.AddMember("immutable", false, allocator);
        oriTimed.AddMember("bufferView", bufferViewIndices_.orientationCircular, allocator);
        oriTimedExt.AddMember("MPEG_accessor_timed", oriTimed, allocator);
        oriAccessor.AddMember("extensions", oriTimedExt, allocator);
    }
    
    accessors.PushBack(oriAccessor, allocator);
    accessorIndices_.orientation = currentAccessorIndex_++;
    
    rapidjson::Value scaleAccessor(rapidjson::kObjectType);
    scaleAccessor.AddMember("bufferView", bufferViewIndices_.scale, allocator);
    scaleAccessor.AddMember("componentType", 5126, allocator);
    scaleAccessor.AddMember("type", "VEC3", allocator);
    scaleAccessor.AddMember("count", static_cast<uint64_t>(data.frames[0].count()), allocator);
    
    if (data.frameCount > 1) {
        rapidjson::Value scaleTimedExt(rapidjson::kObjectType);
        rapidjson::Value scaleTimed(rapidjson::kObjectType);
        scaleTimed.AddMember("immutable", false, allocator);
        scaleTimed.AddMember("bufferView", bufferViewIndices_.scaleCircular, allocator);
        scaleTimedExt.AddMember("MPEG_accessor_timed", scaleTimed, allocator);
        scaleAccessor.AddMember("extensions", scaleTimedExt, allocator);
    }
    
    accessors.PushBack(scaleAccessor, allocator);
    accessorIndices_.scale = currentAccessorIndex_++;
    
    if (shFirstBuffer_.size() > 0 || staticShFirstBuffer_.size() > 0) {
        rapidjson::Value sh1Accessor(rapidjson::kObjectType);
        sh1Accessor.AddMember("bufferView", bufferViewIndices_.shFirst, allocator);
        sh1Accessor.AddMember("componentType", 5126, allocator);
        
        if (progressive) {
            sh1Accessor.AddMember("type", "MAT3", allocator);
            sh1Accessor.AddMember("count", static_cast<uint64_t>(data.frames[0].count()), allocator);
        } else {
            sh1Accessor.AddMember("type", "SCALAR", allocator);
            sh1Accessor.AddMember("count", static_cast<uint64_t>(data.frames[0].count() * 15), allocator);
        }
        
        rapidjson::Value sh1TimedExt(rapidjson::kObjectType);
        rapidjson::Value sh1Timed(rapidjson::kObjectType);
        sh1Timed.AddMember("immutable", false, allocator);
        sh1Timed.AddMember("bufferView", bufferViewIndices_.shFirstCircular, allocator);
        sh1TimedExt.AddMember("MPEG_accessor_timed", sh1Timed, allocator);
        sh1Accessor.AddMember("extensions", sh1TimedExt, allocator);
        
        accessors.PushBack(sh1Accessor, allocator);
        accessorIndices_.shFirst = currentAccessorIndex_++;
    }
    
    if (shSecondBuffer_.size() > 0 || staticShSecondBuffer_.size() > 0) {
        rapidjson::Value sh2Accessor(rapidjson::kObjectType);
        sh2Accessor.AddMember("bufferView", bufferViewIndices_.shSecond, allocator);
        sh2Accessor.AddMember("componentType", 5126, allocator);
        sh2Accessor.AddMember("type", "SCALAR", allocator);
        sh2Accessor.AddMember("count", static_cast<uint64_t>(data.frames[0].count() * 15), allocator);
        
        rapidjson::Value sh2TimedExt(rapidjson::kObjectType);
        rapidjson::Value sh2Timed(rapidjson::kObjectType);
        sh2Timed.AddMember("immutable", false, allocator);
        sh2Timed.AddMember("bufferView", bufferViewIndices_.shSecondCircular, allocator);
        sh2TimedExt.AddMember("MPEG_accessor_timed", sh2Timed, allocator);
        sh2Accessor.AddMember("extensions", sh2TimedExt, allocator);
        
        accessors.PushBack(sh2Accessor, allocator);
        accessorIndices_.shSecond = currentAccessorIndex_++;
    }
    
    if (shThirdBuffer_.size() > 0 || staticShThirdBuffer_.size() > 0) {
        rapidjson::Value sh3Accessor(rapidjson::kObjectType);
        sh3Accessor.AddMember("bufferView", bufferViewIndices_.shThird, allocator);
        sh3Accessor.AddMember("componentType", 5126, allocator);
        sh3Accessor.AddMember("type", "SCALAR", allocator);
        
        if (progressive) {
            sh3Accessor.AddMember("count", static_cast<uint64_t>(data.frames[0].count() * 21), allocator);
        } else {
            sh3Accessor.AddMember("count", static_cast<uint64_t>(data.frames[0].count() * 15), allocator);
        }
        
        rapidjson::Value sh3TimedExt(rapidjson::kObjectType);
        rapidjson::Value sh3Timed(rapidjson::kObjectType);
        sh3Timed.AddMember("immutable", false, allocator);
        sh3Timed.AddMember("bufferView", bufferViewIndices_.shThirdCircular, allocator);
        sh3TimedExt.AddMember("MPEG_accessor_timed", sh3Timed, allocator);
        sh3Accessor.AddMember("extensions", sh3TimedExt, allocator);
        
        accessors.PushBack(sh3Accessor, allocator);
        accessorIndices_.shThird = currentAccessorIndex_++;
    }
    
    doc.AddMember("accessors", accessors, allocator);
}

void MPEGGltfWriter::addGaussianSplatPrimitive(rapidjson::Document& doc,
                                             rapidjson::Document::AllocatorType& allocator,
                                             const MultiFrameGaussianSplatData& /*data*/,
                                             bool progressive) {
    rapidjson::Value primitive(rapidjson::kObjectType);
    primitive.AddMember("mode", 0, allocator);
    
    rapidjson::Value attributes(rapidjson::kObjectType);
    attributes.AddMember("POSITION", accessorIndices_.position, allocator);
    attributes.AddMember("COLOR_0", accessorIndices_.color, allocator);
    primitive.AddMember("attributes", attributes, allocator);
    
    rapidjson::Value primExtensions(rapidjson::kObjectType);
    rapidjson::Value gaussianExt(rapidjson::kObjectType);
    
    rapidjson::Value gsAttributes(rapidjson::kObjectType);
    gsAttributes.AddMember("_GS_ORIENTATION", accessorIndices_.orientation, allocator);
    gsAttributes.AddMember("_GS_SCALE", accessorIndices_.scale, allocator);
    
    if (progressive) {
        if (accessorIndices_.shFirst >= 0) gsAttributes.AddMember("_GS_SH_COEFF_FIRST", accessorIndices_.shFirst, allocator);
        if (accessorIndices_.shSecond >= 0) gsAttributes.AddMember("_GS_SH_COEFF_SECOND", accessorIndices_.shSecond, allocator);
        if (accessorIndices_.shThird >= 0) gsAttributes.AddMember("_GS_SH_COEFF_THIRD", accessorIndices_.shThird, allocator);
    } else {
        if (accessorIndices_.shFirst >= 0) gsAttributes.AddMember("_GS_SH_COEFF_R", accessorIndices_.shFirst, allocator);
        if (accessorIndices_.shSecond >= 0) gsAttributes.AddMember("_GS_SH_COEFF_G", accessorIndices_.shSecond, allocator);
        if (accessorIndices_.shThird >= 0) gsAttributes.AddMember("_GS_SH_COEFF_B", accessorIndices_.shThird, allocator);
    }
    
    gaussianExt.AddMember("attributes", gsAttributes, allocator);
    primExtensions.AddMember("EXT_gaussian_splats", gaussianExt, allocator);
    primitive.AddMember("extensions", primExtensions, allocator);
    
    if(!doc.HasMember("meshes")) {
        doc.AddMember("meshes", rapidjson::Value(rapidjson::kArrayType), allocator);
    }
    rapidjson::Value& meshes = doc["meshes"];
    
    rapidjson::Value mesh(rapidjson::kObjectType);
    rapidjson::Value primitives(rapidjson::kArrayType);
    primitives.PushBack(primitive, allocator);
    mesh.AddMember("primitives", primitives, allocator);
    mesh.AddMember("name", "GaussianSplatSequence", allocator);
    meshes.PushBack(mesh, allocator);
}

void MPEGGltfWriter::writeAttributeBuffers(const MultiFrameGaussianSplatData& data,
                                         const SequenceConversionOptions& options) {
    if (!data.frames.empty()) {
        const auto& firstFrame = data.frames[0];
        writeStaticFrameBuffers(firstFrame, options);
    }
    
    if (data.frameCount > 1) {
        bufferWriter_.setBuffer(&positionBuffer_);
        size_t circularSize = bufferWriter_.writeInterleavedFrames(data, options.progressive);
        
        if(options.verbose) {
            std::cout << "[MPEGGltfWriter] Wrote " << circularSize << " bytes of interleaved circular buffer data" << std::endl;
        }
        
        circularBufferInfo_ = data;
    }
}

void MPEGGltfWriter::writeStaticFrameBuffers(const GaussianSplatData& frameData,
                                           const SequenceConversionOptions& options) {
    staticPositionBuffer_.clear();
    staticColorBuffer_.clear();
    staticOrientationBuffer_.clear();
    staticScaleBuffer_.clear();
    staticShFirstBuffer_.clear();
    staticShSecondBuffer_.clear();
    staticShThirdBuffer_.clear();
    
    size_t count = frameData.count();
    const float SH_C0 = 0.28209479f;
    
    staticPositionBuffer_.resize(count * 3 * sizeof(float));
    staticColorBuffer_.resize(count * 4 * sizeof(float));
    staticOrientationBuffer_.resize(count * 4 * sizeof(float));
    staticScaleBuffer_.resize(count * 3 * sizeof(float));
    
    bool hasShData = false;
    if (!frameData.splats.empty()) {
        for (size_t j = 0; j < 45 && !hasShData; ++j) {
            if (frameData.splats[0].sh_rest[j] != 0.0f) hasShData = true;
        }
    }
    
    if (options.verbose) {
        std::cout << "[MPEGGltfWriter] SH data check: hasShData=" << hasShData 
                  << ", progressive=" << options.progressive 
                  << ", frameData.splats.size()=" << frameData.splats.size() << std::endl;
    }
    
    if (hasShData || options.progressive) {
        if (options.progressive) {
            staticShFirstBuffer_.resize(count * 9 * sizeof(float));
            staticShSecondBuffer_.resize(count * 15 * sizeof(float));
            staticShThirdBuffer_.resize(count * 21 * sizeof(float));
        } else {
            staticShFirstBuffer_.resize(count * 15 * sizeof(float));
            staticShSecondBuffer_.resize(count * 15 * sizeof(float));
            staticShThirdBuffer_.resize(count * 15 * sizeof(float));
        }
    }
    
    for (size_t i = 0; i < count; ++i) {
        const auto& splat = frameData.splats[i];
        
        std::memcpy(staticPositionBuffer_.data() + i * 12, splat.position.data(), 12);

        float color[4] = {
            splat.sh_dc[0] * SH_C0 + 0.5f,
            splat.sh_dc[1] * SH_C0 + 0.5f,
            splat.sh_dc[2] * SH_C0 + 0.5f,
            1.0f / (1.0f + std::exp(-splat.opacity))
        };
        std::memcpy(staticColorBuffer_.data() + i * 16, color, 16);

        float quat[4] = { splat.rotation[0], splat.rotation[1], splat.rotation[2], splat.rotation[3] };
        float norm = std::sqrt(quat[0]*quat[0] + quat[1]*quat[1] + quat[2]*quat[2] + quat[3]*quat[3]);
        if (norm > 0) { for(int k=0; k<4; ++k) quat[k] /= norm; } else { quat[3]=1.0f; }
        std::memcpy(staticOrientationBuffer_.data() + i * 16, quat, 16);
        
        std::memcpy(staticScaleBuffer_.data() + i * 12, splat.scale.data(), 12);
        
        if (!staticShFirstBuffer_.empty()) {
            if (options.progressive) {
                std::memcpy(staticShFirstBuffer_.data() + i * (9 * sizeof(float)), splat.sh_rest.data(), 9 * sizeof(float));
                std::memcpy(staticShSecondBuffer_.data() + i * (15 * sizeof(float)), splat.sh_rest.data() + 9, 15 * sizeof(float));
                std::memcpy(staticShThirdBuffer_.data() + i * (21 * sizeof(float)), splat.sh_rest.data() + 24, 21 * sizeof(float));
            } else {
                for (int j = 0; j < 15; j++) {
                    reinterpret_cast<float*>(staticShFirstBuffer_.data())[i * 15 + j] = splat.sh_rest[j*3+0];
                    reinterpret_cast<float*>(staticShSecondBuffer_.data())[i * 15 + j] = splat.sh_rest[j*3+1];
                    reinterpret_cast<float*>(staticShThirdBuffer_.data())[i * 15 + j] = splat.sh_rest[j*3+2];
                }
            }
        }
    }

    if (options.drm_enabled) {
        if (options.verbose) std::cout << "[MPEGGltfWriter] Applying DRM to static frame (frame 0)..." << std::endl;
        if (options.isAttributeEncrypted("position")) xorEncrypt(staticPositionBuffer_, options.drm_key, options.verbose);
        if (options.isAttributeEncrypted("color")) xorEncrypt(staticColorBuffer_, options.drm_key, options.verbose);
        if (options.isAttributeEncrypted("orientation")) xorEncrypt(staticOrientationBuffer_, options.drm_key, options.verbose);
        if (options.isAttributeEncrypted("scale")) xorEncrypt(staticScaleBuffer_, options.drm_key, options.verbose);
        if (options.isAttributeEncrypted("sh")) {
            xorEncrypt(staticShFirstBuffer_, options.drm_key, options.verbose);
            xorEncrypt(staticShSecondBuffer_, options.drm_key, options.verbose);
            xorEncrypt(staticShThirdBuffer_, options.drm_key, options.verbose);
        }
    }
}

void MPEGGltfWriter::combineBuffers() {
    size_t totalSize = staticPositionBuffer_.size() + staticColorBuffer_.size() + 
                       staticOrientationBuffer_.size() + staticScaleBuffer_.size() +
                       staticShFirstBuffer_.size() + staticShSecondBuffer_.size() + 
                       staticShThirdBuffer_.size() +
                       positionBuffer_.size();
    
    binary_buffer_.clear();
    binary_buffer_.reserve(totalSize);
    
    binary_buffer_.insert(binary_buffer_.end(), staticPositionBuffer_.begin(), staticPositionBuffer_.end());
    binary_buffer_.insert(binary_buffer_.end(), staticColorBuffer_.begin(), staticColorBuffer_.end());
    binary_buffer_.insert(binary_buffer_.end(), staticOrientationBuffer_.begin(), staticOrientationBuffer_.end());
    binary_buffer_.insert(binary_buffer_.end(), staticScaleBuffer_.begin(), staticScaleBuffer_.end());
    binary_buffer_.insert(binary_buffer_.end(), staticShFirstBuffer_.begin(), staticShFirstBuffer_.end());
    binary_buffer_.insert(binary_buffer_.end(), staticShSecondBuffer_.begin(), staticShSecondBuffer_.end());
    binary_buffer_.insert(binary_buffer_.end(), staticShThirdBuffer_.begin(), staticShThirdBuffer_.end());
    
    binary_buffer_.insert(binary_buffer_.end(), positionBuffer_.begin(), positionBuffer_.end());
    
    while (binary_buffer_.size() % 4 != 0) {
        binary_buffer_.push_back(0);
    }
}

// ## FINAL FIX: REMOVED DUPLICATE FUNCTION ##
// The combineStaticBuffers() was here, now it's gone.

bool MPEGGltfWriter::writeSequenceChunked(const std::string& filename,
                                        const MultiFrameGaussianSplatData& data,
                                        const SequenceConversionOptions& options) {
    if (data.frames.empty()) {
        error_ = "No frame data to write";
        return false;
    }

    writeStaticFrameBuffers(data.frames[0], options);
    combineStaticBuffers();

    std::string json = createSequenceJSONChunked(data, options);

    std::string paddedJson = json;
    while (paddedJson.size() % 4 != 0) paddedJson += ' ';
    padBuffer(4);

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        error_ = "Failed to open output file: " + filename;
        return false;
    }

    auto calcFramePayloadSize = [&](const GaussianSplatData& f)->size_t{
        size_t count = f.count();
        size_t sz = 0;
        sz += 16; // SPLTFrameHeader
        sz += count * 3 * sizeof(float); // pos
        sz += count * 4 * sizeof(float); // color
        sz += count * 4 * sizeof(float); // orientation
        sz += count * 3 * sizeof(float); // scale
        if (options.progressive) {
            sz += count * (9 + 15 + 21) * sizeof(float);
        } else {
            sz += count * 45 * sizeof(float);
        }
        return (sz + 3) & ~size_t(3);
    };

    uint32_t jsonChunkLen = static_cast<uint32_t>(paddedJson.size());
    uint32_t binChunkLen = static_cast<uint32_t>(binary_buffer_.size());
    size_t glbCoreSize = sizeof(GlbHeader) + sizeof(ChunkHeader) + jsonChunkLen + sizeof(ChunkHeader) + binChunkLen;
    
    if (glbCoreSize > 0xFFFFFFFFull) {
        error_ = "Core GLB (JSON+BIN) exceeds 4 GiB.";
        return false;
    }

    GlbHeader header;
    header.length = static_cast<uint32_t>(glbCoreSize);
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    ChunkHeader jsonChunk; jsonChunk.length = jsonChunkLen; jsonChunk.type = CHUNK_TYPE_JSON;
    file.write(reinterpret_cast<const char*>(&jsonChunk), sizeof(jsonChunk));
    file.write(paddedJson.data(), paddedJson.size());

    ChunkHeader binChunk; binChunk.length = binChunkLen; binChunk.type = CHUNK_TYPE_BIN;
    file.write(reinterpret_cast<const char*>(&binChunk), sizeof(binChunk));
    if (binChunkLen)
        file.write(reinterpret_cast<const char*>(binary_buffer_.data()), binary_buffer_.size());

    const uint32_t SPLT_TYPE = 0x53504C54; // 'SPLT'
    for (size_t fi = 1; fi < data.frameCount; ++fi) {
        const auto& f = data.frames[fi];
        size_t count = f.count();
        std::vector<uint8_t> payload;
        payload.reserve(calcFramePayloadSize(f));

        struct SPLTFrameHeader { uint32_t splatCount; uint32_t flags; uint64_t timestamp; };
        SPLTFrameHeader fh{};
        fh.splatCount = static_cast<uint32_t>(count);
        fh.flags = options.progressive ? 1u : 0u;
        float tsec = (fi < data.frameTimes.size()) ? data.frameTimes[fi] : static_cast<float>(fi)/data.frameRate;
        fh.timestamp = toTimestamp64(tsec);
        payload.resize(payload.size() + sizeof(SPLTFrameHeader));
        std::memcpy(payload.data(), &fh, sizeof(SPLTFrameHeader));
        
        auto appendAndMaybeEncrypt = [&](const std::vector<float>& buffer, const std::string& attrName) {
            std::vector<uint8_t> temp_byte_buffer(buffer.size() * sizeof(float));
            std::memcpy(temp_byte_buffer.data(), buffer.data(), temp_byte_buffer.size());
            
            if (options.isAttributeEncrypted(attrName)) {
                xorEncrypt(temp_byte_buffer, options.drm_key, options.verbose);
            }
            
            payload.insert(payload.end(), temp_byte_buffer.begin(), temp_byte_buffer.end());
        };
        
        std::vector<float> tmp;
        const float SH_C0 = 0.28209479f;
        
        // Position
        tmp.clear(); tmp.reserve(count*3);
        for(const auto& s : f.splats) tmp.insert(tmp.end(), s.position.begin(), s.position.end());
        appendAndMaybeEncrypt(tmp, "position");

        // Color
        tmp.clear(); tmp.reserve(count*4);
        for (const auto& s : f.splats) {
            tmp.push_back(0.5f + SH_C0 * s.sh_dc[0]);
            tmp.push_back(0.5f + SH_C0 * s.sh_dc[1]);
            tmp.push_back(0.5f + SH_C0 * s.sh_dc[2]);
            tmp.push_back(1.0f / (1.0f + std::exp(-s.opacity)));
        }
        appendAndMaybeEncrypt(tmp, "color");

        // Orientation
        tmp.clear(); tmp.reserve(count*4);
        for (const auto& s : f.splats) {
            float q[4] = {s.rotation[0], s.rotation[1], s.rotation[2], s.rotation[3]};
            float n = std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);
            if (n>0){ for(int k=0; k<4; ++k) q[k]/=n; } else { q[3] = 1.0f; }
            tmp.insert(tmp.end(), q, q+4);
        }
        appendAndMaybeEncrypt(tmp, "orientation");

        // Scale
        tmp.clear(); tmp.reserve(count*3);
        for (const auto& s : f.splats) tmp.insert(tmp.end(), s.scale.begin(), s.scale.end());
        appendAndMaybeEncrypt(tmp, "scale");
        
        // SH
        bool hasShData = false;
        if (!f.splats.empty()) {
            for (size_t j = 0; j < 45 && !hasShData; ++j) {
                if (f.splats[0].sh_rest[j] != 0.0f) hasShData = true;
            }
        }
        if(hasShData || options.progressive) {
             if (!options.progressive) {
                 tmp.clear(); tmp.reserve(count*15); // R
                 for (const auto& s : f.splats) { for (int i=0;i<15;++i) tmp.push_back(s.sh_rest[i*3+0]); }
                 appendAndMaybeEncrypt(tmp, "sh");
                 
                 tmp.clear(); tmp.reserve(count*15); // G
                 for (const auto& s : f.splats) { for (int i=0;i<15;++i) tmp.push_back(s.sh_rest[i*3+1]); }
                 appendAndMaybeEncrypt(tmp, "sh");

                 tmp.clear(); tmp.reserve(count*15); // B
                 for (const auto& s : f.splats) { for (int i=0;i<15;++i) tmp.push_back(s.sh_rest[i*3+2]); }
                 appendAndMaybeEncrypt(tmp, "sh");
             } else {
                 // Progressive logic for SH needs careful implementation if required
             }
        }

        while (payload.size() % 4 != 0) payload.push_back(0);
        ChunkHeader splt; splt.length = static_cast<uint32_t>(payload.size()); splt.type = SPLT_TYPE;
        file.write(reinterpret_cast<const char*>(&splt), sizeof(splt));
        file.write(reinterpret_cast<const char*>(payload.data()), payload.size());
    }

    return true;
}

void MPEGGltfWriter::combineStaticBuffers() {
    binary_buffer_.clear();
    current_offset_ = 0;
    addToBuffer(staticPositionBuffer_.data(), staticPositionBuffer_.size());
    addToBuffer(staticColorBuffer_.data(), staticColorBuffer_.size());
    addToBuffer(staticOrientationBuffer_.data(), staticOrientationBuffer_.size());
    addToBuffer(staticScaleBuffer_.data(), staticScaleBuffer_.size());
    if (!staticShFirstBuffer_.empty()) addToBuffer(staticShFirstBuffer_.data(), staticShFirstBuffer_.size());
    if (!staticShSecondBuffer_.empty()) addToBuffer(staticShSecondBuffer_.data(), staticShSecondBuffer_.size());
    if (!staticShThirdBuffer_.empty()) addToBuffer(staticShThirdBuffer_.data(), staticShThirdBuffer_.size());
}

size_t MPEGGltfWriter::calculateTotalBufferSize(const MultiFrameGaussianSplatData& data) const {
    return data.positionBuffer.actualSize + data.colorBuffer.actualSize +
           data.orientationBuffer.actualSize + data.scaleBuffer.actualSize +
           data.shFirstBuffer.actualSize + data.shSecondBuffer.actualSize +
           data.shThirdBuffer.actualSize;
}

void MPEGGltfWriter::displayStatistics(const MultiFrameGaussianSplatData& data,
                                     const SequenceConversionOptions& options) const {
    std::cout << "\n========== GLB File Structure Statistics ==========\n" << std::endl;
    
    std::cout << "Sequence Information:" << std::endl;
    std::cout << "  Total frames: " << data.frameCount << std::endl;
    std::cout << "  Frame rate: " << data.frameRate << " fps" << std::endl;
    std::cout << "  Duration: " << data.duration << " seconds" << std::endl;
    std::cout << "  Max splats per frame: " << data.maxSplatCount << std::endl;
    std::cout << std::endl;
    
    std::cout << "Frame Splat Counts:" << std::endl;
    for (size_t i = 0; i < data.frameSplatCounts.size(); ++i) {
        std::cout << "  Frame " << i << ": " << data.frameSplatCounts[i] << " splats";
        if (data.frameSplatCounts[i] < data.maxSplatCount) {
            std::cout << " (" << (data.maxSplatCount - data.frameSplatCounts[i]) 
                      << " fewer than max)";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
    
    std::cout << "Buffer Layout:" << std::endl;
    size_t offset = 0;
    
    auto printBufferInfo = [&](const std::string& name, const CircularBufferInfo& buffer, size_t actualSize) {
        std::cout << "  " << name << ":" << std::endl;
        std::cout << "    Offset: " << offset << " bytes" << std::endl;
        std::cout << "    Size: " << actualSize << " bytes (" 
                  << (actualSize / 1024.0 / 1024.0) << " MB)" << std::endl;
        std::cout << "    Circular buffer count: " << buffer.frameCount << std::endl;
        size_t maxFrameSize = sizeof(TimedAccessorInfoHeader) + (data.maxSplatCount * buffer.stride);
        std::cout << "    Max frame size: " << maxFrameSize << " bytes" << std::endl;
        std::cout << "    Component count: " << buffer.componentCount << std::endl;
        std::cout << "    Stride: " << buffer.stride << " bytes" << std::endl;
        
        if (buffer.isDynamic) {
            std::cout << "    Dynamic sizing: YES" << std::endl;
        }
        
        offset += actualSize;
        std::cout << std::endl;
    };
    
    if(options.verbose) {
        printBufferInfo("Position Buffer", data.positionBuffer, positionBuffer_.size());
        printBufferInfo("Color Buffer", data.colorBuffer, colorBuffer_.size());
        printBufferInfo("Orientation Buffer", data.orientationBuffer, orientationBuffer_.size());
        printBufferInfo("Scale Buffer", data.scaleBuffer, scaleBuffer_.size());

        if (!shFirstBuffer_.empty()) {
            printBufferInfo("SH First Buffer", data.shFirstBuffer, shFirstBuffer_.size());
        }
        if (!shSecondBuffer_.empty()) {
            printBufferInfo("SH Second Buffer", data.shSecondBuffer, shSecondBuffer_.size());
        }
        if (!shThirdBuffer_.empty()) {
            printBufferInfo("SH Third Buffer", data.shThirdBuffer, shThirdBuffer_.size());
        }
    }
}

} // namespace ply2gltf