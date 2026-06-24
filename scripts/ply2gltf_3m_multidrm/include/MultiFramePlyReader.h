#ifndef MULTI_FRAME_PLY_READER_H
#define MULTI_FRAME_PLY_READER_H

#include "PlyReader.h"
#include "MultiFrameGaussianSplat.h"
#include <vector>
#include <string>
#include <thread>
#include <future>

namespace ply2gltf {

class MultiFramePlyReader {
public:
    MultiFramePlyReader();
    ~MultiFramePlyReader();
    
    // Read a sequence of PLY files
    bool readSequence(const std::vector<std::string>& filenames, 
                     MultiFrameGaussianSplatData& data,
                     bool verbose = false,
                     bool parallel = true);
    
    // Read files matching a pattern
    bool readPattern(const std::string& pattern,
                    MultiFrameGaussianSplatData& data,
                    bool verbose = false);
    
    // Get error message
    const std::string& getError() const { return error_; }
    
private:
    std::string error_;
    
    // Validate frame consistency
    bool validateFrameConsistency(const GaussianSplatData& frame1, 
                                 const GaussianSplatData& frame2,
                                 size_t index1, size_t index2);
    
    // Load a single frame
    bool loadFrame(const std::string& filename, 
                  GaussianSplatData& frameData,
                  bool verbose);
    
    // Find files matching pattern
    std::vector<std::string> findMatchingFiles(const std::string& pattern);
    
    // Sort filenames naturally (frame_1.ply before frame_10.ply)
    void sortFilenamesNaturally(std::vector<std::string>& filenames);
};

} // namespace ply2gltf

#endif // MULTI_FRAME_PLY_READER_H