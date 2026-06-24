#include "PlyReader.h"
#include "GltfWriter.h"
#include "MultiFramePlyReader.h"
#include "MPEGGltfWriter.h"
// #include "StreamingMPEGGltfWriter.h"  // Temporarily disabled
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <chrono>
#include <vector>
#include <algorithm>
#include <iterator>
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#include "rapidjson/document.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " input.ply output.glb [options]" << std::endl;
    std::cout << "   or: " << programName << " --sequence frame_*.ply output.glb [options]" << std::endl;
    std::cout << "   or: " << programName << " --multi-object output.glb [options]" << std::endl;
    std::cout << "   or: " << programName << " --manifest scene_3m.json output.glb [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --help               Show this help message" << std::endl;
    std::cout << "  -o, --output             Output file path (default: input filename with .glb extension)" << std::endl;
    std::cout << "  -f, --format             Output format: 'glb' or 'gltf' (default: glb)" << std::endl;
    std::cout << "  -p, --progressive        Organize data for progressive loading" << std::endl;
    std::cout << "  -v, --verbose            Enable verbose output" << std::endl;
    std::cout << "  -s, --stats              Display detailed file structure statistics" << std::endl;
    std::cout << "  --basic-pointcloud       Export as basic point cloud (POSITION + COLOR_0 only)" << std::endl;
    std::cout << "  --packed                 Use tightly packed attributes (default: packed)" << std::endl;
    std::cout << "  --viewpoint <file>       Load viewpoint (quat+cxyzd) and embed as camera node in GLB" << std::endl;
    std::cout << "  --manifest <file>        Package static multi-object, multi-layer scene from JSON manifest" << std::endl;
    std::cout << std::endl;
    std::cout << "Sequence options:" << std::endl;
    std::cout << "  --sequence               Process multiple PLY files as a sequence" << std::endl;
    std::cout << "  --fps                    Frame rate for sequence (default: 30)" << std::endl;
    std::cout << "  --use-chunks             Write frames 1..N as SPLT chunks in GLB (streamable)" << std::endl;
    std::cout << "  --streaming              Use memory-efficient streaming mode" << std::endl;
    std::cout << std::endl;
    std::cout << "Multi-object options:" << std::endl;
    std::cout << "  --multi-object           Process multiple PLY files as separate objects in one scene" << std::endl;
    std::cout << "  --object <name> <file>   Add an object with name and PLY file path" << std::endl;
    std::cout << "  --transform <tx> <ty> <tz> <rx> <ry> <rz> <sx> <sy> <sz>" << std::endl;
    std::cout << "                           Set transformation for the last added object" << std::endl;
    std::cout << "                           (translation xyz, rotation xyz in degrees, scale xyz)" << std::endl;
    std::cout << "  --object-drm <system>    Set DRM system for the last object (xor|widevine|playready)" << std::endl;
    std::cout << "                           Advanced: repeat to package multiple DRM systems for one object" << std::endl;
    std::cout << "  --object-drm-key <key>   Encryption key for the last object's most recent DRM system" << std::endl;
    std::cout << "  --object-drm-key-id <id> Key ID (hex string) for the last object's most recent DRM system" << std::endl;
    std::cout << "  --object-drm-encrypted-attributes <list>" << std::endl;
    std::cout << "                           Comma-separated list of attributes to encrypt for the last object" << std::endl;

    // ## MULTIDRM: DRM OPTIONS ##
    std::cout << std::endl;
    std::cout << "DRM options:" << std::endl;
    std::cout << "  --drm                      Enable DRM packaging" << std::endl;
    std::cout << "  --drm-key <key>            Key for XOR encryption (legacy, single DRM)" << std::endl;
    std::cout << "  --drm-key-id <hex_string>  16-byte Key ID for the JSON metadata (legacy)" << std::endl;
    std::cout << "  --drm-encrypted-attributes <list> Comma-separated list of attributes to encrypt (e.g., \"sh,scale\")" << std::endl;
    std::cout << std::endl;
    std::cout << "MultiDRM options:" << std::endl;
    std::cout << "  --drm-system <system>      DRM system: xor|widevine|playready" << std::endl;
    std::cout << "  --drm-scheme-uri <uri>     DRM scheme URI (e.g., Widevine UUID)" << std::endl;
    std::cout << "  --drm-key-id <hex>         Key ID (hex string, 16 bytes)" << std::endl;
    std::cout << "  --drm-key <key>            Encryption key" << std::endl;
    std::cout << "  --drm-pssh <base64>        PSSH box data (base64 encoded, optional)" << std::endl;
    std::cout << "  --drm-license-url <url>    License server URL (optional)" << std::endl;
    std::cout << "  Note: Use multiple --drm-system options to add multiple DRM systems" << std::endl;

    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << programName << " gaussian_splat.ply output.glb" << std::endl;
    std::cout << "  " << programName << " gaussian_splat.ply output.glb --progressive" << std::endl;
    std::cout << "  " << programName << " --sequence frame_*.ply output.glb --fps 30" << std::endl;
    std::cout << "  " << programName << " --sequence frame_001.ply frame_002.ply frame_003.ply output.glb" << std::endl;
}

