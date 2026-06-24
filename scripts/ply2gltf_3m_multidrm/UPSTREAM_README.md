# PLY to glTF Gaussian Splat Converter

A C++ tool for converting PLY Gaussian Splat files to glTF 2.0 format with the EXT_gaussian_splats extension. Now with support for multi-frame sequences using MPEG-I Scene Description extensions.

## Overview

This tool converts 3D Gaussian Splat data from the INRIA PLY format to glTF 2.0 format, following the EXT_gaussian_splats extension specification. The converter preserves all Gaussian splat attributes including position, color, opacity, scale, rotation, and spherical harmonics coefficients.

## Features

### Core Features
- Converts PLY Gaussian splat files to glTF/GLB format
- Full support for EXT_gaussian_splats extension
- Preserves all spherical harmonics coefficients (up to 3rd order)
- Supports both ASCII and binary PLY input
- Generates compact binary GLB output
- Progressive data organization for streaming

### New: Multi-Frame Sequence Support
- Convert multiple PLY files into a single glTF with temporal data
- MPEG-I Scene Description extensions (MPEG_media, MPEG_buffer_circular, MPEG_accessor_timed)
- Circular buffers for efficient streaming of temporal data
- Per-attribute buffer management for optimized delivery
- Frame padding for sequences with varying splat counts
- Natural filename sorting for proper frame ordering
- **NEW**: Streaming mode for memory-efficient processing of large sequences

## Building

### Requirements

- CMake 3.16 or higher
- C++17 compatible compiler
- RapidJSON (automatically downloaded)
- TinyPLY (automatically downloaded)

### Build Instructions

```bash
mkdir build
cd build
cmake ..
make
```

Or use the provided build script:
```bash
./build.sh
```

The executable will be created in `build/bin/ply2gltf`

## Usage

### Single File Conversion
```bash
./ply2gltf input.ply output.glb [options]
```

### Sequence Conversion
```bash
./ply2gltf --sequence frame_*.ply output.glb [options]
# or
./ply2gltf --sequence frame1.ply frame2.ply frame3.ply output.glb [options]
```

### Options

#### Basic Options
- `-h, --help`: Show help message
- `-o, --output`: Output file path (default: input filename with .glb extension)
- `-f, --format`: Output format: "glb" or "gltf" (default: glb)
- `-p, --progressive`: Organize data for progressive loading
- `-v, --verbose`: Enable verbose output

#### Sequence Options
- `--sequence`: Process multiple PLY files as a sequence
- `--fps`: Frame rate for sequence (default: 30)
- `--streaming`: Use memory-efficient streaming mode (processes frames one at a time)

### Examples

#### Single File Examples

Convert a PLY file to GLB:
```bash
./ply2gltf gaussian_splat.ply output.glb
```

Convert with progressive data organization:
```bash
./ply2gltf gaussian_splat.ply output.glb --progressive
```

Convert to glTF (separate JSON and binary):
```bash
./ply2gltf gaussian_splat.ply output.gltf --format gltf
```

#### Sequence Examples

Convert a sequence using wildcards:
```bash
./ply2gltf --sequence frame_*.ply animation.glb --fps 30
```

Convert specific frames:
```bash
./ply2gltf --sequence frame001.ply frame002.ply frame003.ply output.glb
```

Optimize for streaming:
```bash
./ply2gltf --sequence frame_*.ply stream.glb --streaming
```

Progressive sequence with high frame rate:
```bash
./ply2gltf --sequence capture_*.ply output.glb \
    --progressive \
    --fps 60
```

Memory-efficient streaming for large sequences:
```bash
./ply2gltf --sequence frame_*.ply large_sequence.glb \
    --streaming \
    --fps 30 \
    --verbose
```

## PLY Input Format

The tool expects PLY files with the following properties in order:

1. **Position**: x, y, z (float)
2. **Normals**: nx, ny, nz (float) - ignored but must be present
3. **Spherical Harmonics DC**: f_dc_0, f_dc_1, f_dc_2 (float)
4. **Spherical Harmonics Rest**: f_rest_0 through f_rest_44 (float)
5. **Opacity**: opacity (float)
6. **Scale**: scale_0, scale_1, scale_2 (float)
7. **Rotation**: rot_0, rot_1, rot_2, rot_3 (float)

## glTF Output Format

### Single File Output

The tool generates glTF 2.0 files with:

