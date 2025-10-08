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
 
#include "sif_utils.h"
#include "sif_parser.h"
#include <ctype.h>
#include <inttypes.h> 

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <math.h>

static int read_binary_string(FILE *fp, char *buffer, int max_length, int length);
static int read_line_with_binary_check(FILE *fp, char *buffer, int max_length);
static int read_line_directly(FILE *fp, char *buffer, int max_length);
static int safe_read_length_prefixed_string(FILE *fp, char *buffer, int max_length, const char *field_name);
static void discard_line(FILE *fp);
static void discard_bytes(FILE *fp, long count);

void trim_trailing_whitespace(char *str) {
    if (!str) return;
    
    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[len - 1] = '\0';
        len--;
    }
}

void debug_hex_dump(FILE* fp, long debug_pos, int num_bytes_to_dump) {

    if (current_verbose_level < SIF_DEBUG) {
        return;  
    }

    long original_pos = ftell(fp);
    
    // move to the designated position
    fseek(fp, debug_pos, SEEK_SET);
    
    PRINT_DEBUG("→ Debug Hex Dump starting from position: 0x%lX\n", debug_pos);
    PRINT_DEBUG("Bytes to dump: %d\n\n", num_bytes_to_dump);
    
    // reading data
    unsigned char *buffer = (unsigned char*)malloc(num_bytes_to_dump);
    if (!buffer) {
        printf("Error: Memory allocation failed for %d bytes\n", num_bytes_to_dump);
        fseek(fp, original_pos, SEEK_SET);
        return;
    }
    
    long bytes_read = fread(buffer, 1, num_bytes_to_dump, fp);
    
    PRINT_DEBUG("Bytes actually read: %ld\n\n", bytes_read);
    PRINT_DEBUG("Offset  Hex                                               ASCII\n");
    PRINT_DEBUG("------  ------------------------------------------------  ----------------\n");
    
    // 計算要顯示的行數
    long lines_to_display = (bytes_read + 15) / 16;
    
    for (long i = 0; i < lines_to_display; i++) {
        long offset = i * 16;
        long absolute_offset = debug_pos + offset;
        
        PRINT_DEBUG("%06lX  ", absolute_offset);
        
        // display Hexdecimal
        for (int j = 0; j < 16; j++) {
            if (offset + j < bytes_read) {
                PRINT_DEBUG("%02X ", buffer[offset + j]);
            } else {
                PRINT_DEBUG("   ");
            }
        }
        
        PRINT_DEBUG(" ");
        
        // display ASCII
        for (int j = 0; j < 16 && offset + j < bytes_read; j++) {
            unsigned char c = buffer[offset + j];
            if (isprint(c)) {
                PRINT_DEBUG("%c", c);
            } else {
                PRINT_DEBUG(".");
            }
        }
        
        PRINT_DEBUG("\n");
        
        // mark some special positions
        if (offset == 0) {
            PRINT_DEBUG("       ^-- Start of dump (position 0x%lX)\n", debug_pos);
        }
        
        // mark floating point data format
        if (offset + 4 <= bytes_read) {
            // Check if it might be floating-point data (common 0x44 pattern)
            if (buffer[offset + 2] == 0x1C && buffer[offset + 3] == 0x44) {
                PRINT_DEBUG("       ^-- Possible float data pattern: 1C 44\n");
            }
        }
    }
    
    // show stats
    PRINT_DEBUG("\n=== Debug Hex Dump Summary ===\n");
    PRINT_DEBUG("Start position: 0x%lX\n", debug_pos);
    PRINT_DEBUG("Bytes requested: %d\n", num_bytes_to_dump);
    PRINT_DEBUG("Bytes displayed: %ld\n", bytes_read);
    PRINT_DEBUG("End position: 0x%lX\n", debug_pos + bytes_read);
    
    free(buffer);
    fseek(fp, original_pos, SEEK_SET);
    PRINT_DEBUG("Reset to original position: 0x%lX\n", original_pos);
}

// 結合兩者的多功能調試函數
void debug_comprehensive(FILE* fp, long debug_pos, int num_lines, int hex_dump_bytes) {
    if (current_verbose_level < SIF_DEBUG) {
        return; 
    }

    PRINT_DEBUG("=== Comprehensive Debug Analysis ===\n");
    PRINT_DEBUG("Starting from position: 0x%lX\n\n", debug_pos);
    
    // 1. display text
    debug_print_some_lines(fp, debug_pos, num_lines);
    
    PRINT_DEBUG("\n");
    
    // 2. display hex dump
    debug_hex_dump(fp, debug_pos, hex_dump_bytes);
}

// Helper: Reads a 4-byte (32-bit) little-endian int from a file stream
// Returns: The integer value on success, -1 on failure
int32_t read_little_endian_int32(FILE *fp) {
    uint8_t bytes[4];
    size_t count = fread(bytes, 1, 4, fp);

    if (count != 4) {
        // If fewer than 4 bytes were read, return an error
        return -1; 
    }

    // Construct the 32-bit integer (Little-Endian: byte[0] is the least significant byte)
    int32_t value = (int32_t)(
        (bytes[0] << 0) |
        (bytes[1] << 8) |
        (bytes[2] << 16) |
        (bytes[3] << 24)
    );

    return value;
}

// Read a Big-Endian 32-bit integer
int32_t read_big_endian_int32(FILE *fp) {
    uint8_t bytes[4];
    size_t count = fread(bytes, 1, 4, fp);
    
    if (count != 4) {
        return -1;
    }
    
    // Big-Endian: byte[0] is the most significant byte
    int32_t value = (int32_t)(
        (bytes[0] << 24) |
        (bytes[1] << 16) | 
        (bytes[2] << 8) |
        (bytes[3] << 0)
    );
    
    return value;
}

void print_sif_first_line(const char *filename, SifInfo *info) {
    if (current_verbose_level < SIF_DEBUG) {
        return;  
    }

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        printf("Error: Cannot open file %s\n", filename);
        return;
    }
    
    char line[256];
    if (fgets(line, sizeof(line), fp)) {
        PRINT_DEBUG("First line of %s:\n", filename);
        PRINT_DEBUG("Hex: ");
        for (int i = 0; i < strlen(line) && i < 50; i++) {
            PRINT_DEBUG("%02X ", (unsigned char)line[i]);
        }
        PRINT_DEBUG("\nText: ");
        for (int i = 0; i < strlen(line) && i < 50; i++) {
            if (isprint((unsigned char)line[i])) {
                PRINT_DEBUG("%c", line[i]);
            } else {
                PRINT_DEBUG(".");
            }
        }
        PRINT_DEBUG("\n");
    }
    
    fclose(fp);
}

void print_sif_info_summary(const SifInfo *info) {
    if (!info) return;
    
    PRINT_NORMAL("SIF File Information Summary:\n");
    PRINT_NORMAL("=============================\n");
    PRINT_NORMAL("Detector Type: %s\n", info->detector_type);
    PRINT_NORMAL("Experiment Time: %" PRId64 "\n", info->experiment_time);
    
    if (info->detector_temperature < -900) {
        PRINT_NORMAL("Detector Temperature: [SENSOR OFFLINE]\n");
    } else {
        PRINT_NORMAL("Detector Temperature: %.2f °C\n", info->detector_temperature);
    }
    
    PRINT_NORMAL("Original Filename: %s\n", info->original_filename);
    PRINT_NORMAL("Spectrograph: %s\n", info->spectrograph);
    PRINT_NORMAL("SIF Version: %d\n", info->sif_version);
    PRINT_NORMAL("SIF Calibration Version: %d\n", info->sif_calb_version);
    PRINT_NORMAL("Detector Dimensions: %d x %d\n", info->detector_width, info->detector_height);
    PRINT_NORMAL("Image Size: %d x %d\n", info->image_width, info->image_height);
    PRINT_NORMAL("Number of Frames: %d\n", info->number_of_frames);
    PRINT_NORMAL("Number of Subimages: %d\n", info->number_of_subimages);
    PRINT_NORMAL("Exposure Time: %f s\n", info->exposure_time);
    PRINT_NORMAL("Cycle Time: %f s\n", info->cycle_time);
    PRINT_NORMAL("Data Offset: 0x%08lX\n", info->data_offset);
    
    if (info->calibration_coeff_count > 0) {
        PRINT_NORMAL("Calibration Coefficients: ");
        for (int i = 0; i < info->calibration_coeff_count; i++) {
            PRINT_NORMAL("%f ", info->calibration_coefficients[i]);
        }
        PRINT_NORMAL("\n");
    }
    
    PRINT_NORMAL("Frame Axis: %s\n", info->frame_axis);
    PRINT_NORMAL("Data Type: %s\n", info->data_type);
    PRINT_NORMAL("Image Axis: %s\n", info->image_axis);
    
    if (info->timestamps && info->number_of_frames > 0) {
        PRINT_VERBOSE("First 5 timestamps: ");
        for (int i = 0; i < 5 && i < info->number_of_frames; i++) {
            PRINT_VERBOSE("%" PRId64 " ", info->timestamps[i]);
        }
        PRINT_VERBOSE("\n");
    }
}