bool parseArguments(int argc, char* argv[], ply2gltf::SequenceConversionOptions& options) {
    if (argc < 2) {
        return false;
    }
    
    // Check for help
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            return false;
        }
    }
    
    // Check if this is a sequence or multi-object conversion
    bool isSequence = false;
    bool isMultiObject = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--sequence") == 0) {
            isSequence = true;
            options.isSequence = true;
            break;
        }
        if (std::strcmp(argv[i], "--multi-object") == 0) {
            isMultiObject = true;
            options.isMultiObject = true;
            break;
        }
        if (std::strcmp(argv[i], "--manifest") == 0) {
            options.isLayered = true;
            break;
        }
    }
    
    // Parse arguments
    int i = 1;
    bool foundOutput = false;
    
    while (i < argc) {
        if (std::strcmp(argv[i], "--sequence") == 0) {
            // Already handled
            i++;
        }
        else if (std::strcmp(argv[i], "--multi-object") == 0) {
            // Already handled
            i++;
        }
        else if (std::strcmp(argv[i], "--manifest") == 0) {
            if (i + 1 < argc) {
                options.isLayered = true;
                options.manifest_file = argv[++i];
            } else {
                std::cerr << "Error: --manifest requires a file path" << std::endl;
                return false;
            }
            i++;
        }
        // ## MULTI-OBJECT: Parse object and transformation options ##
        else if (std::strcmp(argv[i], "--object") == 0) {
            if (i + 2 < argc) {
                std::string name = argv[++i];
                std::string file = argv[++i];
                options.objectFiles.push_back(file);
                ply2gltf::ObjectTransform transform;
                transform.name = name;
                options.objectTransforms.push_back(transform);
            } else {
                std::cerr << "Error: --object requires name and file path" << std::endl;
                return false;
            }
            i++;
        }
        else if (std::strcmp(argv[i], "--transform") == 0) {
            if (i + 9 < argc && !options.objectTransforms.empty()) {
                auto& transform = options.objectTransforms.back();
                transform.translation[0] = std::stof(argv[++i]);
                transform.translation[1] = std::stof(argv[++i]);
                transform.translation[2] = std::stof(argv[++i]);
                transform.rotation[0] = std::stof(argv[++i]);
                transform.rotation[1] = std::stof(argv[++i]);
                transform.rotation[2] = std::stof(argv[++i]);
                transform.scale[0] = std::stof(argv[++i]);
                transform.scale[1] = std::stof(argv[++i]);
                transform.scale[2] = std::stof(argv[++i]);
            } else {
                std::cerr << "Error: --transform requires 9 arguments (tx ty tz rx ry rz sx sy sz)" << std::endl;
                return false;
            }
            i++;
        }
        // ## MULTIDRM: Per-object DRM options ##
        else if (std::strcmp(argv[i], "--object-drm") == 0) {
            if (i + 1 < argc && !options.objectTransforms.empty()) {
                auto& transform = options.objectTransforms.back();
                std::string drmSystem = argv[++i];
                transform.drm_enabled = true;
                
                if (drmSystem == "widevine" || drmSystem == "WIDEVINE") {
                    transform.drm_config.system = ply2gltf::DRMSystem::WIDEVINE;
                    transform.drm_config.schemeIdUri = "urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd5121ed8";
                } else if (drmSystem == "playready" || drmSystem == "PLAYREADY") {
                    transform.drm_config.system = ply2gltf::DRMSystem::PLAYREADY;
                    transform.drm_config.schemeIdUri = "urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95";
                } else if (drmSystem == "xor" || drmSystem == "SIMPLE_XOR") {
                    transform.drm_config.system = ply2gltf::DRMSystem::SIMPLE_XOR;
                    transform.drm_config.schemeIdUri = "urn:exp:simple-xor-drm";
                } else {
                    std::cerr << "Error: Unknown DRM system: " << drmSystem << std::endl;
                    std::cerr << "Supported systems: xor, widevine, playready" << std::endl;
                    return false;
                }
                transform.drm_configs.push_back(transform.drm_config);
            } else {
                std::cerr << "Error: --object-drm requires an argument and must be used after --object" << std::endl;
                return false;
            }
            i++;
        }
        else if (std::strcmp(argv[i], "--object-drm-key") == 0) {
            if (i + 1 < argc && !options.objectTransforms.empty()) {
                std::string key = argv[++i];
                auto& transform = options.objectTransforms.back();
                transform.drm_config.key = key;
                if (transform.drm_configs.empty()) {
                    transform.drm_configs.push_back(transform.drm_config);
                }
                transform.drm_configs.back().key = key;
            } else {
                std::cerr << "Error: --object-drm-key requires an argument" << std::endl;
                return false;
            }
            i++;
        }
        else if (std::strcmp(argv[i], "--object-drm-key-id") == 0) {
            if (i + 1 < argc && !options.objectTransforms.empty()) {
                std::string keyId = argv[++i];
                auto& transform = options.objectTransforms.back();
                transform.drm_config.keyId = keyId;
                if (transform.drm_configs.empty()) {
                    transform.drm_configs.push_back(transform.drm_config);
                }
                transform.drm_configs.back().keyId = keyId;
            } else {
                std::cerr << "Error: --object-drm-key-id requires an argument" << std::endl;
                return false;
            }
            i++;
        }
        else if (std::strcmp(argv[i], "--object-drm-encrypted-attributes") == 0) {
            if (i + 1 < argc && !options.objectTransforms.empty()) {
                options.objectTransforms.back().drm_encrypted_attributes = argv[++i];
            } else {
                std::cerr << "Error: --object-drm-encrypted-attributes requires an argument" << std::endl;
                return false;
            }
            i++;
        }
        else if (std::strcmp(argv[i], "-o") == 0 || std::strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) {
                options.output_file = argv[++i];
                foundOutput = true;
            } else {
                std::cerr << "Error: --output requires an argument" << std::endl;
                return false;
            }
            i++;
        }
        else if (std::strcmp(argv[i], "-f") == 0 || std::strcmp(argv[i], "--format") == 0) {
            if (i + 1 < argc) {
                std::string format = argv[++i];
                if (format == "glb") {
                    options.binary = true;
                } else if (format == "gltf") {
                    options.binary = false;
                } else {
                    std::cerr << "Error: Unknown format '" << format << "'. Use 'glb' or 'gltf'." << std::endl;
                    return false;
                }
            } else {
                std::cerr << "Error: --format requires an argument" << std::endl;
                return false;
            }
            i++;
        }
        else if (std::strcmp(argv[i], "-p") == 0 || std::strcmp(argv[i], "--progressive") == 0) {
            options.progressive = true;
            i++;
        }
        else if (std::strcmp(argv[i], "-v") == 0 || std::strcmp(argv[i], "--verbose") == 0) {
            options.verbose = true;
            i++;
        }
        else if (std::strcmp(argv[i], "-s") == 0 || std::strcmp(argv[i], "--stats") == 0) {
            options.stats = true;
            i++;
        }
        else if (std::strcmp(argv[i], "--fps") == 0) {
            if (i + 1 < argc) {
                options.frameRate = std::stof(argv[++i]);
            } else {
                std::cerr << "Error: --fps requires an argument" << std::endl;
                return false;
            }
            i++;
        }
        else if (std::strcmp(argv[i], "--streaming") == 0) {
            // options.streaming = true;  // Temporarily disabled
            std::cerr << "Streaming mode is temporarily disabled" << std::endl;
            return 1;
            i++;
        }
        else if (std::strcmp(argv[i], "--use-chunks") == 0) {
            options.useChunks = true;
            i++;
        }
        else if (std::strcmp(argv[i], "--basic-pointcloud") == 0) {
            options.basic_pointcloud = true;
            i++;
        }
        else if (std::strcmp(argv[i], "--packed") == 0) {
            options.packed = true;
            i++;
        }
        else if (std::strcmp(argv[i], "--viewpoint") == 0) {
            if (i + 1 < argc) {
                options.viewpoint_file = argv[++i];
            } else {
                std::cerr << "Error: --viewpoint requires a file path" << std::endl;
                return false;
            }
            i++;
        }
        // ## MULTIDRM: DRM OPTIONS ##
        else if (std::strcmp(argv[i], "--drm") == 0) {
            options.drm_enabled = true;
            i++;
        }
        // Legacy single DRM options (for backward compatibility)
        else if (std::strcmp(argv[i], "--drm-key") == 0) {
            if (i + 1 < argc) {
                std::string key = argv[++i];
                options.drm_key = key;  // Keep for backward compatibility
                // Apply to the most recent DRM config, or create a default one
                if (options.drm_configs.empty()) {
                    ply2gltf::DRMConfig defaultConfig;
                    defaultConfig.system = ply2gltf::DRMSystem::SIMPLE_XOR;
                    defaultConfig.schemeIdUri = "urn:exp:simple-xor-drm";
                    defaultConfig.key = key;
                    options.drm_configs.push_back(defaultConfig);
                } else {
                    options.drm_configs.back().key = key;
                }
            } else {
                std::cerr << "Error: --drm-key requires an argument" << std::endl;
                return false;
            }
            i++;
        }
        else if (std::strcmp(argv[i], "--drm-key-id") == 0) {
            if (i + 1 < argc) {
                std::string keyId = argv[++i];
                options.drm_key_id = keyId;  // Keep for backward compatibility
                // Apply to the most recent DRM config, or create a default one
                if (options.drm_configs.empty()) {
                    ply2gltf::DRMConfig defaultConfig;
                    defaultConfig.system = ply2gltf::DRMSystem::SIMPLE_XOR;
                    defaultConfig.schemeIdUri = "urn:exp:simple-xor-drm";
                    defaultConfig.keyId = keyId;
                    options.drm_configs.push_back(defaultConfig);
                } else {
                    options.drm_configs.back().keyId = keyId;
                }
            } else {
                std::cerr << "Error: --drm-key-id requires an argument" << std::endl;
                return false;
            }
            i++;
        }
        else if (std::strcmp(argv[i], "--drm-encrypted-attributes") == 0) {
            if (i + 1 < argc) {
                options.drm_encrypted_attributes = argv[++i];
            } else {
                std::cerr << "Error: --drm-encrypted-attributes requires an argument" << std::endl;
                return false;
            }
            i++;
        }
        // MultiDRM options
        else if (std::strcmp(argv[i], "--drm-system") == 0) {
            if (i + 1 < argc) {
                std::string systemStr = argv[++i];
                ply2gltf::DRMSystem system = ply2gltf::DRMSystem::SIMPLE_XOR;
                std::string schemeUri = "urn:exp:simple-xor-drm";
                
                if (systemStr == "xor" || systemStr == "SIMPLE_XOR") {
                    system = ply2gltf::DRMSystem::SIMPLE_XOR;
                    schemeUri = "urn:exp:simple-xor-drm";
                } else if (systemStr == "widevine" || systemStr == "WIDEVINE") {
                    system = ply2gltf::DRMSystem::WIDEVINE;
                    schemeUri = "urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd5121ed8";  // Widevine UUID
                } else if (systemStr == "playready" || systemStr == "PLAYREADY") {
                    system = ply2gltf::DRMSystem::PLAYREADY;
                    schemeUri = "urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95";  // PlayReady UUID
                } else {
                    std::cerr << "Error: Unknown DRM system: " << systemStr << std::endl;
                    std::cerr << "Supported systems: xor, widevine, playready" << std::endl;
                    return false;
                }
                
                // Create new DRM config
                ply2gltf::DRMConfig config;
                config.system = system;
                config.schemeIdUri = schemeUri;
                options.drm_configs.push_back(config);
                options.drm_enabled = true;
            } else {
                std::cerr << "Error: --drm-system requires an argument" << std::endl;
                return false;
            }
            i++;
        }
        else if (std::strcmp(argv[i], "--drm-scheme-uri") == 0) {
            if (i + 1 < argc) {
                if (options.drm_configs.empty()) {
                    std::cerr << "Error: --drm-scheme-uri must be used after --drm-system" << std::endl;
                    return false;
                }
                options.drm_configs.back().schemeIdUri = argv[++i];
            } else {
                std::cerr << "Error: --drm-scheme-uri requires an argument" << std::endl;
                return false;
            }
            i++;
        }
        else if (std::strcmp(argv[i], "--drm-pssh") == 0) {
            if (i + 1 < argc) {
                if (options.drm_configs.empty()) {
                    std::cerr << "Error: --drm-pssh must be used after --drm-system" << std::endl;
                    return false;
                }
                options.drm_configs.back().pssh = argv[++i];
            } else {
                std::cerr << "Error: --drm-pssh requires an argument" << std::endl;
                return false;
            }
            i++;
        }
        else if (std::strcmp(argv[i], "--drm-license-url") == 0) {
            if (i + 1 < argc) {
                if (options.drm_configs.empty()) {
                    std::cerr << "Error: --drm-license-url must be used after --drm-system" << std::endl;
                    return false;
                }
                options.drm_configs.back().licenseUrl = argv[++i];
            } else {
                std::cerr << "Error: --drm-license-url requires an argument" << std::endl;
                return false;
            }
            i++;
        }
        else if (argv[i][0] != '-') {
            // Input file or output file
            if (isSequence) {
                // Check if this looks like a PLY file
                std::string arg = argv[i];
                bool isPlyFile = (arg.size() > 4 && arg.substr(arg.size() - 4) == ".ply");
                
                if (isPlyFile) {
                    options.inputFiles.push_back(argv[i]);
                } else if (!foundOutput) {
                    // Non-PLY file is assumed to be output
                    options.output_file = argv[i];
                    foundOutput = true;
                }
            } else if (options.isLayered) {
                if (!foundOutput) {
                    options.output_file = argv[i];
                    foundOutput = true;
                }
            } else {
                if (options.input_file.empty()) {
                    options.input_file = argv[i];
                    options.inputFiles.push_back(argv[i]);
                } else if (!foundOutput) {
                    options.output_file = argv[i];
                    foundOutput = true;
                }
            }
            i++;
        }
        else {
            std::cerr << "Error: Unknown option: " << argv[i] << std::endl;
            return false;
        }
    }
    
    // Validate input
    if (!isSequence && !isMultiObject && !options.isLayered && options.input_file.empty()) {
        std::cerr << "Error: No input file specified" << std::endl;
        return false;
    }
    
    if (isSequence && options.inputFiles.empty()) {
        std::cerr << "Error: No input files specified for sequence" << std::endl;
        return false;
    }
    
    if (isMultiObject && options.objectFiles.empty()) {
        std::cerr << "Error: No objects specified for multi-object mode. Use --object <name> <file>" << std::endl;
        return false;
    }
    
    if (isMultiObject && options.objectFiles.size() != options.objectTransforms.size()) {
        std::cerr << "Error: Number of objects (" << options.objectFiles.size() 
                  << ") does not match number of transforms (" << options.objectTransforms.size() << ")" << std::endl;
        return false;
    }

    if (options.isLayered && options.manifest_file.empty()) {
        std::cerr << "Error: No manifest specified" << std::endl;
        return false;
    }
    
    // Set default output file if not specified
    if (options.output_file.empty()) {
        if (options.isLayered) {
            options.output_file = "layered_scene.glb";
        } else if (isSequence && !options.inputFiles.empty()) {
            options.output_file = "sequence.glb";
        } else if (isMultiObject && !options.objectFiles.empty()) {
            options.output_file = "multi_object.glb";
        } else if (!options.input_file.empty()) {
            options.output_file = options.input_file;
            size_t dotPos = options.output_file.find_last_of('.');
            if (dotPos != std::string::npos) {
                options.output_file = options.output_file.substr(0, dotPos);
            }
            options.output_file += ".glb";
        }
    }
    
    // Ensure output file has correct extension
    std::string expectedExt = options.binary ? ".glb" : ".gltf";
    if (options.output_file.size() < expectedExt.size() || 
        options.output_file.substr(options.output_file.size() - expectedExt.size()) != expectedExt) {
        // Remove any existing extension
        size_t dotPos = options.output_file.find_last_of('.');
        if (dotPos != std::string::npos) {
            options.output_file = options.output_file.substr(0, dotPos);
        }
        options.output_file += expectedExt;
    }
    
    return true;
}

