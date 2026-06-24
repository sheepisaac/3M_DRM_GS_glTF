#include "PlyReader.h"
#include "tinyply.h"
#include <fstream>
#include <iostream>
#include <algorithm>

namespace ply2gltf {

PlyReader::PlyReader() = default;
PlyReader::~PlyReader() = default;

bool PlyReader::read(const std::string& filename, GaussianSplatData& data, bool verbose) {
    try {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            error_ = "Failed to open file: " + filename;
            return false;
        }
        
        tinyply::PlyFile ply;
        ply.parse_header(file);
        
        if (verbose) {
            std::cout << "PLY file contains " << ply.get_elements().size() << " elements" << std::endl;
            for (const auto& e : ply.get_elements()) {
                std::cout << "Element '" << e.name << "' has " << e.size << " instances" << std::endl;
            }
        }
        
        // Request vertex properties in the expected order
        std::shared_ptr<tinyply::PlyData> positions, normals, f_dc, f_rest, opacities, scales, rotations;
        
        try {
            positions = ply.request_properties_from_element("vertex", {"x", "y", "z"});
            
            // Normal properties are optional - try to request but don't fail if not present
            try {
                normals = ply.request_properties_from_element("vertex", {"nx", "ny", "nz"});
            } catch (const std::exception&) {
                // Normals are optional, continue without them
                normals = nullptr;
                if (verbose) {
                    std::cout << "Note: Normal properties (nx, ny, nz) not found in PLY file, continuing without them" << std::endl;
                }
            }
            
            f_dc = ply.request_properties_from_element("vertex", {"f_dc_0", "f_dc_1", "f_dc_2"});
            opacities = ply.request_properties_from_element("vertex", {"opacity"});
            scales = ply.request_properties_from_element("vertex", {"scale_0", "scale_1", "scale_2"});
            rotations = ply.request_properties_from_element("vertex", {"rot_0", "rot_1", "rot_2", "rot_3"});
            
            // Request all available f_rest properties
            std::vector<std::string> f_rest_names;
            
            // First, check which f_rest properties exist
            int num_f_rest = 0;
            for (const auto& e : ply.get_elements()) {
                if (e.name == "vertex") {
                    for (const auto& p : e.properties) {
                        if (p.name.substr(0, 7) == "f_rest_") {
                            num_f_rest++;
                        }
                    }
                    break;
                }
            }
            
            // Request only the f_rest properties that exist
            for (int i = 0; i < num_f_rest; ++i) {
                f_rest_names.push_back("f_rest_" + std::to_string(i));
            }
            
            if (!f_rest_names.empty()) {
                f_rest = ply.request_properties_from_element("vertex", f_rest_names);
            }
        }
        catch (const std::exception& e) {
            error_ = "Failed to request properties: " + std::string(e.what());
            return false;
        }
        
        // Read the data
        ply.read(file);
        
        // Validate we got all required data
        if (!positions || positions->count == 0) {
            error_ = "No position data found";
            return false;
        }
        
        size_t count = positions->count;
        if (verbose) {
            std::cout << "Reading " << count << " Gaussian splats" << std::endl;
        }
        
        // Resize the output vector
        data.splats.resize(count);
        
        // Copy position data
        if (positions->t == tinyply::Type::FLOAT32) {
            const float* pos_data = reinterpret_cast<const float*>(positions->buffer.get());
            for (size_t i = 0; i < count; ++i) {
                data.splats[i].position[0] = pos_data[i * 3 + 0];
                data.splats[i].position[1] = pos_data[i * 3 + 1];
                data.splats[i].position[2] = pos_data[i * 3 + 2];
            }
        }
        
        // Copy normal data (if present)
        if (normals && normals->t == tinyply::Type::FLOAT32) {
            const float* normal_data = reinterpret_cast<const float*>(normals->buffer.get());
            for (size_t i = 0; i < count; ++i) {
                data.splats[i].normal[0] = normal_data[i * 3 + 0];
                data.splats[i].normal[1] = normal_data[i * 3 + 1];
                data.splats[i].normal[2] = normal_data[i * 3 + 2];
            }
        } else {
            // Initialize normals to zero if not present
            for (size_t i = 0; i < count; ++i) {
                data.splats[i].normal[0] = 0.0f;
                data.splats[i].normal[1] = 0.0f;
                data.splats[i].normal[2] = 0.0f;
            }
        }
        
        // Copy f_dc data
        if (f_dc && f_dc->t == tinyply::Type::FLOAT32) {
            const float* f_dc_data = reinterpret_cast<const float*>(f_dc->buffer.get());
            for (size_t i = 0; i < count; ++i) {
                data.splats[i].sh_dc[0] = f_dc_data[i * 3 + 0];
                data.splats[i].sh_dc[1] = f_dc_data[i * 3 + 1];
                data.splats[i].sh_dc[2] = f_dc_data[i * 3 + 2];
            }
        }
        
        // Copy f_rest data
        if (f_rest && f_rest->t == tinyply::Type::FLOAT32) {
            const float* f_rest_data = reinterpret_cast<const float*>(f_rest->buffer.get());
            size_t num_sh_coeffs = f_rest->count / count;  // Number of SH coefficients per splat
            
            for (size_t i = 0; i < count; ++i) {
                // Initialize all to zero first
                for (int j = 0; j < 45; ++j) {
                    data.splats[i].sh_rest[j] = 0.0f;
                }
                // Copy available coefficients
                for (size_t j = 0; j < num_sh_coeffs && j < 45; ++j) {
                    data.splats[i].sh_rest[j] = f_rest_data[i * num_sh_coeffs + j];
                }
            }
        } else {
            // No SH coefficients, initialize to zero
            for (size_t i = 0; i < count; ++i) {
                for (int j = 0; j < 45; ++j) {
                    data.splats[i].sh_rest[j] = 0.0f;
                }
            }
        }
        
        // Copy opacity data
        if (opacities && opacities->t == tinyply::Type::FLOAT32) {
            const float* opacity_data = reinterpret_cast<const float*>(opacities->buffer.get());
            for (size_t i = 0; i < count; ++i) {
                data.splats[i].opacity = opacity_data[i];
            }
        }
        
        // Copy scale data
        if (scales && scales->t == tinyply::Type::FLOAT32) {
            const float* scale_data = reinterpret_cast<const float*>(scales->buffer.get());
            for (size_t i = 0; i < count; ++i) {
                data.splats[i].scale[0] = scale_data[i * 3 + 0];
                data.splats[i].scale[1] = scale_data[i * 3 + 1];
                data.splats[i].scale[2] = scale_data[i * 3 + 2];
            }
        }
        
        // Copy rotation data
        if (rotations && rotations->t == tinyply::Type::FLOAT32) {
            const float* rotation_data = reinterpret_cast<const float*>(rotations->buffer.get());
            for (size_t i = 0; i < count; ++i) {
                data.splats[i].rotation[0] = rotation_data[i * 4 + 0];
                data.splats[i].rotation[1] = rotation_data[i * 4 + 1];
                data.splats[i].rotation[2] = rotation_data[i * 4 + 2];
                data.splats[i].rotation[3] = rotation_data[i * 4 + 3];
            }
        }
        
        // Compute bounding box
        data.computeBounds();
        
        if (verbose) {
            std::cout << "Successfully read " << count << " Gaussian splats" << std::endl;
            std::cout << "Bounds: [" 
                      << data.min_bounds[0] << ", " << data.min_bounds[1] << ", " << data.min_bounds[2]
                      << "] to ["
                      << data.max_bounds[0] << ", " << data.max_bounds[1] << ", " << data.max_bounds[2]
                      << "]" << std::endl;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        error_ = "Error reading PLY file: " + std::string(e.what());
        return false;
    }
}

bool PlyReader::validateProperties(const std::vector<std::string>& property_names) {
    // Expected properties in order (nx, ny, nz are optional)
    const std::vector<std::string> expected = {
        "x", "y", "z",
        "f_dc_0", "f_dc_1", "f_dc_2"
    };
    
    // Add f_rest properties
    std::vector<std::string> all_expected = expected;
    for (int i = 0; i < 45; ++i) {
        all_expected.push_back("f_rest_" + std::to_string(i));
    }
    all_expected.push_back("opacity");
    all_expected.push_back("scale_0");
    all_expected.push_back("scale_1");
    all_expected.push_back("scale_2");
    all_expected.push_back("rot_0");
    all_expected.push_back("rot_1");
    all_expected.push_back("rot_2");
    all_expected.push_back("rot_3");
    
    // Check if all expected properties are present
    for (const auto& prop : all_expected) {
        if (std::find(property_names.begin(), property_names.end(), prop) == property_names.end()) {
            error_ = "Missing required property: " + prop;
            return false;
        }
    }
    
    return true;
}

} // namespace ply2gltf