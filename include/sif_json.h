/*
 * csif - Andor SIF Parser in C
 * Copyright (C) 2025 mithgil
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
 
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