int processSingleFile(const ply2gltf::ConversionOptions& options) {
    // Start timing
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Read PLY file
    ply2gltf::PlyReader reader;
    ply2gltf::GaussianSplatData data;
    
    std::cout << "Reading PLY file..." << std::endl;
    if (!reader.read(options.input_file, data, options.verbose)) {
        std::cerr << "Error reading PLY file: " << reader.getError() << std::endl;
        return 1;
    }
    
    auto read_time = std::chrono::high_resolution_clock::now();
    auto read_duration = std::chrono::duration_cast<std::chrono::milliseconds>(read_time - start_time);
    
    std::cout << "Read " << data.count() << " Gaussian splats in " << read_duration.count() << " ms" << std::endl;
    
    // Write glTF file
    ply2gltf::GltfWriter writer;
    
    std::cout << "Writing " << (options.binary ? "GLB" : "glTF") << " file..." << std::endl;
    if (!writer.write(options.output_file, data, options)) {
        std::cerr << "Error writing glTF file: " << writer.getError() << std::endl;
        return 1;
    }
    
    auto write_time = std::chrono::high_resolution_clock::now();
    auto write_duration = std::chrono::duration_cast<std::chrono::milliseconds>(write_time - read_time);
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(write_time - start_time);
    
    std::cout << "Written in " << write_duration.count() << " ms" << std::endl;
    std::cout << std::endl;
    std::cout << "Conversion completed successfully!" << std::endl;
    std::cout << "Total time: " << total_duration.count() << " ms" << std::endl;
    
    return 0;
}

