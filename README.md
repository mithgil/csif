# csif: Andor SIF Parser in C

A high-performance C library for reading Andor Technology SIF (Multi-Channel File) format files. Provides efficient access to scientific image and spectral data from Andor cameras and spectrographs. 

Node.js integration is supported now.

## Features

- 🚀 **High Performance**: Pure C implementation for fast data loading
- 📊 **Complete Data Access**: Read image data, calibration coefficients, and metadata
- 🔧 **Flexible Output Control**: Configurable verbosity levels for different use cases
- 📈 **Calibration Support**: Extracts and processes calibration data for accurate measurements
- 🌐 Node.js Integration: High-performance Node.js addon for JavaScript applications
- 📦 Multiple Output Formats: JSON output for web applications and data analysis

## Project Structure

```bash
$ tree . -L 3
.
├── build
│   ├── bin
│   │   ├── debug_detail_sif    # Independent debug tool
│   │   ├── debug_sif           # Dependent debug tool  
│   │   └── read_sif            # Main example executable
│   ├── lib
│   │   ├── libsifparser.a      # Static library
│   │   └── libsifparser.so*    # Shared library
│   └── Release
│       └── sifaddon.node       # Node.js addon
├── include
│   ├── sif_parser.h           # Main parsing library
│   ├── sif_utils.h            # Utility functions
│   └── sif_json.h             # JSON output functions
└── src
    ├── sif_parser.c           # Core parsing implementation
    ├── sif_utils.c            # Utility implementations
    ├── sif_json.c             # JSON output implementation
    ├── binding.cc             # Node.js addon binding
    └── main.c                 # Example usage
```

## Quick Start

### Building the Library

C Library (CMake)
```bash
# Clone and build
git clone <repository-url>
cd csif
mkdir build && cd build
cmake ..
make -j4
```

### Node.js Addon (npm)
```bash
# Build Node.js addon
npm install
npm run build

# Or manually with node-gyp
npx node-gyp configure
npx node-gyp build
```

### Basic Usage

```c
#include "sif_parser.h"

int main() {
    SifFile sif_file;
    
    // Open and parse SIF file
    if (sif_open_file("data.sif", &sif_file) == 0) {
        // Access image data
        float* frame_data = sif_get_frame_data(&sif_file, 0);
        
        // Get calibration data
        int calib_size;
        double* calibration = retrieve_calibration(&sif_file.info, &calib_size);
        
        // Clean up
        sif_close(&sif_file);
    }
    
    return 0;
}
```

### Command Line Tools

```bash
# Basic file reading
./bin/read_sif /path/to/your/file.sif

# Quiet mode (only essential output)
./bin/read_sif /path/to/file.sif -q

# Verbose mode (detailed parsing info)
./bin/read_sif /path/to/file.sif -v

# Debug mode (all internal information)
./bin/read_sif /path/to/file.sif -d
```

## Output Levels

| Level | Description | Use Case |
|-------|-------------|----------|
| `SIF_SILENT` (0) | No output except errors | Batch processing |
| `SIF_QUIET` (1) | Essential results only | Integration |
| `SIF_NORMAL` (2) | Basic progress information | Default |
| `SIF_VERBOSE` (3) | Detailed parsing process | Debugging |
| `SIF_DEBUG` (4) | All internal information | Development |

## API Overview

### Core Functions

```c
// File operations
int sif_open_file(const char* filename, SifFile* sif_file);
int sif_open(FILE* fp, SifFile* sif_file);
void sif_close(SifFile* sif_file);

// Data access
float* sif_get_frame_data(SifFile* sif_file, int frame_index);
int sif_load_all_frames(SifFile* sif_file, int byte_swap);

// Calibration
double* retrieve_calibration(SifInfo* info, int* calibration_size);

// Output control
void sif_set_verbose_level(SifVerboseLevel level);
```

### Key Data Structures

```c
typedef struct {
    char detector_type[64];
    int number_of_frames;
    int image_width, image_height;
    float exposure_time;
    double calibration_coefficients[MAX_CALIBRATION_COEFFS];
    int calibration_coeff_count;
    // ... more fields
} SifInfo;

typedef struct {
    SifInfo info;
    SifTile* tiles;
    int tile_count;
    FILE* file_ptr;
} SifFile;
```

## Examples

### Reading Image Data

```c
SifFile sif_file;
if (sif_open_file("spectrum.sif", &sif_file) == 0) {
    printf("Image size: %dx%d, Frames: %d\n", 
           sif_file.info.image_width, 
           sif_file.info.image_height,
           sif_file.info.number_of_frames);
    
    // Load all frames
    if (sif_load_all_frames(&sif_file, 0) == 0) {
        float* frame0 = sif_get_frame_data(&sif_file, 0);
        
        // Process frame data
        for (int i = 0; i < 10; i++) {
            printf("Pixel %d: %.1f\n", i, frame0[i]);
        }
    }
    
    sif_close(&sif_file);
}

```

#### JSON Output (C)
```c
SifFile sif_file;
if (sif_open_file("spectrum.sif", &sif_file) == 0) {
    JsonOutputOptions opts = {
        .pretty_print = 1,
        .include_metadata = 1,
        .include_calibration = 1,
        .include_raw_data = 1
    };
    
    char* json_str = sif_file_to_json(&sif_file, opts);
    if (json_str) {
        printf("JSON Output:\n%s\n", json_str);
        free(json_str);
    }
    
    sif_close(&sif_file);
}

```