- **Primitive mode**: POINTS (0)
- **Required attributes**:
  - POSITION: Vec3 (x, y, z)
  - COLOR_0: Vec4 (RGB from f_dc_*, alpha from opacity)
- **Extension attributes** (EXT_gaussian_splats):
  - _GS_ORIENTATION: Vec4 (quaternion)
  - _GS_SCALE: Vec3 (log scale)
  - _GS_SH_COEFF_FIRST: Custom[9] (1st order SH)
  - _GS_SH_COEFF_SECOND: Custom[15] (2nd order SH)
  - _GS_SH_COEFF_THIRD: Custom[21] (3rd order SH)

### Sequence Output

For sequences, the tool additionally uses:

- **MPEG Extensions**:
  - `MPEG_media`: References the embedded sequence data
  - `MPEG_buffer_circular`: Circular buffers for each attribute
  - `MPEG_accessor_timed`: Dynamic accessors for time-varying data
- **Separate Buffers**: Each attribute stored in its own circular buffer
- **Frame Headers**: Metadata for each frame including timestamp and splat count

## Data Organization

### Standard Mode

All attributes are stored sequentially in the buffer for each splat.

### Progressive Mode

Data is organized in separate buffer views by significance:
1. Base level: POSITION + COLOR_0
2. Transform level: _GS_ORIENTATION + _GS_SCALE
3. SH Level 1: _GS_SH_COEFF_FIRST
4. SH Level 2: _GS_SH_COEFF_SECOND
5. SH Level 3: _GS_SH_COEFF_THIRD

This organization enables progressive streaming and rendering.

### Sequence Mode (with MPEG Extensions)

#### Buffer Structure
Each attribute buffer stores all N frames sequentially:
```
[Frame 0][Frame 1][Frame 2]...[Frame N-1]
```

#### Frame Layout
Each frame includes:
- Frame header (16 bytes): frame index, splat count, timestamp
- Attribute data padded to maximum splat count

#### Attribute Buffers
1. **Position Buffer**: 3D positions (Vec3)
2. **Color Buffer**: RGB + opacity (Vec4)
3. **Orientation Buffer**: Rotation quaternions (Vec4)
4. **Scale Buffer**: Scale factors in log space (Vec3)
5. **SH Coefficient Buffers**: Separate buffers for each SH level

## Implementation Status

This converter is fully implemented with the following features:

### Core Features
- ✅ PLY file reading with TinyPLY
- ✅ glTF/GLB writing with RapidJSON
- ✅ EXT_gaussian_splats extension support
- ✅ Progressive data organization
- ✅ Command-line interface
- ✅ Both GLB (binary) and glTF (JSON+binary) output
- ✅ Proper data transformations (opacity, scale, rotation)
- ✅ Spherical harmonics reorganization

### Sequence Features
- ✅ Multi-frame PLY sequence loading
- ✅ MPEG-I Scene Description extensions
- ✅ Circular buffer implementation
- ✅ Per-attribute buffer management
- ✅ Natural filename sorting
- ✅ Frame padding for variable splat counts
- ✅ Parallel PLY loading for performance
- ✅ Backward compatibility with single-file conversion
- ✅ **Streaming mode**: Process frames one-by-one for memory efficiency

## Implementation Details

### Data Transformations

- **Opacity**: Converted from logit space to [0,1] range: `opacity = 1 / (1 + exp(-logit_opacity))`
- **Scale**: Stored in log space (as in PLY format)
- **Rotation**: Stored as normalized quaternion
- **Color**: DC spherical harmonics coefficients converted to RGB: `RGB = 0.5 + 0.28209479 * f_dc`
- **SH Coefficients**: Stored per-channel in separate accessors (15 values per channel)

## Parser Implementation Guide

This section provides detailed instructions on how to implement a parser for the glTF files generated by this converter.

### glTF Structure Overview

The generated glTF files use the following structure:
- **Main attributes**: `POSITION` and `COLOR_0` (standard glTF)
- **Extension attributes**: All Gaussian splat-specific data in `EXT_gaussian_splats` extension
- **Buffer layout**: Tightly packed data with separate buffer views for each attribute

### Step-by-Step Parsing Instructions

#### 1. Check for Extension Support

```javascript
// Check if the file uses the Gaussian splats extension
if (!gltf.extensionsUsed || !gltf.extensionsUsed.includes('EXT_gaussian_splats')) {
    throw new Error('This is not a Gaussian splat file');
}
```

#### 2. Access the Primitive