int processMultiObject(const ply2gltf::SequenceConversionOptions& options);

int processSequence(const ply2gltf::SequenceConversionOptions& options) {
    // Check if we should use streaming approach
    // if (options.streaming) {  // Temporarily disabled
    //     // Use the new streaming approach
    //     ply2gltf::StreamingMPEGGltfWriter writer;
    //     
    //     std::cout << "Using streaming approach for sequence conversion..." << std::endl;
    //     if (!writer.writeSequenceStreaming(options.inputFiles, options.output_file, options)) {
    //         std::cerr << "Error writing glTF file: " << writer.getError() << std::endl;
    //         return 1;
    //     }
    //     
    //     return 0;
    // }
    
    // Original non-streaming approach (kept for backward compatibility)
    // Start timing
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Read PLY sequence
    ply2gltf::MultiFramePlyReader reader;
    ply2gltf::MultiFrameGaussianSplatData data;
    
    std::cout << "Reading PLY sequence..." << std::endl;
    
    // Check if we have a pattern or a list of files
    bool success = false;
    if (options.inputFiles.size() == 1 && 
        (options.inputFiles[0].find('*') != std::string::npos || 
         options.inputFiles[0].find('?') != std::string::npos)) {
        // Pattern-based loading
        success = reader.readPattern(options.inputFiles[0], data, options.verbose);
    } else {
        // List-based loading
        success = reader.readSequence(options.inputFiles, data, options.verbose);
    }
    
    if (!success) {
        std::cerr << "Error reading PLY sequence: " << reader.getError() << std::endl;
        return 1;
    }
    
    auto read_time = std::chrono::high_resolution_clock::now();
    auto read_duration = std::chrono::duration_cast<std::chrono::milliseconds>(read_time - start_time);
    
    std::cout << "Read " << data.frameCount << " frames with max " << data.maxSplatCount 
              << " splats in " << read_duration.count() << " ms" << std::endl;
    
    // Set frame rate
    data.frameRate = options.frameRate;
    
    // Write only the chunked GLB format (multiple chunks); enforce binary GLB
    if (!options.binary) {
        std::cerr << "Error: Chunked SPLT output requires GLB (binary) format. Use --format glb." << std::endl;
        return 1;
    }
    ply2gltf::MPEGGltfWriter writer;
    std::cout << "Writing GLB file with SPLT chunks (chunked streaming)..." << std::endl;
    if (!writer.writeSequenceChunked(options.output_file, data, options)) {
        std::cerr << "Error writing GLB file: " << writer.getError() << std::endl;
        return 1;
    }
    
    auto write_time = std::chrono::high_resolution_clock::now();
    auto write_duration = std::chrono::duration_cast<std::chrono::milliseconds>(write_time - read_time);
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(write_time - start_time);
    
    std::cout << "Written in " << write_duration.count() << " ms" << std::endl;
    std::cout << std::endl;
    std::cout << "Sequence conversion completed successfully!" << std::endl;
    std::cout << "Total time: " << total_duration.count() << " ms" << std::endl;
    std::cout << "Duration: " << data.duration << " seconds at " << data.frameRate << " fps" << std::endl;
    
    return 0;
}

