# csif: Andor SIF Parser in C

A high-performance C library for reading Andor Technology SIF (Multi-Channel File) format files. Provides efficient access to scientific image and spectral data from Andor cameras and spectrographs.

## Features

- 🚀 **High Performance**: Pure C implementation for fast data loading
- 📊 **Complete Data Access**: Read image data, calibration coefficients, and metadata
- 🔧 **Flexible Output Control**: Configurable verbosity levels for different use cases
- 🎯 **Accurate Parsing**: Handles various SIF file versions and formats
- 📈 **Calibration Support**: Extracts and processes calibration data for accurate measurements

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
├── include
│   ├── sif_parser.h           # Main parsing library
│   └── sif_utils.h            # Utility functions
└── src
    ├── sif_parser.c           # Core parsing implementation
    ├── sif_utils.c            # Utility implementations
    └── main.c                 # Example usage
```

## Quick Start

### Building the Library

```bash
# Clone and build
git clone <repository-url>
cd csif
mkdir build && cd build
cmake ..
make -j4
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

### Using in Your Project

```cmake
# CMakeLists.txt
find_library(SIFPARSER_LIB sifparser)
target_link_libraries(your_target ${SIFPARSER_LIB})
```

## File Format Support

- ✅ Andor SIF format versions including 65567, 65540
- ✅ Multi-frame data
- ✅ Calibration data extraction
- ✅ Subimage and binning information
- ✅ Timestamp data
- ✅ User text metadata

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

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