#include "sif_parser.h"

// 輔助函數實現
int read_string(FILE *fp, char *buffer, int max_length) {
    int length = read_int(fp);
    if (length <= 0 || length >= max_length) {
        return -1;
    }
    
    if (fread(buffer, 1, length, fp) != length) {
        return -1;
    }
    buffer[length] = '\0';
    return length;
}

int read_until(FILE *fp, char *buffer, int max_length, char terminator) {
    int i = 0;
    char c;
    
    while (i < max_length - 1) {
        if (fread(&c, 1, 1, fp) != 1) {
            return -1;
        }
        
        if (c == terminator || c == '\n') {
            if (i > 0) break;
            continue;
        }
        
        buffer[i++] = c;
    }
    
    buffer[i] = '\0';
    return i;
}

int read_int(FILE *fp) {
    char buffer[32];
    if (read_until(fp, buffer, sizeof(buffer), ' ') < 0) {
        return -1;
    }
    return atoi(buffer);
}

double read_float(FILE *fp) {
    char buffer[64];
    if (read_until(fp, buffer, sizeof(buffer), ' ') < 0) {
        return NAN;
    }
    return atof(buffer);
}

void skip_spaces(FILE *fp) {
    char c;
    long offset;
    
    while (1) {
        offset = ftell(fp);
        if (fread(&c, 1, 1, fp) != 1) break;
        
        if (c != ' ' && c != '\n') {
            fseek(fp, offset, SEEK_SET);
            break;
        }
    }
}

// 主要解析函數
int sif_open(FILE *fp, SifFile *sif_file) {
    if (!fp || !sif_file) return -1;
    
    SifInfo *info = &sif_file->info;
    memset(info, 0, sizeof(SifInfo));
    info->raman_ex_wavelength = NAN;
    
    // Line 1: Magic string
    char magic[64];
    if (fread(magic, 1, 36, fp) != 36 || strncmp(magic, SIF_MAGIC, 36) != 0) {
        return -1;
    }
    
    // Line 2: Skip
    char line_buffer[256];
    if (!fgets(line_buffer, sizeof(line_buffer), fp)) return -1;
    
    // Line 3
    info->sif_version = read_int(fp);
    read_int(fp); read_int(fp); read_int(fp); // skip 0, 0, 1
    
    info->experiment_time = read_int(fp);
    info->detector_temperature = read_float(fp);
    
    // Skip 10 bytes
    fseek(fp, 10, SEEK_CUR);
    
    read_int(fp); // skip 0
    info->exposure_time = read_float(fp);
    info->cycle_time = read_float(fp);
    info->accumulated_cycle_time = read_float(fp);
    info->accumulated_cycles = read_int(fp);
    
    // Skip NULL and space
    fseek(fp, 2, SEEK_CUR);
    
    info->stack_cycle_time = read_float(fp);
    info->pixel_readout_time = read_float(fp);
    
    read_int(fp); read_int(fp); // skip 0, 1
    info->gain_dac = read_float(fp);
    
    read_int(fp); read_int(fp); // skip 0, 0
    info->gate_width = read_float(fp);
    
    // Skip 16 values
    for (int i = 0; i < 16; i++) read_int(fp);
    
    info->grating_blaze = read_float(fp);
    
    // Skip to end of line
    if (!fgets(line_buffer, sizeof(line_buffer), fp)) return -1;
    
    // Line 4: Detector type
    if (!fgets(line_buffer, sizeof(line_buffer), fp)) return -1;
    line_buffer[strcspn(line_buffer, "\r\n")] = 0;
    strncpy(info->detector_type, line_buffer, sizeof(info->detector_type) - 1);
    
    // Line 5: Detector dimensions
    info->detector_width = read_int(fp);
    info->detector_height = read_int(fp);
    
    // Original filename
    if (read_string(fp, info->original_filename, sizeof(info->original_filename)) < 0) {
        return -1;
    }
    
    // Skip space and newline
    fseek(fp, 2, SEEK_CUR);
    
    // Line 7: Skip 65538
    read_int(fp);
    
    // Line 8: User text
    if (read_string(fp, info->user_text, sizeof(info->user_text)) < 0) {
        return -1;
    }
    
    // Skip newline
    fseek(fp, 1, SEEK_CUR);
    
    // Line 9
    read_int(fp); // 65538
    fseek(fp, 8, SEEK_CUR); // skip spaces and binary
    
    info->shutter_time[0] = read_float(fp);
    info->shutter_time[1] = read_float(fp);
    
    // Version-specific skipping
    if (info->sif_version >= 65548 && info->sif_version <= 65557) {
        for (int i = 0; i < 2; i++) fgets(line_buffer, sizeof(line_buffer), fp);
    } else if (info->sif_version == 65558) {
        for (int i = 0; i < 5; i++) fgets(line_buffer, sizeof(line_buffer), fp);
    } else if (info->sif_version == 65559 || info->sif_version == 65564) {
        for (int i = 0; i < 8; i++) fgets(line_buffer, sizeof(line_buffer), fp);
        fgets(line_buffer, sizeof(line_buffer), fp);
        char *token = strtok(line_buffer, " \t\r\n");
        token = strtok(NULL, " \t\r\n"); // get second token
        if (token) strncpy(info->spectrograph, token, sizeof(info->spectrograph) - 1);
    } else if (info->sif_version == 65565) {
        for (int i = 0; i < 15; i++) fgets(line_buffer, sizeof(line_buffer), fp);
    } else if (info->sif_version > 65565) {
        for (int i = 0; i < 8; i++) fgets(line_buffer, sizeof(line_buffer), fp);
        fgets(line_buffer, sizeof(line_buffer), fp);
        char *token = strtok(line_buffer, " \t\r\n");
        token = strtok(NULL, " \t\r\n");
        if (token) strncpy(info->spectrograph, token, sizeof(info->spectrograph) - 1);
        
        fgets(line_buffer, sizeof(line_buffer), fp);
        read_float(fp); read_float(fp); read_float(fp);
        info->gate_gain = read_float(fp);
        read_float(fp); read_float(fp);
        info->gate_delay = read_float(fp) * 1e-12;
        info->gate_width = read_float(fp) * 1e-12;
        
        for (int i = 0; i < 8; i++) fgets(line_buffer, sizeof(line_buffer), fp);
    }
    
    // SifCalbVersion
    info->sif_calb_version = read_int(fp);
    
    if (info->sif_calb_version == 65540) {
        fgets(line_buffer, sizeof(line_buffer), fp);
    }
    
    // Calibration data
    if (strstr(info->spectrograph, "Mechelle") != NULL) {
        fgets(line_buffer, sizeof(line_buffer), fp);
        char *token = strtok(line_buffer, " \t\r\n");
        int i = 0;
        while (token && i < MAX_CALIBRATION_COEFFS) {
            info->calibration_data[i++] = atof(token);
            token = strtok(NULL, " \t\r\n");
        }
        info->calibration_data_count = i;
    } else {
        fgets(line_buffer, sizeof(line_buffer), fp);
    }
    
    // Skip calibration data old, raman wavelength, etc.
    fgets(line_buffer, sizeof(line_buffer), fp);
    fgets(line_buffer, sizeof(line_buffer), fp);
    if (sscanf(line_buffer, "%lf", &info->raman_ex_wavelength) != 1) {
        info->raman_ex_wavelength = NAN;
    }
    
    fgets(line_buffer, sizeof(line_buffer), fp);
    fgets(line_buffer, sizeof(line_buffer), fp);
    fgets(line_buffer, sizeof(line_buffer), fp);
    
    // Frame axis, data type, image axis
    if (read_string(fp, info->frame_axis, sizeof(info->frame_axis)) < 0) return -1;
    if (read_string(fp, info->data_type, sizeof(info->data_type)) < 0) return -1;
    if (read_string(fp, info->image_axis, sizeof(info->image_axis)) < 0) return -1;
    
    read_int(fp); // skip 65541 or 65539
    
    // Read coordinates
    int x0 = read_int(fp);
    int y1 = read_int(fp);
    int x1 = read_int(fp);
    int y0 = read_int(fp);
    
    info->number_of_frames = read_int(fp);
    info->number_of_subimages = read_int(fp);
    info->total_length = read_int(fp);
    info->image_length = read_int(fp);
    
    // Allocate subimages
    info->subimages = malloc(info->number_of_subimages * sizeof(SubImageInfo));
    if (!info->subimages) return -1;
    
    int total_width = 0, total_height = 0;
    
    for (int i = 0; i < info->number_of_subimages; i++) {
        SubImageInfo *sub = &info->subimages[i];
        read_int(fp); // skip 65538
        
        fgets(line_buffer, sizeof(line_buffer), fp);
        sscanf(line_buffer, "%d %d %d %d %d %d", 
               &sub->x0, &sub->y1, &sub->x1, &sub->y0, 
               &sub->ybin, &sub->xbin);
        
        sub->width = (1 + sub->x1 - sub->x0) / sub->xbin;
        sub->height = (1 + sub->y1 - sub->y0) / sub->ybin;
        
        total_width = sub->width;
        total_height += sub->height;
        
        info->xbin = sub->xbin;
        info->ybin = sub->ybin;
    }
    
    info->image_width = total_width;
    info->image_height = total_height;
    sif_file->image_width = total_width;
    sif_file->image_height = total_height;
    sif_file->frame_count = info->number_of_frames;
    
    // Skip spaces and read timestamps
    skip_spaces(fp);
    
    info->timestamps = malloc(info->number_of_frames * sizeof(int64_t));
    if (!info->timestamps) {
        free(info->subimages);
        return -1;
    }
    
    for (int f = 0; f < info->number_of_frames; f++) {
        fgets(line_buffer, sizeof(line_buffer), fp);
        info->timestamps[f] = atoll(line_buffer);
    }
    
    // Determine data offset
    info->data_offset = ftell(fp);
    
    // Check for extra flags (version-specific)
    long offset = info->data_offset;
    fseek(fp, offset, SEEK_SET);
    
    int flag = read_int(fp);
    if (flag == 0) {
        info->data_offset = ftell(fp);
    } else if (flag == 1 && info->sif_version == 65567) {
        for (int i = 0; i < info->number_of_frames; i++) {
            fgets(line_buffer, sizeof(line_buffer), fp);
        }
        info->data_offset = ftell(fp);
    }
    
    // Create tiles
    sif_file->tile_count = info->number_of_frames;
    sif_file->tiles = malloc(sif_file->tile_count * sizeof(ImageTile));
    if (!sif_file->tiles) {
        free(info->subimages);
        free(info->timestamps);
        return -1;
    }
    
    for (int f = 0; f < sif_file->tile_count; f++) {
        sif_file->tiles[f].offset = info->data_offset + f * total_width * total_height * 4;
        sif_file->tiles[f].width = total_width;
        sif_file->tiles[f].height = total_height;
        sif_file->tiles[f].frame_index = f;
    }
    
    // Extract user text information
    extract_user_text(info);
    
    return 0;
}