int processMultiObject(const ply2gltf::SequenceConversionOptions& options) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Read all PLY files
    std::vector<ply2gltf::GaussianSplatData> objectsData;
    ply2gltf::PlyReader reader;
    
    std::cout << "Reading " << options.objectFiles.size() << " PLY files..." << std::endl;
    
    for (size_t i = 0; i < options.objectFiles.size(); ++i) {
        const auto& file = options.objectFiles[i];
        const auto& transform = options.objectTransforms[i];
        
        std::cout << "  [" << (i+1) << "/" << options.objectFiles.size() << "] " 
                  << transform.name << ": " << file << std::endl;
        
        ply2gltf::GaussianSplatData data;
        if (!reader.read(file, data, options.verbose)) {
            std::cerr << "Error reading PLY file " << file << ": " << reader.getError() << std::endl;
            return 1;
        }
        
        objectsData.push_back(std::move(data));
    }
    
    auto read_time = std::chrono::high_resolution_clock::now();
    auto read_duration = std::chrono::duration_cast<std::chrono::milliseconds>(read_time - start_time);
    
    std::cout << "Read " << objectsData.size() << " objects in " << read_duration.count() << " ms" << std::endl;
    
    // Write multi-object GLB
    ply2gltf::GltfWriter writer;
    std::cout << "Writing multi-object GLB file..." << std::endl;
    if (!writer.writeMultiObject(options.output_file, objectsData, options.objectTransforms, options)) {
        std::cerr << "Error writing GLB file: " << writer.getError() << std::endl;
        return 1;
    }
    
    auto write_time = std::chrono::high_resolution_clock::now();
    auto write_duration = std::chrono::duration_cast<std::chrono::milliseconds>(write_time - read_time);
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(write_time - start_time);
    
    std::cout << "Written in " << write_duration.count() << " ms" << std::endl;
    std::cout << std::endl;
    std::cout << "Multi-object conversion completed successfully!" << std::endl;
    std::cout << "Total time: " << total_duration.count() << " ms" << std::endl;
    
    return 0;
}

