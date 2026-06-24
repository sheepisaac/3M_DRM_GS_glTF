#ifndef PLY_READER_H
#define PLY_READER_H

#include "GaussianSplat.h"
#include <string>
#include <memory>

namespace ply2gltf {

class PlyReader {
public:
    PlyReader();
    ~PlyReader();
    
    // Read PLY file containing Gaussian splats
    bool read(const std::string& filename, GaussianSplatData& data, bool verbose = false);
    
    // Get last error message
    const std::string& getError() const { return error_; }
    
private:
    std::string error_;
    
    // Validate PLY properties match expected Gaussian splat format
    bool validateProperties(const std::vector<std::string>& property_names);
};

} // namespace ply2gltf

#endif // PLY_READER_H