void sif_close(SifFile *sif_file) {
    if (!sif_file) return;
    
    free(sif_file->info.subimages);
    free(sif_file->info.timestamps);
    free(sif_file->tiles);
    
    if (sif_file->info.frame_calibrations) {
        free(sif_file->info.frame_calibrations);
    }
    
    memset(sif_file, 0, sizeof(SifFile));
}

int extract_user_text(SifInfo *info) {
    if (strstr(info->user_text, "Calibration data for") != NULL) {
        // Parse frame calibrations from user text
        // This is a simplified version - you'd need more complex parsing
        // for the actual calibration coefficients
        info->has_frame_calibrations = 1;
    } else {
        info->has_frame_calibrations = 0;
    }
    return 0;
}

int extract_calibration(const SifInfo *info, double **calibration, 
                       int *calib_width, int *calib_frames) {
    if (!info || !calibration) return -1;
    
    int width = info->image_length > 0 ? info->image_length : info->detector_width;
    
    if (info->has_frame_calibrations && info->number_of_frames > 0) {
        // Multiple calibrations (simplified)
        *calib_frames = info->number_of_frames;
        *calib_width = width;
        *calibration = malloc(*calib_frames * *calib_width * sizeof(double));
        
        if (!*calibration) return -1;
        
        // This would need actual polynomial calculation based on coefficients
        for (int f = 0; f < *calib_frames; f++) {
            for (int i = 0; i < width; i++) {
                // Simplified linear calibration
                (*calibration)[f * width + i] = i;
            }
        }
        
    } else if (info->calibration_data_count > 0) {
        // Single calibration
        *calib_frames = 1;
        *calib_width = width;
        *calibration = malloc(width * sizeof(double));
        
        if (!*calibration) return -1;
        
        // Calculate polynomial values
        for (int i = 0; i < width; i++) {
            double x = i + 1;
            double value = 0;
            double x_power = 1;
            
            for (int j = 0; j < info->calibration_data_count; j++) {
                value += info->calibration_data[j] * x_power;
                x_power *= x;
            }
            
            (*calibration)[i] = value;
        }
        
    } else {
        *calibration = NULL;
        *calib_width = 0;
        *calib_frames = 0;
        return -1;
    }
    
    return 0;
}