void print_sif_file_structure(const SifFile *sif_file) {
    if (!sif_file) return;
    
    PRINT_NORMAL("SIF File Structure:\n");
    PRINT_NORMAL("===================\n");
    PRINT_NORMAL("Total Frames: %d\n", sif_file->info.number_of_frames);
    PRINT_NORMAL("Image Size: %d x %d\n", sif_file->info.image_width, sif_file->info.image_height);
    PRINT_NORMAL("Tile Count: %d\n", sif_file->tile_count);
    PRINT_NORMAL("\n");
    
    PRINT_VERBOSE("Tile Information:\n");
    for (int i = 0; i < sif_file->tile_count; i++) {
        PRINT_VERBOSE("  Tile %d: offset=0x%08lX, size=%dx%d\n", 
                     i, sif_file->tiles[i].offset, 
                     sif_file->tiles[i].width, sif_file->tiles[i].height);
    }
    
    PRINT_VERBOSE("\nSubimage Information:\n");
    for (int i = 0; i < sif_file->info.number_of_subimages; i++) {
        PRINT_VERBOSE("  Subimage %d: area=(%d,%d)-(%d,%d), binning=%dx%d, size=%dx%d\n",
                    i,  sif_file->info.subimages[i].x0,     
                        sif_file->info.subimages[i].y0,     
                        sif_file->info.subimages[i].x1,     
                        sif_file->info.subimages[i].y1,     
                        sif_file->info.subimages[i].xbin,   
                        sif_file->info.subimages[i].ybin,   
                        sif_file->info.subimages[i].width, 
                        sif_file->info.subimages[i].height);
    }
}

void debug_print_some_lines(FILE* fp, long debug_pos, int num_lines) {
    long original_pos = ftell(fp);
    fseek(fp, debug_pos, SEEK_SET);
    
    PRINT_DEBUG("→ Debug: Checking actual data format at 0x%lX\n", debug_pos);
    
    for (int i = 0; i < num_lines; i++) {
        char debug_line[256];
        if (fgets(debug_line, sizeof(debug_line), fp) == NULL) break;
        trim_trailing_whitespace(debug_line);
        PRINT_DEBUG("  Line %d: '%s' (length: %lu)\n", i, debug_line, strlen(debug_line));
    }
    
    fseek(fp, original_pos, SEEK_SET);
    PRINT_DEBUG("  Reset to position: 0x%lX\n", original_pos);
}


// Prints a hexadecimal dump (supports starting before a specified position)
void print_hex_dump(FILE *fp, int target_offset, int before_bytes, int after_bytes) {

    if (current_verbose_level < SIF_DEBUG) {
        return;  
    }

    if (!fp) return;
    
    //  Calculate the actual starting position
    int start_offset = target_offset - before_bytes;
    if (start_offset < 0) start_offset = 0;
    
    int total_length = before_bytes + after_bytes;
    
    fseek(fp, start_offset, SEEK_SET);
    
    unsigned char buffer[16];
    int bytes_read;
    int total_bytes = 0;
    
    PRINT_DEBUG("Hex Dump (offset 0x%08X, showing %d bytes before and %d bytes after):\n", 
           target_offset, before_bytes, after_bytes);
    PRINT_DEBUG("Offset    Hex Content                     ASCII\n");
    PRINT_DEBUG("--------  ------------------------------  ----------------\n");
    
    while (total_bytes < total_length && 
           (bytes_read = fread(buffer, 1, 16, fp)) > 0) {
        
        int current_offset = start_offset + total_bytes;
        
        PRINT_DEBUG("%08X  ", current_offset);
        
        //  Mark the target position
        if (current_offset <= target_offset && (current_offset + bytes_read) > target_offset) {
            PRINT_DEBUG(">");
        } else {
            PRINT_DEBUG(" ");
        }
        
        //  Display hexadecimal
        for (int i = 0; i < 16; i++) {
            if (i < bytes_read) {
                // 標記目標位置的字節
                if (current_offset + i == target_offset) {
                    PRINT_DEBUG("[%02X]", buffer[i]);
                } else {
                    PRINT_DEBUG("%02X ", buffer[i]);
                }
            } else {
                PRINT_DEBUG("   ");
            }
            
            if (i == 7) PRINT_DEBUG(" ");
        }
        
        PRINT_DEBUG(" ");
        
        // Display ASCII
        for (int i = 0; i < bytes_read; i++) {
            if (current_offset + i == target_offset) {
                PRINT_DEBUG("["); // Start marker
            }
            
            if (isprint(buffer[i])) {
                PRINT_DEBUG("%c", buffer[i]);
            } else {
                PRINT_DEBUG(".");
            }
            
            if (current_offset + i == target_offset) {
                PRINT_DEBUG("]"); // End marker
            }
        }
        
        PRINT_DEBUG("\n");
        
        total_bytes += bytes_read;
        if (total_bytes >= total_length) break;
    }
}


