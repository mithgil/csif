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

#ifndef SIF_PARSER_H
#define SIF_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#define SIF_MAGIC "Andor Technology Multi-Channel File\n"
#define MAX_STRING_LENGTH 1024
#define MAX_USER_TEXT_LENGTH 8192
#define MAX_CALIBRATION_COEFFS 10
#define MAX_FRAMES 100
#define MAX_COEFFICIENTS 20


typedef enum {
    SIF_SILENT = 0,    // No output (except for error messages)
    SIF_QUIET = 1,     // Only display the most important results
    SIF_NORMAL = 2,    // Display basic progress information (default)
    SIF_VERBOSE = 3,   // Display detailed parsing process
    SIF_DEBUG = 4      // Display all debug info
} SifVerboseLevel;

extern SifVerboseLevel current_verbose_level;

typedef struct {
    int x0, y0, x1, y1;
    int xbin, ybin;
    int width, height;
} SubImageInfo;

typedef struct {
    double coefficients[MAX_COEFFICIENTS];
    int coeff_count;
} FrameCalibration;

typedef struct {

    SifVerboseLevel verbose_level;

    char detector_type[MAX_STRING_LENGTH];
    char original_filename[MAX_STRING_LENGTH];
    char spectrograph[MAX_STRING_LENGTH];
    char user_text[MAX_USER_TEXT_LENGTH];
    int user_text_length; 
    int user_text_processed;  
    char frame_axis[MAX_STRING_LENGTH];
    char data_type[MAX_STRING_LENGTH];
    char image_axis[MAX_STRING_LENGTH];
    
    int sif_version;
    int sif_calb_version;
    int experiment_time;
    int accumulated_cycles;
    int number_of_frames;
    int number_of_subimages;
    int total_length;
    int image_length;
    int detector_width;
    int detector_height;
    int xbin, ybin;
        
    double detector_temperature;
    double exposure_time;
    double cycle_time;
    double accumulated_cycle_time;
    double stack_cycle_time;
    double pixel_readout_time;
    double gain_dac;
    double gate_width;
    double grating_blaze;
    double shutter_time[2];
    double gate_gain;
    double gate_delay;
    double raman_ex_wavelength;

    char calibration_data[256];  
    double calibration_coefficients[10];
    int calibration_coeff_count;  

    int has_frame_calibrations;    
    FrameCalibration frame_calibrations[MAX_FRAMES];
    
    SubImageInfo *subimages;
    int64_t *timestamps;
    
    int64_t data_offset;
    int image_width;
    int image_height;
    
} SifInfo;

typedef struct {
    int64_t offset;
    int width;
    int height;
    int frame_index;
} ImageTile;

typedef struct {
    ImageTile *tiles;
    int frame_count;
    int tile_count;
    SifInfo info;
    
    // data storage
    float *frame_data;            // 1D：frame_data[frame * height * width + row * width + col]
    int data_loaded;              // mark for data to be loaded
    
    FILE *file_ptr;               // File pointer (used for lazy loading)
    const char *filename;         // File name (used to reopen the file)
    
} SifFile;

// main functions
int sif_open(FILE *fp, SifFile *sif_file);
void sif_close(SifFile *sif_file);
int extract_calibration(const SifInfo *info, double **calibration, int *calib_width, int *calib_frames);

// Data reading function
int sif_load_all_frames(SifFile *sif_file, int enable_byte_swap);
int sif_load_single_frame(SifFile *sif_file, int frame_index);
int sif_load_frame_range(SifFile *sif_file, int start_frame, int end_frame);
void sif_unload_data(SifFile *sif_file);

//  Data access function
float *sif_get_frame_data(SifFile *sif_file, int frame_index);
int sif_save_frame_as_text(SifFile *sif_file, int frame_index, const char *filename);
float sif_get_pixel_value(SifFile *sif_file, int frame_index, int row, int col);
int sif_copy_frame_data(SifFile *sif_file, int frame_index, float *output_buffer);

// helper functions
int read_until(FILE *fp, char *buffer, int max_length, char terminator);
int read_int(FILE *fp);
double read_float(FILE *fp);
void skip_spaces(FILE *fp);
void extract_user_text(SifInfo *info);
void extract_frame_calibrations(SifInfo *info, int start_pos);
void parse_calibration_coefficients(SifInfo *info);
void parse_frame_calibration_coefficients(SifInfo *info, int frame, const char* data_str);

// function to control verbose level
void sif_set_verbose_level(SifVerboseLevel level);
void sif_print(SifVerboseLevel min_level, const char* format, ...);

// Convenience macro (defined in header file)
#define PRINT_SILENT(...)   sif_print(SIF_SILENT, __VA_ARGS__)
#define PRINT_NORMAL(...)   sif_print(SIF_NORMAL, __VA_ARGS__)
#define PRINT_VERBOSE(...)  sif_print(SIF_VERBOSE, __VA_ARGS__)
#define PRINT_DEBUG(...)    sif_print(SIF_DEBUG, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif