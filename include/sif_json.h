#ifndef SIF_JSON_H
#define SIF_JSON_H

#include "sif_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

// JSON output
typedef struct {
    int include_raw_data;          // if contains raw data 
    int include_calibration;       // if contains calibration coefficients
    int include_metadata;          // if contains metadata
    int pretty_print;              // if pretty print （newline and indent）
    size_t max_data_points;        // max poiint (0 denotes all)
    int include_all_frames;        // if includes all frames
    size_t max_frames;             // max frames
} JsonOutputOptions;

// default options
extern const JsonOutputOptions JSON_DEFAULT_OPTIONS;
extern const JsonOutputOptions JSON_METADATA_ONLY_OPTIONS;
extern const JsonOutputOptions JSON_FULL_DATA_OPTIONS;

// main json output functions
char* sif_file_to_json(SifFile *sif_file, JsonOutputOptions options);
char* sif_info_to_json(SifInfo *info);
char* sif_frame_data_to_json(SifFile *sif_file, int frame_index, JsonOutputOptions options);

// documents output
int sif_save_as_json(SifFile *sif_file, const char *filename, JsonOutputOptions options);

// convenient functions
char* sif_file_to_json_simple(SifFile *sif_file);  // use default
char* sif_file_metadata_to_json(SifFile *sif_file); // only metadata

#ifdef __cplusplus
}
#endif

#endif