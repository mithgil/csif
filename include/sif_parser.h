#ifndef SIF_PARSER_H
#define SIF_PARSER_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SIF_MAGIC "Andor Technology Multi-Channel File\n"
#define MAX_STRING_LENGTH 1024
#define MAX_USER_TEXT_LENGTH 8192
#define MAX_CALIBRATION_COEFFS 10

typedef enum {
    BYTE_SWAP_DISABLE = 0,
    BYTE_SWAP_ENABLE = 1,
} ByteSwapMode;

typedef struct {
    int x0, y0, x1, y1;
    int xbin, ybin;
    int width, height;
} SubImageInfo;

typedef struct {
    char detector_type[MAX_STRING_LENGTH];
    char original_filename[MAX_STRING_LENGTH];
    char spectrograph[MAX_STRING_LENGTH];
    char user_text[MAX_USER_TEXT_LENGTH];
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
    
    double *calibration_data; 
    int calibration_data_count;
    double *frame_calibrations;
    int has_frame_calibrations;
    
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
    
    // 数据存储
    float *frame_data;            // 一維數組：frame_data[frame * height * width + row * width + col]
    int data_loaded;              // 标记数据是否已加载
    
    FILE *file_ptr;               // 文件指针（用于延迟加载）
    const char *filename;         // 文件名（用于重新打开文件）
    
} SifFile;

// 主要函數
int sif_open(FILE *fp, SifFile *sif_file);
void sif_close(SifFile *sif_file);
int extract_calibration(const SifInfo *info, double **calibration, int *calib_width, int *calib_frames);

// 数据读取函数
int sif_load_all_frames(SifFile *sif_file, int enable_byte_swap);
int sif_load_single_frame(SifFile *sif_file, int frame_index);
int sif_load_frame_range(SifFile *sif_file, int start_frame, int end_frame);
void sif_unload_data(SifFile *sif_file);

// 数据访问函数
float *sif_get_frame_data(SifFile *sif_file, int frame_index);
int sif_save_frame_as_text(SifFile *sif_file, int frame_index, const char *filename);
float sif_get_pixel_value(SifFile *sif_file, int frame_index, int row, int col);
int sif_copy_frame_data(SifFile *sif_file, int frame_index, float *output_buffer);

// 輔助函數
int read_until(FILE *fp, char *buffer, int max_length, char terminator);
int read_int(FILE *fp);
double read_float(FILE *fp);
void skip_spaces(FILE *fp);
void extract_user_text(SifInfo *info);

#endif