```javascript
// Get the first primitive (Gaussian splats are stored as POINTS)
const mesh = gltf.meshes[0];
const primitive = mesh.primitives[0];

// Verify it's a point cloud
if (primitive.mode !== 0) { // 0 = POINTS
    throw new Error('Expected POINTS primitive');
}
```

#### 3. Parse Main Attributes

```javascript
// Get main attributes
const positionAccessorIndex = primitive.attributes.POSITION;
const colorAccessorIndex = primitive.attributes.COLOR_0;

// Get accessors
const positionAccessor = gltf.accessors[positionAccessorIndex];
const colorAccessor = gltf.accessors[colorAccessorIndex];

// Verify types
console.assert(positionAccessor.type === 'VEC3');
console.assert(colorAccessor.type === 'VEC4');
```

#### 4. Parse Extension Attributes

```javascript
// Get extension attributes
const gsExtension = primitive.extensions?.EXT_gaussian_splats;
if (!gsExtension) {
    throw new Error('Missing EXT_gaussian_splats extension');
}

const gsAttributes = gsExtension.attributes;

// Get accessor indices
const orientationAccessorIndex = gsAttributes._GS_ORIENTATION;
const scaleAccessorIndex = gsAttributes._GS_SCALE;
const shRAccessorIndex = gsAttributes._GS_SH_COEFF_R;
const shGAccessorIndex = gsAttributes._GS_SH_COEFF_G;
const shBAccessorIndex = gsAttributes._GS_SH_COEFF_B;
```

#### 5. Read Buffer Data

```javascript
// Function to read data from an accessor
function readAccessorData(gltf, accessorIndex, bufferData) {
    const accessor = gltf.accessors[accessorIndex];
    const bufferView = gltf.bufferViews[accessor.bufferView];
    
    // Calculate byte offsets
    const byteOffset = (bufferView.byteOffset || 0) + (accessor.byteOffset || 0);
    const byteStride = bufferView.byteStride || 0;
    
    // Component sizes
    const componentSizes = {
        5126: 4, // FLOAT
        5125: 4, // UNSIGNED_INT
        5123: 2, // UNSIGNED_SHORT
    };
    
    const componentSize = componentSizes[accessor.componentType];
    const componentsPerElement = {
        'SCALAR': 1,
        'VEC2': 2,
        'VEC3': 3,
        'VEC4': 4,
        'MAT2': 4,
        'MAT3': 9,
        'MAT4': 16
    }[accessor.type];
    
    // Create typed array view
    const TypedArray = {
        5126: Float32Array,
        5125: Uint32Array,
        5123: Uint16Array,
    }[accessor.componentType];
    
    const elementCount = accessor.count;
    const result = new TypedArray(elementCount * componentsPerElement);
    
    // Read data - handle both packed and strided data
    if (byteStride && byteStride !== componentSize * componentsPerElement) {
        // Strided data
        for (let i = 0; i < elementCount; i++) {
            const elementOffset = byteOffset + i * byteStride;
            const elementData = new TypedArray(
                bufferData,
                elementOffset,
                componentsPerElement
            );
            result.set(elementData, i * componentsPerElement);
        }
    } else {
        // Packed data
        const dataView = new TypedArray(
            bufferData,
            byteOffset,
            elementCount * componentsPerElement
        );
        result.set(dataView);
    }
    
    return result;
}
```

#### 6. Parse Gaussian Splat Data

```javascript
// Load the binary buffer (GLB or external .bin file)
const bufferData = await loadBinaryBuffer(gltf.buffers[0].uri);

// Read all attributes
const positions = readAccessorData(gltf, positionAccessorIndex, bufferData);
const colors = readAccessorData(gltf, colorAccessorIndex, bufferData);
const orientations = readAccessorData(gltf, orientationAccessorIndex, bufferData);
const scales = readAccessorData(gltf, scaleAccessorIndex, bufferData);
const shCoeffsR = readAccessorData(gltf, shRAccessorIndex, bufferData);
const shCoeffsG = readAccessorData(gltf, shGAccessorIndex, bufferData);
const shCoeffsB = readAccessorData(gltf, shBAccessorIndex, bufferData);

// Get splat count
const splatCount = positionAccessor.count;
```

#### 7. Reconstruct Splat Data