static bool readFloatArray3(const rapidjson::Value& obj, const char* name, std::array<float, 3>& out) {
    if (!obj.HasMember(name)) {
        return true;
    }
    const auto& arr = obj[name];
    if (!arr.IsArray() || arr.Size() != 3) {
        std::cerr << "Error: '" << name << "' must be an array of 3 numbers" << std::endl;
        return false;
    }
    for (rapidjson::SizeType i = 0; i < 3; ++i) {
        if (!arr[i].IsNumber()) {
            std::cerr << "Error: '" << name << "' must contain numbers" << std::endl;
            return false;
        }
        out[i] = arr[i].GetFloat();
    }
    return true;
}

static ply2gltf::DRMConfig makeDRMConfig(const std::string& system) {
    ply2gltf::DRMConfig config;
    if (system == "widevine" || system == "WIDEVINE") {
        config.system = ply2gltf::DRMSystem::WIDEVINE;
        config.schemeIdUri = "urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd5121ed8";
    } else if (system == "playready" || system == "PLAYREADY") {
        config.system = ply2gltf::DRMSystem::PLAYREADY;
        config.schemeIdUri = "urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95";
    } else {
        config.system = ply2gltf::DRMSystem::SIMPLE_XOR;
        config.schemeIdUri = "urn:exp:simple-xor-drm";
    }
    return config;
}

static bool parseDRMPolicy(const rapidjson::Value& obj,
                           bool& enabled,
                           std::vector<ply2gltf::DRMConfig>& configs,
                           std::string& encryptedAttributes) {
    enabled = false;
    configs.clear();
    encryptedAttributes.clear();
    if (!obj.HasMember("drmPolicy") || obj["drmPolicy"].IsNull()) {
        return true;
    }

    const auto& drm = obj["drmPolicy"];
    if (!drm.IsObject()) {
        std::cerr << "Error: drmPolicy must be an object or null" << std::endl;
        return false;
    }

    if (drm.HasMember("attributes")) {
        if (drm["attributes"].IsArray()) {
            for (rapidjson::SizeType i = 0; i < drm["attributes"].Size(); ++i) {
                if (!drm["attributes"][i].IsString()) {
                    std::cerr << "Error: drmPolicy.attributes must contain strings" << std::endl;
                    return false;
                }
                if (!encryptedAttributes.empty()) {
                    encryptedAttributes += ",";
                }
                encryptedAttributes += drm["attributes"][i].GetString();
            }
        } else if (drm["attributes"].IsString()) {
            encryptedAttributes = drm["attributes"].GetString();
        }
    }

    auto applyCommonFields = [&](ply2gltf::DRMConfig& config, const rapidjson::Value& source) {
        if (source.HasMember("schemeIdUri") && source["schemeIdUri"].IsString()) {
            config.schemeIdUri = source["schemeIdUri"].GetString();
        }
        if (source.HasMember("keyId") && source["keyId"].IsString()) {
            config.keyId = source["keyId"].GetString();
        }
        if (source.HasMember("key") && source["key"].IsString()) {
            config.key = source["key"].GetString();
        }
        if (source.HasMember("pssh") && source["pssh"].IsString()) {
            config.pssh = source["pssh"].GetString();
        }
        if (source.HasMember("licenseUrl") && source["licenseUrl"].IsString()) {
            config.licenseUrl = source["licenseUrl"].GetString();
        }
    };

    auto addConfig = [&](const rapidjson::Value& systemValue) -> bool {
        ply2gltf::DRMConfig config;
        if (systemValue.IsString()) {
            config = makeDRMConfig(systemValue.GetString());
            applyCommonFields(config, drm);
        } else if (systemValue.IsObject()) {
            if (!systemValue.HasMember("system") || !systemValue["system"].IsString()) {
                std::cerr << "Error: drmPolicy.systems object entries must include a string system field" << std::endl;
                return false;
            }
            config = makeDRMConfig(systemValue["system"].GetString());
            applyCommonFields(config, drm);
            applyCommonFields(config, systemValue);
        } else {
            std::cerr << "Error: drmPolicy.system or systems must contain strings or objects" << std::endl;
            return false;
        }
        configs.push_back(config);
        return true;
    };

    if (drm.HasMember("systems") && drm["systems"].IsArray()) {
        for (rapidjson::SizeType i = 0; i < drm["systems"].Size(); ++i) {
            if (!addConfig(drm["systems"][i])) {
                return false;
            }
        }
    } else if (drm.HasMember("system")) {
        if (!addConfig(drm["system"])) {
            return false;
        }
    }

    enabled = !configs.empty();
    return true;
}

