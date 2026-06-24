#include "MultiFramePlyReader.h"
#include <algorithm>
#include <filesystem>
#include <regex>
#include <iostream>
#include <future>
#include <numeric>

namespace fs = std::filesystem;

namespace ply2gltf {

MultiFramePlyReader::MultiFramePlyReader() = default;
MultiFramePlyReader::~MultiFramePlyReader() = default;

bool MultiFramePlyReader::readSequence(const std::vector<std::string>& filenames, 
                                      MultiFrameGaussianSplatData& data,
                                      bool verbose,
                                      bool parallel) {
    if (filenames.empty()) {
        error_ = "No input files provided";
        return false;
    }
    
    // Clear existing data
    data.frames.clear();
    data.frames.reserve(filenames.size());
    
    if (verbose) {
        std::cout << "Reading " << filenames.size() << " PLY files..." << std::endl;
    }
    
    // Load frames
    if (parallel && filenames.size() > 1) {
        // Parallel loading
        std::vector<std::future<std::pair<bool, GaussianSplatData>>> futures;
        
        for (const auto& filename : filenames) {
            futures.push_back(std::async(std::launch::async, [this, &filename, verbose]() {
                GaussianSplatData frameData;
                bool success = loadFrame(filename, frameData, false); // No verbose in parallel
                return std::make_pair(success, std::move(frameData));
            }));
        }
        
        // Collect results
        for (size_t i = 0; i < futures.size(); ++i) {
            auto result = futures[i].get();
            if (!result.first) {
                error_ = "Failed to load frame " + std::to_string(i) + ": " + filenames[i];
                return false;
            }
            data.frames.push_back(std::move(result.second));
            
            if (verbose && (i + 1) % 10 == 0) {
                std::cout << "Loaded " << (i + 1) << " / " << filenames.size() << " frames" << std::endl;
            }
        }
    } else {
        // Sequential loading
        for (size_t i = 0; i < filenames.size(); ++i) {
            GaussianSplatData frameData;
            if (!loadFrame(filenames[i], frameData, verbose)) {
                error_ = "Failed to load frame " + std::to_string(i) + ": " + filenames[i];
                return false;
            }
            data.frames.push_back(std::move(frameData));
        }
    }
    
    if (verbose) {
        std::cout << "Validating frame consistency..." << std::endl;
    }
    
    // Validate consistency between frames
    for (size_t i = 1; i < data.frames.size(); ++i) {
        if (!validateFrameConsistency(data.frames[0], data.frames[i], 0, i)) {
            return false;
        }
    }
    
    // Calculate buffer sizes
    data.calculateBufferSizes();
    
    if (verbose) {
        std::cout << "Successfully loaded " << data.frames.size() << " frames" << std::endl;
        std::cout << "Max splat count: " << data.maxSplatCount << std::endl;
        std::cout << "Duration: " << data.duration << " seconds at " << data.frameRate << " fps" << std::endl;
    }
    
    return true;
}

bool MultiFramePlyReader::readPattern(const std::string& pattern,
                                     MultiFrameGaussianSplatData& data,
                                     bool verbose) {
    auto filenames = findMatchingFiles(pattern);
    if (filenames.empty()) {
        error_ = "No files matching pattern: " + pattern;
        return false;
    }
    
    sortFilenamesNaturally(filenames);
    
    if (verbose) {
        std::cout << "Found " << filenames.size() << " files matching pattern" << std::endl;
    }
    
    return readSequence(filenames, data, verbose);
}

bool MultiFramePlyReader::validateFrameConsistency(const GaussianSplatData& frame1, 
                                                  const GaussianSplatData& frame2,
                                                  size_t index1, size_t index2) {
    // For now, we allow different splat counts but warn about it
    if (frame1.count() != frame2.count()) {
        std::cout << "Info: Frame " << index1 << " has " << frame1.count() 
                  << " splats, frame " << index2 << " has " << frame2.count() 
                  << " splats. Dynamic sizing will be used." << std::endl;
    }
    
    // Could add more validation here (e.g., check if bounding boxes are similar)
    return true;
}

bool MultiFramePlyReader::loadFrame(const std::string& filename, 
                                   GaussianSplatData& frameData,
                                   bool verbose) {
    PlyReader reader;
    if (!reader.read(filename, frameData, verbose)) {
        error_ = reader.getError();
        return false;
    }
    return true;
}

std::vector<std::string> MultiFramePlyReader::findMatchingFiles(const std::string& pattern) {
    std::vector<std::string> result;
    
    // Extract directory and filename pattern
    fs::path patternPath(pattern);
    fs::path directory = patternPath.parent_path();
    std::string filePattern = patternPath.filename().string();
    
    if (directory.empty()) {
        directory = ".";
    }
    
    // Convert glob pattern to regex
    std::string regexPattern = filePattern;
    // Replace * with .*
    regexPattern = std::regex_replace(regexPattern, std::regex("\\*"), ".*");
    // Replace ? with .
    regexPattern = std::regex_replace(regexPattern, std::regex("\\?"), ".");
    // Escape dots
    regexPattern = std::regex_replace(regexPattern, std::regex("\\."), "\\.");
    
    std::regex regex(regexPattern);
    
    // Find matching files
    try {
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (std::regex_match(filename, regex)) {
                    result.push_back(entry.path().string());
                }
            }
        }
    } catch (const std::exception& e) {
        // Directory doesn't exist or other error
        return result;
    }
    
    return result;
}

void MultiFramePlyReader::sortFilenamesNaturally(std::vector<std::string>& filenames) {
    // Natural sort to handle frame_1.ply, frame_2.ply, ..., frame_10.ply correctly
    std::sort(filenames.begin(), filenames.end(), [](const std::string& a, const std::string& b) {
        // Extract numbers from filenames
        std::regex numRegex("(\\d+)");
        std::smatch matchA, matchB;
        
        if (std::regex_search(a, matchA, numRegex) && std::regex_search(b, matchB, numRegex)) {
            // Compare the numeric parts
            int numA = std::stoi(matchA[1]);
            int numB = std::stoi(matchB[1]);
            if (numA != numB) {
                return numA < numB;
            }
        }
        
        // Fall back to lexicographic comparison
        return a < b;
    });
}

} // namespace ply2gltf