```javascript
// Create splat array
const splats = [];

for (let i = 0; i < splatCount; i++) {
    const splat = {
        // Position (x, y, z)
        position: [
            positions[i * 3 + 0],
            positions[i * 3 + 1],
            positions[i * 3 + 2]
        ],
        
        // Color (RGBA) - already in [0,1] range
        color: [
            colors[i * 4 + 0],
            colors[i * 4 + 1],
            colors[i * 4 + 2]
        ],
        opacity: colors[i * 4 + 3],
        
        // Orientation quaternion (x, y, z, w)
        rotation: [
            orientations[i * 4 + 0],
            orientations[i * 4 + 1],
            orientations[i * 4 + 2],
            orientations[i * 4 + 3]
        ],
        
        // Scale in log space
        scale: [
            scales[i * 3 + 0],
            scales[i * 3 + 1],
            scales[i * 3 + 2]
        ],
        
        // Spherical harmonics coefficients (15 per channel)
        sh_coeffs: {
            r: shCoeffsR.slice(i * 15, (i + 1) * 15),
            g: shCoeffsG.slice(i * 15, (i + 1) * 15),
            b: shCoeffsB.slice(i * 15, (i + 1) * 15)
        }
    };
    
    splats.push(splat);
}
```

### Buffer Layout Details

#### Packed Mode (Default)

Each attribute is stored in its own tightly packed buffer view:

```
Position Buffer:    [x0,y0,z0, x1,y1,z1, x2,y2,z2, ...]
Color Buffer:       [r0,g0,b0,a0, r1,g1,b1,a1, ...]
Orientation Buffer: [x0,y0,z0,w0, x1,y1,z1,w1, ...]
Scale Buffer:       [x0,y0,z0, x1,y1,z1, x2,y2,z2, ...]
SH Red Buffer:      [r0_0...r0_14, r1_0...r1_14, ...]
SH Green Buffer:    [g0_0...g0_14, g1_0...g1_14, ...]
SH Blue Buffer:     [b0_0...b0_14, b1_0...b1_14, ...]
```

#### Progressive Mode

When using `--progressive`, data is organized in levels:

1. **Level 0**: Position + Color (base rendering)
2. **Level 1**: Orientation + Scale (3D Gaussians)
3. **Level 2**: SH coefficients (advanced lighting)

### Rendering Considerations

#### Converting from SH to RGB

The color stored in COLOR_0 is already converted from spherical harmonics DC terms:
```javascript
// No conversion needed - color is already in RGB space
const rgb = [colors[i*4], colors[i*4+1], colors[i*4+2]];
```

#### Scale Conversion

Scales are stored in log space. To get actual scale:
```javascript
const actualScale = scale.map(s => Math.exp(s));
```

#### Opacity

Opacity is already in [0,1] range (converted from logit during export).

### Example: Complete Parser

```javascript
class GaussianSplatParser {
    async parse(gltfPath) {
        // Load glTF
        const gltf = await this.loadJSON(gltfPath);
        
        // Validate
        if (!gltf.extensionsUsed?.includes('EXT_gaussian_splats')) {
            throw new Error('Not a Gaussian splat file');
        }
        
        // Get primitive
        const primitive = gltf.meshes[0].primitives[0];
        const gsExt = primitive.extensions.EXT_gaussian_splats;
        
        // Load binary data
        const bufferUri = gltf.buffers[0].uri;
        const bufferData = await this.loadBinary(
            gltfPath.replace('.gltf', '.bin')
        );
        
        // Parse attributes
        const data = {
            positions: this.readAccessor(gltf, primitive.attributes.POSITION, bufferData),
            colors: this.readAccessor(gltf, primitive.attributes.COLOR_0, bufferData),
            orientations: this.readAccessor(gltf, gsExt.attributes._GS_ORIENTATION, bufferData),
            scales: this.readAccessor(gltf, gsExt.attributes._GS_SCALE, bufferData),
            shR: this.readAccessor(gltf, gsExt.attributes._GS_SH_COEFF_R, bufferData),
            shG: this.readAccessor(gltf, gsExt.attributes._GS_SH_COEFF_G, bufferData),
            shB: this.readAccessor(gltf, gsExt.attributes._GS_SH_COEFF_B, bufferData)
        };
        
        return this.reconstructSplats(data, gltf.accessors[primitive.attributes.POSITION].count);
    }
}
```

### Performance Tips

1. **Use typed arrays**: Process data directly as typed arrays when possible
2. **Batch operations**: Process multiple splats at once for better performance
3. **Progressive loading**: Load base attributes first, then enhancement data
4. **Memory management**: For large files, consider streaming or chunked processing

### C++ Parser Example