static bool parseLayerManifest(const std::string& manifestPath, std::vector<ply2gltf::LayeredObjectInfo>& objects) {
    std::ifstream in(manifestPath);
    if (!in.is_open()) {
        std::cerr << "Error: Cannot open manifest: " << manifestPath << std::endl;
        return false;
    }
    std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    rapidjson::Document doc;
    doc.Parse(json.c_str());
    if (doc.HasParseError() || !doc.IsObject()) {
        std::cerr << "Error: Invalid JSON manifest" << std::endl;
        return false;
    }
    if (!doc.HasMember("objects") || !doc["objects"].IsArray()) {
        std::cerr << "Error: Manifest must contain objects[]" << std::endl;
        return false;
    }

    objects.clear();
    const auto& jsonObjects = doc["objects"];
    for (rapidjson::SizeType objIdx = 0; objIdx < jsonObjects.Size(); ++objIdx) {
        const auto& jsonObj = jsonObjects[objIdx];
        if (!jsonObj.IsObject()) {
            std::cerr << "Error: objects[] entries must be objects" << std::endl;
            return false;
        }

        ply2gltf::LayeredObjectInfo object;
        object.transform.name = jsonObj.HasMember("name") && jsonObj["name"].IsString()
            ? jsonObj["name"].GetString()
            : ("object_" + std::to_string(objIdx));

        if (jsonObj.HasMember("transform")) {
            const auto& tr = jsonObj["transform"];
            if (!tr.IsObject()) {
                std::cerr << "Error: transform must be an object" << std::endl;
                return false;
            }
            if (!readFloatArray3(tr, "translation", object.transform.translation) ||
                !readFloatArray3(tr, "rotation", object.transform.rotation) ||
                !readFloatArray3(tr, "scale", object.transform.scale)) {
                return false;
            }
        }

        if (!parseDRMPolicy(jsonObj, object.transform.drm_enabled,
                            object.transform.drm_configs,
                            object.transform.drm_encrypted_attributes)) {
            return false;
        }
        if (!object.transform.drm_configs.empty()) {
            object.transform.drm_config = object.transform.drm_configs.front();
        }

        if (!jsonObj.HasMember("layers") || !jsonObj["layers"].IsArray() || jsonObj["layers"].Empty()) {
            std::cerr << "Error: object '" << object.transform.name << "' must contain non-empty layers[]" << std::endl;
            return false;
        }
        const auto& layers = jsonObj["layers"];
        for (rapidjson::SizeType layerIdx = 0; layerIdx < layers.Size(); ++layerIdx) {
            const auto& jsonLayer = layers[layerIdx];
            if (!jsonLayer.IsObject() || !jsonLayer.HasMember("ply") || !jsonLayer["ply"].IsString()) {
                std::cerr << "Error: each layer must contain a string ply path" << std::endl;
                return false;
            }

            ply2gltf::LayerInfo layer;
            layer.id = jsonLayer.HasMember("id") && jsonLayer["id"].IsString()
                ? jsonLayer["id"].GetString()
                : (object.transform.name + "_layer_" + std::to_string(layerIdx));
            layer.role = jsonLayer.HasMember("role") && jsonLayer["role"].IsString()
                ? jsonLayer["role"].GetString()
                : (layerIdx == 0 ? "base" : "enhancement");
            layer.ply = jsonLayer["ply"].GetString();
            if (jsonLayer.HasMember("composition") && jsonLayer["composition"].IsString()) {
                layer.composition = jsonLayer["composition"].GetString();
            }
            if (jsonLayer.HasMember("dependsOn") && jsonLayer["dependsOn"].IsArray()) {
                for (rapidjson::SizeType i = 0; i < jsonLayer["dependsOn"].Size(); ++i) {
                    if (!jsonLayer["dependsOn"][i].IsString()) {
                        std::cerr << "Error: dependsOn must contain strings" << std::endl;
                        return false;
                    }
                    layer.dependsOn.push_back(jsonLayer["dependsOn"][i].GetString());
                }
            }

            if (!parseDRMPolicy(jsonLayer, layer.drm_enabled, layer.drm_configs, layer.drm_encrypted_attributes)) {
                return false;
            }
            layer.has_drm_policy = jsonLayer.HasMember("drmPolicy");
            object.layers.push_back(layer);
        }
        objects.push_back(object);
    }

    return true;
}