#### Node.js Integration
```javascript
const sifParser = require('./build/Release/sifaddon.node');

class SpectrumAnalyzer {
    static parseFile(filename) {
        try {
            const jsonString = sifParser.sifFileToJson(filename);
            const data = JSON.parse(jsonString);
            
            return {
                intensities: data.data,
                wavelengths: this.calculateWavelengths(data),
                metadata: data.metadata,
                calibration: data.calibration
            };
        } catch (error) {
            throw new Error(`Failed to parse SIF file: ${error.message}`);
        }
    }
    
    static calculateWavelengths(data) {
        if (data.calibration && data.calibration.coefficients) {
            const coeffs = data.calibration.coefficients;
            return data.data.map((_, i) => {
                // Polynomial calibration: λ = c0 + c1*x + c2*x² + c3*x³
                const x = i;
                return coeffs[0] + coeffs[1]*x + coeffs[2]*x*x + coeffs[3]*x*x*x;
            });
        }
        return null;
    }
}

// Usage
const spectrum = SpectrumAnalyzer.parseFile('spectrum.sif');
console.log('Peak intensity:', Math.max(...spectrum.intensities));
console.log('Data points:', spectrum.intensities.length);
```
### Working with Calibration Data

```c
int calib_size;
double* calibration = retrieve_calibration(&sif_file.info, &calib_size);

if (calibration) {
    printf("Calibration coefficients: %d\n", sif_file.info.calibration_coeff_count);
    
    if (sif_file.info.has_frame_calibrations) {
        printf("Frame-specific calibration available\n");
    } else {
        printf("Global calibration data:\n");
        for (int i = 0; i < 5 && i < calib_size; i++) {
            printf("  [%d] = %f\n", i, calibration[i]);
        }
    }
    
    free(calibration);
}
```

### Javascript Integration by Node Addon

Compiling the c code to a node addon will enable JS applications call c sif parsing with intrinsic C performance

#### Compile with N-API

Remember to have `node-gyp` tool 

```bash
sudo npm install -g node-gyp
```

and include `napi` header file in the `binding.cc`, 

```bash
#if there is 'build' by CMakeLists

mv build build_cmake_backup

mkdir build

npx node-gyp configure

npx node-gyp build

```

finally, your would see a addon appears under the build/Release

If anything changed, then do

```bash

rm -rf build

npx node-gyp clean

npx node-gyp configure

npx node-gyp build

```

#### Load the node addon

First, let your electron app or web app to load this node addon correctly.

Just follow the example of `test_complete.js` as follows

```JS
//test_complete.js
const sifParser = require('./build/Release/sifaddon.node');

function analyzeSifFile(filename) {
    try {
        const jsonResult = sifParser.sifFileToJson(filename);
        const data = JSON.parse(jsonResult); 
        ...
    }
}
...
// process your data and visulization

```

Have fun!

## Debug Tools

### `debug_detail_sif`
- Independent debugging tool
- Direct file analysis without library dependencies
- Raw file structure examination

### `debug_sif` 
- Library-dependent debug tool
- Tests parsing functionality
- Internal state inspection

## Installation

### System-wide Installation

```bash
cd build
sudo make install
```

## Using in Your Project

### CMake Integration
```cmake
# CMakeLists.txt
find_library(SIFPARSER_LIB sifparser)
target_link_libraries(your_target ${SIFPARSER_LIB})
```

### Node.js Integration

```bash
# Install from local path
npm install /path/to/csif

# Or link for development
cd /path/to/csif
npm link
cd /path/to/your-project
npm link sif-parser

```

### Performance Comparison

| Method | Performance | Use Case |
|--------|-------------|----------|
| **Node.js Addon** | 🚀 Highest | Electron apps, web services |
| **C Library** | 🚀 High | Native applications, CLI tools |
| **CLI + Subprocess** | 🐢 Lower | Legacy integration |

- ✅ Andor SIF format versions including 65567, 65540
- ✅ Multi-frame data
- ✅ Calibration data extraction
- ✅ Subimage and binning information
- ✅ Timestamp data
- ✅ User text metadata
- ✅ JSON output for web applications

## License
This project is licensed under the GNU General Public License v3.0 - see the LICENSE file for details.

## Contributing

Fork the repository

Create a feature branch (git checkout -b feature/amazing-feature)

Commit your changes (git commit -m 'Add amazing feature')

Push to the branch (git push origin feature/amazing-feature)

Open a Pull Request

## Development
Building for Development
```bash
# C library development
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make


# Node.js addon development
npm run clean && npm run build

# Testing
npm test
./bin/read_sif test_data/example.sif
```

## Project Architecture

- Core Parser (sif_parser.c): Low-level SIF file parsing

- JSON Output (sif_json.c): Structured data serialization

- Node.js Binding (binding.cc): V8/N-API integration

- CLI Tools: Example applications and debugging utilities

## Citation

If you use this library in your research, please cite:

```bibtex
@software{csif_parser,
  title = {csif: Andor SIF Parser in C},
  author = {Tim},
  year = {2025},
  url = {https://github.com/mithgil/csif}
}
```

## Support

For bug reports and feature requests, please open an issue on GitHub.

---

**csif** - Efficient Andor SIF file parsing in pure C.