```cpp
#include <rapidjson/document.h>
#include <fstream>
#include <vector>

struct GaussianSplat {
    float position[3];
    float color[4];      // RGBA
    float rotation[4];   // Quaternion
    float scale[3];      // Log scale
    float sh_r[15];      // Red SH coefficients
    float sh_g[15];      // Green SH coefficients  
    float sh_b[15];      // Blue SH coefficients
};

class GaussianSplatParser {
public:
    std::vector<GaussianSplat> parse(const std::string& gltfPath) {
        // Load JSON
        rapidjson::Document doc;
        std::ifstream file(gltfPath);
        std::string json((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
        doc.Parse(json.c_str());
        
        // Verify extension
        auto& extensionsUsed = doc["extensionsUsed"];
        bool hasExtension = false;
        for (auto& ext : extensionsUsed.GetArray()) {
            if (std::string(ext.GetString()) == "EXT_gaussian_splats") {
                hasExtension = true;
                break;
            }
        }
        if (!hasExtension) {
            throw std::runtime_error("Missing EXT_gaussian_splats extension");
        }
        
        // Get primitive
        auto& primitive = doc["meshes"][0]["primitives"][0];
        auto& attributes = primitive["attributes"];
        auto& gsExt = primitive["extensions"]["EXT_gaussian_splats"];
        auto& gsAttributes = gsExt["attributes"];
        
        // Get accessor indices
        int posIdx = attributes["POSITION"].GetInt();
        int colorIdx = attributes["COLOR_0"].GetInt();
        int orientIdx = gsAttributes["_GS_ORIENTATION"].GetInt();
        int scaleIdx = gsAttributes["_GS_SCALE"].GetInt();
        int shRIdx = gsAttributes["_GS_SH_COEFF_R"].GetInt();
        int shGIdx = gsAttributes["_GS_SH_COEFF_G"].GetInt();
        int shBIdx = gsAttributes["_GS_SH_COEFF_B"].GetInt();
        
        // Load binary buffer
        std::string binPath = gltfPath.substr(0, gltfPath.find_last_of('.')) + ".bin";
        std::vector<uint8_t> bufferData = loadBinaryFile(binPath);
        
        // Get splat count
        int splatCount = doc["accessors"][posIdx]["count"].GetInt();
        
        // Parse splats
        std::vector<GaussianSplat> splats(splatCount);
        
        // Read each attribute
        readAttribute(doc, posIdx, bufferData, splats, 
                     offsetof(GaussianSplat, position), 3);
        readAttribute(doc, colorIdx, bufferData, splats,
                     offsetof(GaussianSplat, color), 4);
        readAttribute(doc, orientIdx, bufferData, splats,
                     offsetof(GaussianSplat, rotation), 4);
        readAttribute(doc, scaleIdx, bufferData, splats,
                     offsetof(GaussianSplat, scale), 3);
        readAttribute(doc, shRIdx, bufferData, splats,
                     offsetof(GaussianSplat, sh_r), 15);
        readAttribute(doc, shGIdx, bufferData, splats,
                     offsetof(GaussianSplat, sh_g), 15);
        readAttribute(doc, shBIdx, bufferData, splats,
                     offsetof(GaussianSplat, sh_b), 15);
        
        return splats;
    }
    
private:
    void readAttribute(const rapidjson::Document& doc,
                      int accessorIdx,
                      const std::vector<uint8_t>& bufferData,
                      std::vector<GaussianSplat>& splats,
                      size_t memberOffset,
                      int componentCount) {
        auto& accessor = doc["accessors"][accessorIdx];
        auto& bufferView = doc["bufferViews"][accessor["bufferView"].GetInt()];
        
        size_t byteOffset = bufferView["byteOffset"].GetInt64();
        if (accessor.HasMember("byteOffset")) {
            byteOffset += accessor["byteOffset"].GetInt64();
        }
        
        const float* data = reinterpret_cast<const float*>(
            bufferData.data() + byteOffset
        );
        
        // Copy data to splats
        for (size_t i = 0; i < splats.size(); i++) {
            float* dest = reinterpret_cast<float*>(
                reinterpret_cast<uint8_t*>(&splats[i]) + memberOffset
            );
            std::memcpy(dest, data + i * componentCount, 
                       componentCount * sizeof(float));
        }
    }
};
```

### Validation and Error Handling

When implementing a parser, ensure proper validation:

```javascript
// Validate accessor types and counts
function validateAccessor(accessor, expectedType, expectedComponentType) {
    if (accessor.type !== expectedType) {
        throw new Error(`Expected ${expectedType}, got ${accessor.type}`);
    }
    if (accessor.componentType !== expectedComponentType) {
        throw new Error(`Expected component type ${expectedComponentType}`);
    }
}

// Validate buffer view bounds
function validateBufferView(bufferView, bufferSize) {
    const end = (bufferView.byteOffset || 0) + bufferView.byteLength;
    if (end > bufferSize) {
        throw new Error('Buffer view exceeds buffer bounds');
    }
}

// Validate splat data
function validateSplatData(splat) {
    // Check quaternion normalization
    const quatMag = Math.sqrt(
        splat.rotation[0]**2 + splat.rotation[1]**2 + 
        splat.rotation[2]**2 + splat.rotation[3]**2
    );
    if (Math.abs(quatMag - 1.0) > 0.01) {
        console.warn('Quaternion not normalized:', quatMag);
    }
    
    // Check opacity range
    if (splat.opacity < 0 || splat.opacity > 1) {
        console.warn('Opacity out of range:', splat.opacity);
    }
}
```

### Memory-Mapped File Support

For large files, consider using memory-mapped I/O:

```cpp
// Memory-mapped parsing (Linux/macOS)
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

class MemoryMappedParser {
    void* mapFile(const std::string& path, size_t& size) {
        int fd = open(path.c_str(), O_RDONLY);
        if (fd == -1) throw std::runtime_error("Cannot open file");
        
        struct stat st;
        fstat(fd, &st);
        size = st.st_size;
        
        void* mapped = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
        
        if (mapped == MAP_FAILED) {
            throw std::runtime_error("Memory mapping failed");
        }
        
        return mapped;
    }
};
```

### Streaming Architecture

The streaming mode (`--streaming` flag) uses a two-pass approach:

1. **Metadata Pass**: Reads only PLY headers to determine frame count and splat counts
2. **Processing Pass**: Processes frames one-by-one, writing directly to circular buffers

Benefits:
- **Memory Usage**: Only one frame in memory at a time (vs. all frames)
- **Scalability**: Can process sequences of any length
- **Performance**: Faster startup time for large sequences

Example memory savings for 1000 frames with 100K splats each:
- Traditional: ~24.8 GB memory required
- Streaming: ~25 MB memory required (99.9% reduction)

### File Structure

```
mpeg-ply2gltf/
├── CMakeLists.txt              # Build configuration
├── README.md                   # This file
├── README_MPEG.md             # Detailed MPEG extensions documentation
├── test_sequence.sh           # Test script for sequence conversion
├── test_streaming.sh          # Test script for streaming mode
├── src/
│   ├── main.cpp              # Main program entry
│   ├── PlyReader.cpp         # PLY file reader
│   ├── GltfWriter.cpp        # glTF file writer
│   ├── MultiFramePlyReader.cpp    # Multi-frame sequence reader
│   ├── CircularBufferWriter.cpp   # Circular buffer implementation
│   ├── MPEGGltfWriter.cpp         # MPEG extension writer
│   ├── StreamingMultiFrameReader.cpp    # Streaming sequence reader
│   ├── StreamingCircularBufferWriter.cpp # Streaming buffer writer
│   └── StreamingMPEGGltfWriter.cpp      # Streaming MPEG writer
└── include/
    ├── PlyReader.h           # PLY reader interface
    ├── GltfWriter.h          # glTF writer interface
    ├── GaussianSplat.h       # Data structures
    ├── MultiFrameGaussianSplat.h  # Multi-frame data structures
    ├── MultiFramePlyReader.h      # Sequence reader interface
    ├── CircularBufferWriter.h     # Circular buffer interface
    ├── MPEGGltfWriter.h           # MPEG extension interface
    ├── StreamingMultiFrameReader.h    # Streaming reader interface
    ├── StreamingCircularBufferWriter.h # Streaming buffer interface
    └── StreamingMPEGGltfWriter.h      # Streaming MPEG interface
```

## License

This tool is provided under the same license as the MPEG 3D Renderer project.

## References

1. [glTF 2.0 Specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
2. [3D Gaussian Splatting](https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/)
3. [EXT_gaussian_splats Proposal](../mpeg-3d-renderer/gltfGS.md)
4. [MPEG-I Scene Description Specification](mpegsd.md)
5. [glTF 3D Gaussian Splats Extension](gltf_3dgs.md)