double evaluate_polynomial(const double* coefficients, int coeff_count, double x) {
    double result = 0.0;
    for (int i = 0; i < coeff_count; i++) {
        result += coefficients[i] * pow(x, i);
    }
    return result;
}


//turn coeeficients into polinomials
double* retrieve_calibration(SifInfo *info, int* calibration_size) {

    if (!info) {
        *calibration_size = 0;
        return NULL;
    }
    
    int width = info->detector_width;
    if (info->image_length > 0) {
        width = info->image_length;
    }
    
    PRINT_VERBOSE("→ Retrieving calibration data (width: %d)\n", width);
    
    // 情況1: 有多個 frame 校準數據
    if (info->has_frame_calibrations && info->number_of_frames > 0) {
        PRINT_VERBOSE("  Found frame-specific calibrations for %d frames\n", info->number_of_frames);
        
        // 分配 2D 陣列：number_of_frames × width
        double* calibration = malloc(info->number_of_frames * width * sizeof(double));
        if (!calibration) {
            *calibration_size = 0;
            return NULL;
        }
        
        for (int frame = 0; frame < info->number_of_frames; frame++) {
            FrameCalibration* frame_calib = &info->frame_calibrations[frame];
            
            if (frame_calib->coeff_count > 0) {
                // 複製係數以便反轉（不修改原始數據）
                double coefficients[MAX_COEFFICIENTS];
                memcpy(coefficients, frame_calib->coefficients, frame_calib->coeff_count * sizeof(double));
                               
                PRINT_VERBOSE("    Frame %d: %d coefficients -> ", frame + 1, frame_calib->coeff_count);
                for (int i = 0; i < frame_calib->coeff_count; i++) {
                    printf("%f ", coefficients[i]);
                }
                PRINT_VERBOSE("\n");
                
                // 計算多項式值（對應 Julia 的 p.(1:width)）
                for (int x = 0; x < width; x++) {
                    double pixel_value = evaluate_polynomial(coefficients, frame_calib->coeff_count, x + 1);
                    calibration[frame * width + x] = pixel_value;
                }
            } else {
                // 如果沒有校準數據，填充 0
                for (int x = 0; x < width; x++) {
                    calibration[frame * width + x] = 0.0;
                }
            }
        }
        
        *calibration_size = info->number_of_frames * width;
        return calibration;
    }
    // 情況2: 有單個校準數據
    else if (info->calibration_coeff_count > 0) {
        PRINT_VERBOSE("  Found global calibration data: %d coefficients\n", info->calibration_coeff_count);
        
        // 分配 1D 陣列：width
        double* calibration = malloc(width * sizeof(double));
        if (!calibration) {
            *calibration_size = 0;
            return NULL;
        }
        
        // 複製係數以便反轉
        double coefficients[MAX_COEFFICIENTS];
        memcpy(coefficients, info->calibration_coefficients, info->calibration_coeff_count * sizeof(double));
                
        PRINT_VERBOSE("    Coefficients: ");
        for (int i = 0; i < info->calibration_coeff_count; i++) {
            PRINT_VERBOSE("%f ", coefficients[i]);
        }
        PRINT_VERBOSE("\n");
        
        // 計算多項式值（對應 Julia 的 p.(1:width)）
        for (int x = 0; x < width; x++) {
            calibration[x] = evaluate_polynomial(coefficients, info->calibration_coeff_count, x + 1);
        }
        
        *calibration_size = width;
        return calibration;
    }
    // 情況3: 沒有校準數據
    else {
        PRINT_VERBOSE("  No calibration data found\n");
        *calibration_size = 0;
        return NULL;
    }
}
