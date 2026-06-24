#include "StreamingMultiFrameReader.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <future>
#include <regex>
#include "tinyply.h"

namespace fs = std::filesystem;

namespace ply2gltf {

StreamingMultiFrameReader::StreamingMultiFrameReader() 
    : plyReader_(std::make_unique<PlyReader>()) {
}

StreamingMultiFrameReader::~StreamingMultiFrameReader() = default;

bool StreamingMultiFrameReader::readMetadata(const std::vector<std::string>& filenames,
                                            std::vector<FrameMetadata>& metadata,
                                            bool verbose) {
    if (filenames.empty()) {
        error_ = "No input files provided";
        return false;
    }
    
    metadata.clear();
    metadata.reserve(filenames.size());
    
    if (verbose) {
        std::cout << "Reading metadata from " << filenames.size() << " PLY files..." << std::endl;
    }
    
    // Read metadata from each file
    for (size_t i = 0; i < filenames.size(); ++i) {
        FrameMetadata frameMeta;
        frameMeta.filename = filenames[i];
        frameMeta.frameIndex = i;
        frameMeta.timestamp = static_cast<float>(i) / 30.0f; // Default 30fps
        
        if (!readPlyHeader(filenames[i], frameMeta, false)) {
            error_ = "Failed to read metadata from " + filenames[i];
            return false;
        }
        
        metadata.push_back(frameMeta);
        
        if (verbose && (i + 1) % 10 == 0) {
            std::cout << "Read metadata for " << (i + 1) << " / " << filenames.size() << " files" << std::endl;
        }
    }
    
    // Validate consistency
    if (!validateMetadataConsistency(metadata)) {
        return false;
    }
    
    if (verbose) {
        std::cout << "Successfully read metadata from " << metadata.size() << " frames" << std::endl;
        size_t maxSplats = 0;
        for (const auto& meta : metadata) {
            maxSplats = std::max(maxSplats, meta.splatCount);
        }
        std::cout << "Max splat count: " << maxSplats << std::endl;
    }
    
    return true;
}

bool StreamingMultiFrameReader::readFrame(const std::string& filename,
                                         GaussianSplatData& frameData,
                                         bool verbose) {
    return plyReader_->read(filename, frameData, verbose);
}

bool StreamingMultiFrameReader::processFrames(const std::vector<std::string>& filenames,
                                             FrameProcessor processor,
                                             bool verbose) {
    // First pass: read metadata
    std::vector<FrameMetadata> metadata;
    if (!readMetadata(filenames, metadata, verbose)) {
        return false;
    }
    
    if (verbose) {
        std::cout << "Processing " << filenames.size() << " frames..." << std::endl;
    }
    
    // Second pass: process frames one by one
    for (size_t i = 0; i < filenames.size(); ++i) {
        GaussianSplatData frameData;
        
        if (!readFrame(filenames[i], frameData, false)) {
            error_ = "Failed to read frame " + std::to_string(i) + ": " + filenames[i];
            return false;
        }
        
        // Process this frame
        if (!processor(i, frameData, metadata[i])) {
            error_ = "Frame processor failed for frame " + std::to_string(i);
            return false;
        }
        
        if (verbose && (i + 1) % 10 == 0) {
            std::cout << "Processed " << (i + 1) << " / " << filenames.size() << " frames" << std::endl;
        }
        
        // Frame data goes out of scope here and is freed
    }
    
    if (verbose) {
        std::cout << "Successfully processed all frames" << std::endl;
    }
    
    return true;
}

bool StreamingMultiFrameReader::validateMetadataConsistency(const std::vector<FrameMetadata>& metadata) {
    if (metadata.empty()) {
        error_ = "No metadata to validate";
        return false;
    }
    
    // For now, we're more lenient - frames can have different splat counts
    // This is handled by the dynamic accessor headers
    
    return true;
}

bool StreamingMultiFrameReader::readPlyHeader(const std::string& filename, 
                                              FrameMetadata& metadata,
                                              bool verbose) {
    try {
        std::ifstream file_stream(filename, std::ios::binary);
        if (!file_stream) {
            error_ = "Failed to open file: " + filename;
            return false;
        }
        
        tinyply::PlyFile file;
        file.parse_header(file_stream);
        
        // Get vertex count (splat count)
        auto elements = file.get_elements();
        for (const auto& e : elements) {
            if (e.name == "vertex") {
                metadata.splatCount = e.size;
                break;
            }
        }
        
        // Initialize bounds (would need full read to get actual bounds)
        metadata.minBounds = {0, 0, 0};
        metadata.maxBounds = {0, 0, 0};
        
        return true;
        
    } catch (const std::exception& e) {
        error_ = "Error reading PLY header: " + std::string(e.what());
        return false;
    }
}

} // namespace ply2gltf