int processLayeredManifest(const ply2gltf::SequenceConversionOptions& options) {
    auto start_time = std::chrono::high_resolution_clock::now();
    std::vector<ply2gltf::LayeredObjectInfo> objects;
    if (!parseLayerManifest(options.manifest_file, objects)) {
        return 1;
    }

    std::vector<std::vector<ply2gltf::GaussianSplatData>> layersData;
    ply2gltf::PlyReader reader;

    std::cout << "Reading layered scene manifest: " << options.manifest_file << std::endl;
    for (const auto& object : objects) {
        std::vector<ply2gltf::GaussianSplatData> objectLayers;
        std::cout << "  Object " << object.transform.name << " (" << object.layers.size() << " layers)" << std::endl;
        for (const auto& layer : object.layers) {
            std::cout << "    - " << layer.id << " [" << layer.role << "]: " << layer.ply << std::endl;
            ply2gltf::GaussianSplatData data;
            if (!reader.read(layer.ply, data, options.verbose)) {
                std::cerr << "Error reading PLY file " << layer.ply << ": " << reader.getError() << std::endl;
                return 1;
            }
            objectLayers.push_back(std::move(data));
        }
        layersData.push_back(std::move(objectLayers));
    }

    auto read_time = std::chrono::high_resolution_clock::now();
    auto read_duration = std::chrono::duration_cast<std::chrono::milliseconds>(read_time - start_time);
    std::cout << "Read layered scene in " << read_duration.count() << " ms" << std::endl;

    ply2gltf::GltfWriter writer;
    std::cout << "Writing static multi-layer GLB file..." << std::endl;
    if (!writer.writeLayeredObjects(options.output_file, layersData, objects, options)) {
        std::cerr << "Error writing GLB file: " << writer.getError() << std::endl;
        return 1;
    }

    auto write_time = std::chrono::high_resolution_clock::now();
    auto write_duration = std::chrono::duration_cast<std::chrono::milliseconds>(write_time - read_time);
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(write_time - start_time);

    std::cout << "Written in " << write_duration.count() << " ms" << std::endl;
    std::cout << "Static multi-layer conversion completed successfully!" << std::endl;
    std::cout << "Total time: " << total_duration.count() << " ms" << std::endl;
    return 0;
}

int main(int argc, char* argv[]) {
    ply2gltf::SequenceConversionOptions options;
    
    if (!parseArguments(argc, argv, options)) {
        printUsage(argv[0]);
        return 1;
    }
    
    // Print conversion info
    std::cout << "PLY to glTF Gaussian Splat Converter" << std::endl;
    std::cout << "=====================================" << std::endl;
    
    if (options.isLayered) {
        std::cout << "Mode:   Static multi-layer scene manifest" << std::endl;
        std::cout << "Manifest: " << options.manifest_file << std::endl;
    } else if (options.isSequence) {
        std::cout << "Mode:   Sequence conversion with MPEG extensions" << std::endl;
        std::cout << "Inputs: " << options.inputFiles.size() << " files" << std::endl;
        if (options.inputFiles.size() <= 5) {
            for (const auto& file : options.inputFiles) {
                std::cout << "  - " << file << std::endl;
            }
        } else {
            std::cout << "  - " << options.inputFiles[0] << std::endl;
            std::cout << "  - ..." << std::endl;
            std::cout << "  - " << options.inputFiles.back() << std::endl;
        }
    } else if (options.isMultiObject) {
        std::cout << "Mode:   Multi-object scene" << std::endl;
        std::cout << "Objects: " << options.objectFiles.size() << std::endl;
        for (size_t i = 0; i < options.objectFiles.size(); ++i) {
            const auto& transform = options.objectTransforms[i];
            std::cout << "  - " << transform.name << ": " << options.objectFiles[i] << std::endl;
            std::cout << "    Translation: (" << transform.translation[0] << ", " 
                      << transform.translation[1] << ", " << transform.translation[2] << ")" << std::endl;
            std::cout << "    Rotation: (" << transform.rotation[0] << ", " 
                      << transform.rotation[1] << ", " << transform.rotation[2] << ")" << std::endl;
            std::cout << "    Scale: (" << transform.scale[0] << ", " 
                      << transform.scale[1] << ", " << transform.scale[2] << ")" << std::endl;
        }
    } else {
        std::cout << "Input:  " << options.input_file << std::endl;
    }
    
    std::cout << "Output: " << options.output_file << std::endl;
    std::cout << "Format: " << (options.binary ? "GLB (binary)" : "glTF (JSON + binary)") << std::endl;
    
    if (options.progressive) {
        std::cout << "Mode:   Progressive loading" << std::endl;
    }
    if (options.streaming) {
        std::cout << "Mode:   Streaming optimized" << std::endl;
    }
    
    std::cout << std::endl;
    
    // Process based on mode
    int result;
    if (options.isLayered) {
        result = processLayeredManifest(options);
    } else if (options.isSequence) {
        result = processSequence(options);
    } else if (options.isMultiObject) {
        result = processMultiObject(options);
    } else {
        result = processSingleFile(options);
    }
    
    // Print file size info
    if (result == 0) {
        std::ifstream output_file(options.output_file, std::ios::binary | std::ios::ate);
        if (output_file.is_open()) {
            auto file_size = output_file.tellg();
            std::cout << "Output file size: " << (file_size / 1024.0 / 1024.0) << " MB" << std::endl;
            output_file.close();
        }
    }
    
    return result;
}
