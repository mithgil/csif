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

 #define _POSIX_C_SOURCE 200809L  // 啟用 strdup

#include "sif_parser.h"
#include "sif_utils.h"
#include <ctype.h>
#include <inttypes.h>

SifVerboseLevel current_verbose_level = SIF_NORMAL;

void sif_set_verbose_level(SifVerboseLevel level) {
    current_verbose_level = level;
}

void sif_print(SifVerboseLevel min_level, const char* format, ...) {
    if (current_verbose_level >= min_level) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}

static void swap_float_array_endian(float *data, int count);
static void extract_text_part_robust(const char *input, char *output, int max_length);

static int read_binary_string(FILE *fp, char *buffer, int max_length, int length);
static int read_line_with_binary_check(FILE *fp, char *buffer, int max_length);
static int read_line_directly(FILE *fp, char *buffer, int max_length);

static void discard_line(FILE *fp);
static void discard_bytes(FILE *fp, long count);

static void cleanup_sif_info(SifInfo *info);

static void extract_text_part_robust(const char *input, char *output, int max_length) {
    if (!input || !output) return;
    
    int i = 0;
    int out_index = 0;
    int found_text = 0;
    
    // skip prior spaces
    while (input[i] != '\0' && isspace((unsigned char)input[i])) {
        i++;
    }
    
    // copy char until number is met
    while (input[i] != '\0' && out_index < max_length - 1) {
        unsigned char c = input[i];
        
        if (isalpha(c) || c == ' ') {
            output[out_index++] = c;
            found_text = 1;
        } else if (isdigit(c)) {
            // number met, and stop
            if (found_text) break;
            // if text is not yet found, continue skipping preceding numbers
        } else {
            // other chars (commas), if they are met and stop
            if (found_text) break;
        }
        i++;
    }
    
    output[out_index] = '\0';
    trim_trailing_whitespace(output);
    
    // if retrieving fails, defaults to 
    if (strlen(output) == 0) {
        if (strstr(input, "Wavelength") != NULL) {
            strcpy(output, "Wavelength");
        } else if (strstr(input, "Counts") != NULL) {
            strcpy(output, "Counts");
        } else if (strstr(input, "Pixel") != NULL) {
            strcpy(output, "Pixel number");
        }
    }
}

// read line
static int read_line_directly(FILE *fp, char *buffer, int max_length) {
    long start_pos = ftell(fp);
    printf("  Falling back to direct line reading at offset: 0x%lX\n", start_pos);
    
    int i = 0;
    int c;
    
    while (i < max_length - 1) {
        c = fgetc(fp);
        if (c == EOF || c == '\n' || c == '\r') {
            break;
        }
        
        buffer[i++] = (char)c;
    }
    
    buffer[i] = '\0';
    
    // deal with carridge return
    if (c == '\r') {
        c = fgetc(fp);
        if (c != '\n') {
            ungetc(c, fp);
        }
    }
    
    printf("  Directly read string: '");
    for (int j = 0; j < i && j < 50; j++) {
        if (isprint((unsigned char)buffer[j])) {
            printf("%c", buffer[j]);
        } else {
            printf("\\x%02X", (unsigned char)buffer[j]);
        }
    }
    if (i > 50) printf("...");
    printf("' (length: %d)\n", i);
    
    return i;
}

static int read_binary_string(FILE *fp, char *buffer, int max_length, int length) {
    if (length <= 0 || length >= max_length) {
        return -1;
    }
    
    if (fread(buffer, 1, length, fp) != length) {
        return -1;
    }
    buffer[length] = '\0';
    
    printf("  Read binary string: ");
    for (int i = 0; i < length && i < 50; i++) {
        if (isprint((unsigned char)buffer[i])) {
            printf("%c", buffer[i]);
        } else {
            printf("\\x%02X", (unsigned char)buffer[i]);
        }
    }
    printf(" (length: %d)\n", length);
    
    return length;
}

static int read_line_with_binary_check(FILE *fp, char *buffer, int max_length) {
    long start_pos = ftell(fp);
    int i = 0;
    int c;
    
    while (i < max_length - 1) {
        c = fgetc(fp);
        if (c == EOF) break;
        if (c == '\n') break;
        
        // check if binary data is met
        if (c < 32 && c != '\t' && c != '\r') {
            // move back and stop reading
            fseek(fp, -1, SEEK_CUR);
            break;
        }
        
        buffer[i++] = (char)c;
    }
    
    buffer[i] = '\0';
    return i;
}

// read until terminator
int read_until(FILE *fp, char *buffer, int max_length, char terminator) {
    int i = 0;
    char c;
    long start_pos = ftell(fp);

    while (i < max_length - 1) {
        if (fread(&c, 1, 1, fp) != 1) {
            return -1; // EOF
        }
        
        // encounter terminator/ newline
        if (c == terminator || c == '\n') {
            if (i > 0) break; // stop reading until terminator is met
            // if space / change line and no effective data, and then continue
            if (c != '\n') continue;
            else break; // newline is the end of a line, stops even word is empty
        }
        
        buffer[i++] = c;
    }
    
    buffer[i] = '\0';
    
    // if stops due to newline, make sure not to lose it.
    if (c == '\n' && i == 0) {
        //this is empty line, ore we are at the leading char of a line, make sure pointer is afterward the newline
        if (ftell(fp) > start_pos + 1) fseek(fp, -1, SEEK_CUR); // move back if byte is read
    } else if (c != terminator && c != '\n') {
        // if stop not due to the terminator (EOF or buffer overflow), go back 1 byte and let the next function take care terminator
        if (ftell(fp) > start_pos) fseek(fp, -1, SEEK_CUR);
    }

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

// skip space and Newline 
void skip_spaces(FILE *fp) {
    char c;
    long offset;
    
    while (1) {
        offset = ftell(fp);
        if (fread(&c, 1, 1, fp) != 1) break;
        
        if (c != ' ' && c != '\n' && c != '\r') {
            fseek(fp, offset, SEEK_SET); // go back to nonempty site
            break;
        }
    }
}

static void discard_line(FILE *fp) {
    char buffer[MAX_STRING_LENGTH];
    if (fgets(buffer, MAX_STRING_LENGTH, fp) == NULL) {
        // if reaches EOF, noithing
    }
}

static void discard_bytes(FILE *fp, long count) {
    fseek(fp, count, SEEK_CUR);
}

// main parsing function
int sif_open(FILE *fp, SifFile *sif_file) {

    if (!fp || !sif_file) return -1;
    
    SifInfo *info = &sif_file->info;
    memset(info, 0, sizeof(SifInfo));

    memset(sif_file, 0, sizeof(SifFile));
    sif_file->file_ptr = fp;
    
    // initialize SifInfo internal pointer
    memset(&sif_file->info, 0, sizeof(SifInfo));

    info->raman_ex_wavelength = NAN;
    info->calibration_data[0] = '\0';  
    info->calibration_coeff_count = 0;
    info->has_frame_calibrations = 0;

    
    PRINT_NORMAL("=== Starting SIF File Parsing ===\n");
    
    char line_buffer[MAX_STRING_LENGTH];
    
    // Line 1: Magic string
    if (fread(line_buffer, 1, 36, fp) != 36 || strncmp(line_buffer, SIF_MAGIC, 36) != 0) {
        fprintf(stderr, "Error: Not a SIF file or invalid magic string\n");
        return -1;
    }
    PRINT_VERBOSE("✓ Line 1: Valid magic string\n");

    // Line 2: Skip
    discard_line(fp); 

    // Line 3: Structured data
    PRINT_VERBOSE("→ Line 3: Parsing structured data...\n");
    
    long line3_start = ftell(fp);
    PRINT_VERBOSE("  Line 3 starts at offset: 0x%lX\n", line3_start);

    info->sif_version = read_int(fp);
    
    for (int i = 0; i < 3; i++) {
        int temp = read_int(fp);
        PRINT_VERBOSE("  Skipped int %d: %d\n", i, temp);
    }

    info->experiment_time = read_int(fp);
    info->detector_temperature = read_float(fp);
    
    discard_bytes(fp, 10); // Skip 10-byte padding
    
    read_int(fp); // skip 0
    info->exposure_time = read_float(fp);
    info->cycle_time = read_float(fp);
    info->accumulated_cycle_time = read_float(fp);
    info->accumulated_cycles = read_int(fp);
    
    discard_bytes(fp, 2); // skip NULL and space
    
    info->stack_cycle_time = read_float(fp);
    info->pixel_readout_time = read_float(fp);
    
    read_int(fp); // skip 0
    read_int(fp); // skip 1
    info->gain_dac = read_float(fp);
    
    read_int(fp); // skip 0
    read_int(fp); // skip 0
    info->gate_width = read_float(fp);
    
    // Skip 16 int
    for (int i = 0; i < 16; i++) {
        read_int(fp);
    }
    
    info->grating_blaze = read_float(fp);
    
    // read Line 3 until change line
    if (fgets(line_buffer, sizeof(line_buffer), fp) == NULL) return -1;
    
    // Line 4: Detector Type
    if (fgets(info->detector_type, sizeof(info->detector_type), fp) == NULL) return -1;
    trim_trailing_whitespace(info->detector_type);
    PRINT_VERBOSE("✓ Detector Type: '%s'\n", info->detector_type);

    // Line 5: Detector Dimensions
    info->detector_width = read_int(fp);
    info->detector_height = read_int(fp);
    PRINT_VERBOSE("✓ Detector Dimensions: %d x %d\n", info->detector_width, info->detector_height);

    // documents reading
    PRINT_VERBOSE("→ Reading original filename...\n");
    long before_filename = ftell(fp);
    PRINT_VERBOSE("  Before filename, position: 0x%lX\n", before_filename);
    
    // read and discard the first line（"45"）
    if (fgets(line_buffer, sizeof(line_buffer), fp) == NULL) return -1;
    line_buffer[strcspn(line_buffer, "\r\n")] = 0;
    PRINT_VERBOSE("  Discarded short line: '%s'\n", line_buffer);
    
    // read the real filenmae
    if (fgets(info->original_filename, sizeof(info->original_filename), fp) == NULL) return -1;
    trim_trailing_whitespace(info->original_filename);
    PRINT_VERBOSE("✓ Original Filename: '%s'\n", info->original_filename);
    
    PRINT_DEBUG("After original filename parsing, position: 0x%lX\n", ftell(fp));
    
    // skip 20 space 0A newline
    discard_bytes(fp, 2);

    // Line 7: should be "65538 2048"
    int user_text_flag = read_int(fp);
    int user_text_length = read_int(fp);
    PRINT_DEBUG("  User text flag: %d, length: %d\n", user_text_flag, user_text_length);

    long current_pos = ftell(fp);
    PRINT_DEBUG("  After Line 7, position: 0x%lX\n", current_pos);

    // read User Text (if there is)
    if (user_text_length > 0 && user_text_length < sizeof(info->user_text)) {
        if (fread(info->user_text, 1, user_text_length, fp) == user_text_length) {
            info->user_text[user_text_length] = '\0';
            info->user_text_length = user_text_length;
            PRINT_DEBUG("  User text: %d bytes\n", info->user_text_length);
        }
    }
    discard_line(fp); // read change line

    // Line 9: Shutter Time info
    PRINT_VERBOSE("→ Line 9: Reading shutter time...\n");
    long line9_start = ftell(fp);
    PRINT_VERBOSE("  Line 9 starts at offset: 0x%lX\n", line9_start);

    // read markers and verify
    int line9_marker = read_int(fp);
    if (line9_marker != 65538) {
        PRINT_VERBOSE("  ⚠️ Unexpected marker in Line 9: %d (expected 65538)\n", line9_marker);
    }

    PRINT_VERBOSE("  Line 9 marker: %d\n", line9_marker);

    discard_bytes(fp, 8);
    PRINT_VERBOSE("  Skipped 8 bytes\n");

    info->shutter_time[0] = read_float(fp);
    info->shutter_time[1] = read_float(fp);

    // check if the floating point is valid
    if (isnan(info->shutter_time[0]) || isnan(info->shutter_time[1])) {
        PRINT_DEBUG("  ❌ Failed to read shutter time values\n");
        return -1;
    }

    PRINT_VERBOSE("✓ Shutter Time: %.6f, %.6f\n", info->shutter_time[0], info->shutter_time[1]);

    skip_spaces(fp); 
    PRINT_DEBUG("  After Line 9, position: 0x%lX\n", ftell(fp));

    PRINT_VERBOSE("→ Version-specific skipping logic...\n");
    PRINT_VERBOSE("  SIF Version: %d\n", info->sif_version);

    if (info->sif_version >= 65548 && info->sif_version <= 65557) {
        PRINT_VERBOSE("  Version 65548-65557: skipping 2 lines\n");
        for (int i = 0; i < 2; i++) discard_line(fp);
    }
    else if (info->sif_version == 65558) {
        PRINT_VERBOSE("  Version 65558: skipping 5 lines\n");
        for (int i = 0; i < 5; i++) discard_line(fp);
    }
    else if (info->sif_version == 65559 || info->sif_version == 65564) {
        PRINT_VERBOSE("  Version 65559/65564: skipping 8 lines\n");
        for (int i = 0; i < 8; i++) discard_line(fp);
    }
    else if (info->sif_version == 65565) {
        PRINT_VERBOSE("  Version 65565: skipping 15 lines\n");
        for (int i = 0; i < 15; i++) discard_line(fp);
    }
    else if (info->sif_version > 65565) {
        PRINT_VERBOSE("  Version %d > 65565: complex skipping logic\n", info->sif_version);
    
        // Line 10-17: 跳過 8 行
        for (int i = 0; i < 8; i++) {
            discard_line(fp);
        }
        PRINT_VERBOSE("  Skipped 8 lines (Line 10-17)\n");
        
        // Line 18: Spectrograph
        if (fgets(info->spectrograph, sizeof(info->spectrograph), fp) == NULL) return -1;
        trim_trailing_whitespace(info->spectrograph);
        PRINT_VERBOSE("✓ Spectrograph: '%s'\n", info->spectrograph);
        
        // Line 19: Intensifier info -> skip
        discard_line(fp);
        PRINT_VERBOSE("  Skipped intensifier info line\n");
        
        // Line 20-22: read 3 floats (perhaps extra params)
        for (int i = 0; i < 3; i++) {
            read_float(fp); 
        }
        PRINT_VERBOSE("  Read 3 float parameters\n");
        
        // Line 23: Gate Gain
        info->gate_gain = read_float(fp);
        PRINT_VERBOSE("✓ Gate Gain: %.6f\n", info->gate_gain);
        
        // Line 24-25: read 2 floats
        read_float(fp);
        read_float(fp);
        PRINT_VERBOSE("  Read 2 additional float parameters\n");
        
        // Line 26: Gate Delay (picoseconds to seconds)
        float gate_delay_ps = read_float(fp);
        info->gate_delay = gate_delay_ps * 1e-12;
        PRINT_VERBOSE("✓ Gate Delay: %.6f ps (%.2e s)\n", gate_delay_ps, info->gate_delay);
        
        // Line 27: Gate Width (picoseconds to seconds)  
        float gate_width_ps = read_float(fp);
        info->gate_width = gate_width_ps * 1e-12;
        PRINT_VERBOSE("✓ Gate Width: %.6f ps (%.2e s)\n", gate_width_ps, info->gate_width);
        
        // Line 28-35: skip 8 lines
        for (int i = 0; i < 8; i++) {
            discard_line(fp);
        }
        PRINT_DEBUG("  Skipped 8 lines (Line 28-35)\n");
    }

    PRINT_DEBUG("  After version skipping, position: 0x%lX\n", ftell(fp));
    
    PRINT_VERBOSE("→ Reading calibration and additional data...\n");

    info->sif_calb_version = read_int(fp);
    PRINT_NORMAL("✓ SIF Calibration Version: %d\n", info->sif_calb_version);

    if (info->sif_calb_version == 65540) {
        discard_line(fp);
        PRINT_DEBUG("  Skipped line for calibration version 65540\n");
    }

    // calibration data
    char calib_line[MAX_STRING_LENGTH];
    if (fgets(calib_line, sizeof(calib_line), fp) == NULL) {
        PRINT_DEBUG("  Warning: Failed to read calibration data line\n");
        info->calibration_data[0] = '\0'; // set to be an empty string
    } else {
        trim_trailing_whitespace(calib_line);
        PRINT_VERBOSE("✓ Calibration Data: %s\n", calib_line);
        
        // correct copy to struct
        strncpy(info->calibration_data, calib_line, sizeof(info->calibration_data) - 1);
        info->calibration_data[sizeof(info->calibration_data) - 1] = '\0';
    }

    discard_line(fp);
    PRINT_VERBOSE("  Skipped old calibration data\n");

    char extra_line[MAX_STRING_LENGTH];
    if (fgets(extra_line, sizeof(extra_line), fp) == NULL) return -1;
    trim_trailing_whitespace(extra_line);
    PRINT_VERBOSE("  Extra Data: %s\n", extra_line);

    char raman_line[MAX_STRING_LENGTH];
    if (fgets(raman_line, sizeof(raman_line), fp) == NULL) return -1;

    // direct use the line that fgets read, strtod will automatically deal with the Newline
    char *endptr;
    double raman_value = strtod(raman_line, &endptr);

    //check if line parsing is succesful except for Newline
    if (endptr != raman_line) {
        info->raman_ex_wavelength = raman_value;
        PRINT_VERBOSE("✓ Raman Excitation Wavelength: %.2f nm\n", info->raman_ex_wavelength);
    } else {
        info->raman_ex_wavelength = NAN;
        PRINT_DEBUG("  Raman wavelength: N/A ('%s')\n", raman_line);
    }

    PRINT_DEBUG("→ Skipping 4 lines after Raman wavelength...\n");
    for (int i = 0; i < 4; i++) {
        discard_line(fp);
    }

    long after_calib_pos = ftell(fp);
    PRINT_DEBUG("  Skipped 4 lines position: 0x%lX\n", after_calib_pos);

    // Frame Axis, Data Type, Image Axis
    PRINT_VERBOSE("→ Reading axes as simple text lines...\n");

    if (fgets(info->frame_axis, sizeof(info->frame_axis), fp) == NULL) return -1;
    trim_trailing_whitespace(info->frame_axis);
    PRINT_VERBOSE("  Raw Frame Axis: '%s'\n", info->frame_axis);

    if (fgets(info->data_type, sizeof(info->data_type), fp) == NULL) return -1;
    trim_trailing_whitespace(info->data_type);
    PRINT_VERBOSE("  Raw Data Type: '%s'\n", info->data_type);

    if (fgets(info->image_axis, sizeof(info->image_axis), fp) == NULL) return -1;
    trim_trailing_whitespace(info->image_axis);
    PRINT_VERBOSE("  Raw Image Axis: '%s'\n", info->image_axis);

    // Extract the plain text content
    extract_text_part_robust(info->frame_axis, info->frame_axis, sizeof(info->frame_axis)); 
    extract_text_part_robust(info->data_type, info->data_type, sizeof(info->data_type));  

    // keep the text as a temporary variable, temp
    char temp[MAX_STRING_LENGTH];
    extract_text_part_robust(info->image_axis, temp, sizeof(temp));
    PRINT_VERBOSE("  Text part: '%s'\n", temp);

    // keep number from the temp 
    char *number_part = info->image_axis + strlen(temp);
    PRINT_VERBOSE("  Number part: '%s'\n", number_part);

    PRINT_VERBOSE("✓ Frame Axis: '%s'\n", info->frame_axis);
    PRINT_VERBOSE("✓ Data Type: '%s'\n", info->data_type);

    // number parsing
    char *token = strtok(number_part, " ");
    int values[9];
    int value_count = 0;

    while (token && value_count < 9) {
        values[value_count] = atoi(token);
        value_count++;
        token = strtok(NULL, " ");
    }

    int layout_marker = 0;

    if (value_count >= 9) {
        layout_marker = values[0];

        info->number_of_frames = values[5];
        info->number_of_subimages = values[6];
        info->total_length = values[7];
        info->image_length = values[8];
        
    }
    
    // now can safely write the string to image_axis
    strcpy(info->image_axis, temp);
    PRINT_VERBOSE("✓ Image Axis: '%s'\n", info->image_axis);

    PRINT_VERBOSE("✓ Image info:\n");
    PRINT_VERBOSE("  %-15s %d\n", "Frames:", info->number_of_frames);
    PRINT_VERBOSE("  %-15s %d\n", "Subimages:", info->number_of_subimages);
    PRINT_VERBOSE("  %-15s %d\n", "Total length:", info->total_length);
    PRINT_VERBOSE("  %-15s %d\n", "Image length:", info->image_length);

    if (info->number_of_subimages > 0) {
        PRINT_DEBUG("→ Reading %d subimage(s) for binning information...\n", info->number_of_subimages);
        
        info->subimages = malloc(info->number_of_subimages * sizeof(SubImageInfo));
        
        for (int i = 0; i < info->number_of_subimages; i++) {
            SubImageInfo *sub = &info->subimages[i];
            
            int sub_marker = read_int(fp);
            PRINT_DEBUG("  Subimage %d marker: %d\n", i, sub_marker);
            
            // read suimage and binning
            sub->x0 = read_int(fp);
            sub->y1 = read_int(fp);
            sub->x1 = read_int(fp);
            sub->y0 = read_int(fp);
            sub->ybin = read_int(fp);  
            sub->xbin = read_int(fp);  
            
            PRINT_DEBUG("    Area: (%d,%d)-(%d,%d), Binning: %dx%d\n",
                sub->x0, sub->y0, sub->x1, sub->y1, sub->xbin, sub->ybin);
            
            // calculate the subimage size
            sub->width = (1 + sub->x1 - sub->x0) / sub->xbin;
            sub->height = (1 + sub->y1 - sub->y0) / sub->ybin;
            
            PRINT_DEBUG("    Size: %dx%d\n", sub->width, sub->height);
            
            // set the global scope binning 
            if (i == 0) {
                info->xbin = sub->xbin;
                info->ybin = sub->ybin;
                info->image_width = sub->width;
                info->image_height = sub->height;
            }
        }
        
        PRINT_VERBOSE("✓ Final image configuration:\n");
        PRINT_VERBOSE("  Size: %dx%d pixels\n", info->image_width, info->image_height);
        PRINT_VERBOSE("  Binning: %dx%d\n", info->xbin, info->ybin);
    }

    PRINT_DEBUG("  After layout parsing, position: 0x%lX\n", ftell(fp));
   
    PRINT_DEBUG("→ Reading timestamps for %d frames...\n", info->number_of_frames);

    discard_line(fp);
    PRINT_DEBUG("  After skipping a line, position: 0x%lX\n", ftell(fp));

    // read timestamps
    if (info->number_of_frames > 0) {
        info->timestamps = malloc(info->number_of_frames * sizeof(int64_t));
        if (!info->timestamps) {
            PRINT_DEBUG("❌ Failed to allocate memory for timestamps\n");
            return -1;
        }
        
        for (int f = 0; f < info->number_of_frames; f++) {
            char timestamp_str[64];
            if (fgets(timestamp_str, sizeof(timestamp_str), fp) == NULL) {
                PRINT_DEBUG("❌ Failed to read timestamp for frame %d\n", f);
                info->timestamps[f] = 0;
            } else {
                info->timestamps[f] = atoll(timestamp_str);
                PRINT_VERBOSE("  Frame %d timestamp: %" PRId64 "\n", f, info->timestamps[f]);
            }
        }
    }
    PRINT_DEBUG("  After timestamps, position: 0x%lX\n", ftell(fp));

    PRINT_VERBOSE("→ Determining data offset...\n");

    long before_data = ftell(fp);
    info->data_offset = before_data; // default data offset

    // check extra flags or not
    char line[256];

    PRINT_DEBUG("→ Reading data flag line at position: 0x%lX\n", before_data);

    if (fgets(line, sizeof(line), fp) != NULL) {
        // kill NewLine
        line[strcspn(line, "\n")] = '\0';
        
        PRINT_VERBOSE("  Raw line content: '%s' (length: %lu)\n", line, strlen(line));
        
        // int parsing
        int data_flag = 0;
        if (sscanf(line, "%d", &data_flag) == 1) {
            PRINT_VERBOSE("  Parsed data flag: %d\n", data_flag);
            
            if (data_flag == 0) {
                info->data_offset = ftell(fp);  // now moves to the first char of the line
                PRINT_DEBUG("✓ Data starts after flag 0 at offset: 0x%lX\n", info->data_offset);
            } else if (data_flag == 1 && info->sif_version == 65567) {
                PRINT_DEBUG("  SIF 65567: skipping %d additional lines\n", info->number_of_frames);
                for (int i = 0; i < info->number_of_frames; i++) {
                    if (fgets(line, sizeof(line), fp) == NULL) break;
                    PRINT_DEBUG("    Skipped line %d: '%s'\n", i, line);
                }
                info->data_offset = ftell(fp);
                PRINT_DEBUG("✓ Data starts after version-specific data at offset: 0x%lX\n", info->data_offset);
            } else {
                // other condition, go back to the original position
                fseek(fp, before_data, SEEK_SET);
                PRINT_DEBUG("✓ Data starts at original offset: 0x%lX\n", info->data_offset);
            }
        } else {
            // int parsing fails
            PRINT_DEBUG("  Failed to parse integer from line\n");
            fseek(fp, before_data, SEEK_SET);
            PRINT_DEBUG("✓ Data starts at original offset: 0x%lX\n", info->data_offset);
        }
    } else {
        PRINT_DEBUG("  Failed to read line\n");
        fseek(fp, before_data, SEEK_SET);
        PRINT_DEBUG("✓ Data starts at original offset: 0x%lX\n", info->data_offset);
    }
        
    PRINT_VERBOSE("→ Initializing SifFile structure and tiles...\n");

    sif_file->frame_count = info->number_of_frames;
    sif_file->tile_count = info->number_of_frames;
    
    // allocate and initiate tiles
    if (sif_file->tile_count > 0) {
        sif_file->tiles = malloc(sif_file->tile_count * sizeof(ImageTile));
        if (sif_file->tiles) {
            // compute the size of each frame（width*height*no_subimages*4）
            int pixels_per_frame = info->image_width * info->image_height * info->number_of_subimages;
            int bytes_per_pixel = 4; // 32-bit float 
            
            PRINT_VERBOSE("  Tile configuration:\n");
            PRINT_VERBOSE("    Pixels per frame: %d\n", pixels_per_frame);
            PRINT_VERBOSE("    Bytes per pixel: %d\n", bytes_per_pixel);
            PRINT_VERBOSE("    Total bytes per frame: %d\n", pixels_per_frame * bytes_per_pixel);
            
            for (int f = 0; f < sif_file->tile_count; f++) {
                sif_file->tiles[f].offset = info->data_offset + f * pixels_per_frame * bytes_per_pixel;
                sif_file->tiles[f].width = info->image_width;
                sif_file->tiles[f].height = info->image_height;
                sif_file->tiles[f].frame_index = f;
                
                PRINT_VERBOSE("    Tile %d: offset=0x%08lX, size=%dx%d\n", 
                    f, sif_file->tiles[f].offset,
                    sif_file->tiles[f].width, sif_file->tiles[f].height);
            }
            PRINT_VERBOSE("✓ Allocated %d image tiles\n", sif_file->tile_count);
        } else {
            printf("❌ Failed to allocate memory for tiles\n");
            return -1;
        }
    }

    PRINT_VERBOSE("  Before extract_user_text:\n");
    PRINT_VERBOSE("    user_text pointer: %p\n", info->user_text);
    PRINT_VERBOSE("    user_text[0]: 0x%02X\n", (unsigned char)info->user_text[0]);
    PRINT_VERBOSE("    strlen(user_text): %lu\n", strlen(info->user_text));
    PRINT_VERBOSE("    user_text_length: %d\n", info->user_text_length);

    // manual check the first 10 bytes
    PRINT_VERBOSE("    First 10 bytes: ");
    for (int i = 0; i < 10 && i < user_text_length; i++) {
        PRINT_VERBOSE("%02X ", (unsigned char)info->user_text[i]);
    }
    PRINT_VERBOSE("\n");

    // clean and retriecve the calibration data
    extract_user_text(info);

    PRINT_VERBOSE("✓ SIF file parsing successfully");

    return 0;
}


void extract_frame_calibrations(SifInfo *info, int start_pos) {
    if (!info || !info->user_text || start_pos < 0 || start_pos >= info->user_text_length) {
        return;
    }
    
    PRINT_VERBOSE("→ Extracting frame calibration data from position %d\n", start_pos);
    
    // copy user_text to a mutable buffer
    char* text_copy = malloc(info->user_text_length + 1);
    if (!text_copy) {
        printf("  Memory allocation failed\n");
        return;
    }
    memcpy(text_copy, info->user_text, info->user_text_length);
    text_copy[info->user_text_length] = '\0';
    
    char* current_pos = text_copy + start_pos;
    
    // retrieving data for each frame
    for (int frame = 1; frame <= info->number_of_frames; frame++) {
        // construct the key char "Calibration data for frame X"
        char target[64];
        snprintf(target, sizeof(target), "Calibration data for frame %d", frame);
        
        // search for this target
        char* frame_start = strstr(current_pos, target);
        if (!frame_start) {
            printf("  ✗ Calibration data for frame %d not found\n", frame);
            continue;
        }
        
        //move to the data part (skipping key words and :)
        char* data_start = frame_start + strlen(target);
        
        // skip space and :
        while (*data_start && (isspace((unsigned char)*data_start) || *data_start == ':')) {
            data_start++;
        }
        
        if (!*data_start) {
            printf("  ✗ No data after calibration marker for frame %d\n", frame);
            continue;
        }
        
        // find the end of line and data close
        char* data_end = data_start;
        while (*data_end && *data_end != '\n' && *data_end != '\r') {
            data_end++;
        }
        
        // retrieve the data
        size_t data_len = data_end - data_start;
        char data_str[256];
        if (data_len >= sizeof(data_str)) {
            data_len = sizeof(data_str) - 1;
        }
        memcpy(data_str, data_start, data_len);
        data_str[data_len] = '\0';
        
        PRINT_VERBOSE("  Frame %d calibration data: '%s'\n", frame, data_str);
        
        // paring coefficients
        parse_frame_calibration_coefficients(info, frame, data_str);
        
        // move to the next position
        current_pos = data_end;
    }
    
    free(text_copy);
    info->has_frame_calibrations = 1;
}

void parse_frame_calibration_coefficients(SifInfo *info, int frame, const char* data_str) {
    if (!info || !data_str) {
        printf("    Error: Invalid parameters for frame %d\n", frame);
        return;
    }
    
    PRINT_VERBOSE("    Parsing coefficients for frame %d: '%s'\n", frame, data_str);
    
    // copy string for modification (strtok will modify the original string)
    char* data_copy = strdup(data_str);
    if (!data_copy) {
        PRINT_VERBOSE("    Memory allocation failed for frame %d\n", frame);
        return;
    }
    
    char* token;
    char* rest = data_copy;
    int coeff_count = 0;
    double coefficients[20]; // assume 20 to the most
    
    // trim the leading and trailling whitespace
    char* trimmed = rest;
    while (isspace((unsigned char)*trimmed)) trimmed++;
    char* end = trimmed + strlen(trimmed) - 1;
    while (end > trimmed && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    
    rest = trimmed;
    
    // split by ',' 
    while ((token = strtok_r(rest, ",", &rest))) {
        if (coeff_count < 20) {
            // 去除 token 的空白
            char* token_trim = token;
            while (isspace((unsigned char)*token_trim)) token_trim++;
            char* token_end = token_trim + strlen(token_trim) - 1;
            while (token_end > token_trim && isspace((unsigned char)*token_end)) token_end--;
            *(token_end + 1) = '\0';
            
            if (strlen(token_trim) > 0) {
                char* endptr;
                double value = strtod(token_trim, &endptr);
                
                if (endptr != token_trim) { // successfully conversion
                    coefficients[coeff_count++] = value;
                    PRINT_VERBOSE("      Coefficient %d: %f\n", coeff_count, value);
                } else {
                    printf("      Warning: Failed to parse '%s' as float\n", token_trim);
                }
            }
        }
    }
    
    // save coefficients 
    if (coeff_count > 0) {
        // 確保不會超出陣列範圍
        if (frame <= MAX_FRAMES) {
            info->frame_calibrations[frame-1].coeff_count = coeff_count;
            memcpy(info->frame_calibrations[frame-1].coefficients, coefficients, 
                   coeff_count * sizeof(double));
            PRINT_VERBOSE("    ✓ Frame %d: %d coefficients parsed and saved\n", frame, coeff_count);
        } else {
            PRINT_VERBOSE("    ✗ Frame %d: frame number exceeds maximum (%d)\n", frame, MAX_FRAMES);
        }
    } else {
        PRINT_VERBOSE("    ✗ Frame %d: no valid coefficients found\n", frame);
    }
    
    free(data_copy);
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
        
    } else if (info->calibration_coeff_count > 0) {
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
            
            for (int j = 0; j < info->calibration_coeff_count; j++) {
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

void parse_calibration_coefficients(SifInfo *info) {
    if (!info || info->calibration_data[0] == '\0') {
        info->calibration_coeff_count = 0;  // marked as 
        return;
    }
    
    PRINT_VERBOSE("→ Parsing calibration coefficients from: '%s'\n", info->calibration_data);
    
    char* data_copy = strdup(info->calibration_data);
    if (!data_copy) {
        info->calibration_coeff_count = 0;
        return;
    }
    
    char* token;
    char* rest = data_copy;
    int coeff_count = 0;
    double coefficients[10];
    
    // trim spaces
    char* trimmed = rest;
    while (isspace((unsigned char)*trimmed)) trimmed++;
    char* end = trimmed + strlen(trimmed) - 1;
    while (end > trimmed && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    
    rest = trimmed;
    
    // split by spaces
    while ((token = strtok_r(rest, " \t", &rest))) {
        if (coeff_count < 10) {
            char* endptr;
            double value = strtod(token, &endptr);
            
            if (endptr != token) { // successful conversion
                coefficients[coeff_count++] = value;
                PRINT_VERBOSE("    Coefficient %d: %f\n", coeff_count, value);
            } else {
                PRINT_VERBOSE("    Failed to parse '%s' as float\n", token);
                free(data_copy);
                info->calibration_coeff_count = 0;
                return;
            }
        }
    }
    
    free(data_copy);
    
    if (coeff_count > 0) {
        // save coefficients
        info->calibration_coeff_count = coeff_count;
        memcpy(info->calibration_coefficients, coefficients, coeff_count * sizeof(double));

    } else {
        info->calibration_coeff_count = 0; // fails
    }
}

void extract_user_text(SifInfo *info) {
    if (!info || !info->user_text || info->user_text_length == 0) {
        printf("  Skip: no user text to process\n");
        return;
    }
    
    PRINT_VERBOSE("→ extract_user_text analysis:\n");
    PRINT_VERBOSE("  user_text_length: %d\n", info->user_text_length);
    PRINT_VERBOSE("  calibration_data: '%s'\n", info->calibration_data);
    
    // search for "Calibration data for" - in the first 20 bytes of user_text
    const char* target = "Calibration data for";
    int found = 0;
    
    int search_limit = (info->user_text_length < 20) ? info->user_text_length : 20;
    
    for (int i = 0; i <= search_limit - strlen(target); i++) {
        if (strncmp(&info->user_text[i], target, strlen(target)) == 0) {
            PRINT_VERBOSE("  ✓ Found '%s' in first %d bytes of user_text at position %d\n", 
                   target, search_limit, i);
            found = 1;
            
            extract_frame_calibrations(info, i);
            
            info->calibration_data[0] = '\0';
            info->calibration_coeff_count = 0;
            
            break;
        }
    }
    
    if (!found) {
        PRINT_VERBOSE("  ✗ '%s' not found in first %d bytes of user_text\n", target, search_limit);
        
        // processing the present calibration_data
        if (info->calibration_data[0] != '\0') {
            PRINT_VERBOSE("  calibration_data is a string: '%s'\n", info->calibration_data);
            
            // 嘗試解析校準係數
            parse_calibration_coefficients(info); 
            
            // check if conversion is succesful via calibration_coeff_count
            if (info->calibration_coeff_count > 0) {
                PRINT_VERBOSE("  ✓ Successfully parsed %d calibration coefficients\n", 
                       info->calibration_coeff_count);
            } else {
                // fails to parse
                PRINT_VERBOSE("  ✗ Failed to parse calibration coefficients, clearing data\n");
                info->calibration_data[0] = '\0';
                info->calibration_coeff_count = 0;
            }
        } else {
            PRINT_VERBOSE("  calibration_data is empty or not a string, clearing\n");
            info->calibration_coeff_count = 0;
        }
    }
    
    info->user_text_processed = 1;
    PRINT_VERBOSE("✓ User text processing completed\n");
}

// bytes swap
static void swap_float_array_endian(float *data, int count) {
    for (int i = 0; i < count; i++) {
        uint32_t temp;
        memcpy(&temp, &data[i], sizeof(uint32_t));
        temp = ((temp & 0xFF) << 24) | ((temp & 0xFF00) << 8) |
               ((temp & 0xFF0000) >> 8) | ((temp & 0xFF000000) >> 24);
        memcpy(&data[i], &temp, sizeof(float));
    }
}

// data loading
/*
int sif_load_all_frames(SifFile *sif_file, int enable_byte_swap) {
    PRINT_VERBOSE("=== ENTERING sif_load_all_frames ===\n");
    
    if (!sif_file) {
        PRINT_VERBOSE("❌ sif_file is NULL\n");
        return -1;
    }
    if (!sif_file->file_ptr) {
        PRINT_VERBOSE("❌ sif_file->file_ptr is NULL\n");
        return -1;
    }
    if (sif_file->frame_count == 0) {
        PRINT_VERBOSE("❌ sif_file->frame_count is 0\n");
        return -1;
    }
    
    PRINT_VERBOSE("  frame_count: %d\n", sif_file->frame_count);
    PRINT_VERBOSE("  tiles pointer: %p\n", sif_file->tiles);
    
    if (sif_file->tiles) {
        PRINT_VERBOSE("  tile[0].width: %d, height: %d, offset: 0x%08lX\n", 
               sif_file->tiles[0].width, sif_file->tiles[0].height, sif_file->tiles[0].offset);
    }
    
    if (sif_file->data_loaded) {
        PRINT_VERBOSE("→ Unloading previous data\n");
        sif_unload_data(sif_file);
    }
    
    int frame_size = sif_file->tiles[0].width * sif_file->tiles[0].height;
    int total_pixels = sif_file->frame_count * frame_size;
    
    PRINT_VERBOSE("→ Allocating memory for %d frames, %d pixels each (%d total)\n", 
           sif_file->frame_count, frame_size, total_pixels);
    
    // allocate memory
    sif_file->frame_data = malloc(total_pixels * sizeof(float));
    if (!sif_file->frame_data) {
        PRINT_VERBOSE("❌ Failed to allocate memory for %lu bytes\n", total_pixels * sizeof(float));
        return -1;
    }
    PRINT_VERBOSE("✓ Allocated frame_data at %p\n", sif_file->frame_data);
    
    FILE *fp = sif_file->file_ptr;
    
    // direct retrieve all data
    for (int i = 0; i < sif_file->frame_count; i++) {
        PRINT_VERBOSE("→ Loading frame %d\n", i);
        
        if (!sif_file->tiles || i >= sif_file->frame_count) {
            PRINT_VERBOSE("❌ Invalid tile access at index %d\n", i);
            break;
        }
        
        long offset = sif_file->tiles[i].offset;
        PRINT_VERBOSE("  Seeking to offset: 0x%08lX\n", offset);
        
        if (fseek(fp, offset, SEEK_SET) != 0) {
            PRINT_VERBOSE("❌ Failed to seek to offset 0x%08lX\n", offset);
            continue;
        }
        
        float *frame_start = sif_file->frame_data + i * frame_size;
        PRINT_VERBOSE("  Frame data starts at %p\n", frame_start);
        
        size_t read_count = fread(frame_start, sizeof(float), frame_size, fp);
        PRINT_VERBOSE("  Read %zu/%d pixels\n", read_count, frame_size);
        
        if (read_count != frame_size) {
            PRINT_VERBOSE("⚠️ Frame %d: Only read %zu/%d pixels\n", i, read_count, frame_size);
        }
        
        // bytes swapping 
        if (enable_byte_swap) {
            PRINT_VERBOSE("  Applying byte swap\n");
            swap_float_array_endian(frame_start, frame_size);
        }
        
        // debug the first frame
        if (i == 0 && read_count > 0) {
            PRINT_VERBOSE("  Frame 0 first 5 values: %.1f, %.1f, %.1f, %.1f, %.1f\n",
                   frame_start[0], frame_start[1], frame_start[2], frame_start[3], frame_start[4]);
        }
    }
    
    sif_file->data_loaded = 1;
    PRINT_VERBOSE("✓ Successfully loaded %d frames\n", sif_file->frame_count);
    PRINT_VERBOSE("=== EXITING sif_load_all_frames ===\n");
    return 0;
}
 */

int sif_load_all_frames(SifFile *sif_file, int enable_byte_swap) {
    if (!sif_file || !sif_file->file_ptr || sif_file->frame_count == 0) {
        return -1;
    }
    
    if (sif_file->data_loaded) {
        sif_unload_data(sif_file);
    }
    
    int frame_size = sif_file->tiles[0].width * sif_file->tiles[0].height;
    int total_pixels = sif_file->frame_count * frame_size;
    
    PRINT_VERBOSE("→ Loading frame data%s:\n", enable_byte_swap ? " with endian correction" : "");
    PRINT_VERBOSE("  Frame size: %d x %d = %d pixels\n", 
           sif_file->tiles[0].width, sif_file->tiles[0].height, frame_size);
    PRINT_VERBOSE("  Byte swap: %s\n", enable_byte_swap ? "ENABLED" : "DISABLED");
    
    // allocate memory
    sif_file->frame_data = malloc(total_pixels * sizeof(float));
    if (!sif_file->frame_data) {
        printf("❌ Failed to allocate memory\n");
        return -1;
    }
    
    FILE *fp = sif_file->file_ptr;
    
    // direct retrieve all data
    for (int i = 0; i < sif_file->frame_count; i++) {
        long offset = sif_file->tiles[i].offset;
        fseek(fp, offset, SEEK_SET);
        
        float *frame_start = sif_file->frame_data + i * frame_size;
        size_t read_count = fread(frame_start, sizeof(float), frame_size, fp);
        
        if (read_count != frame_size) {
            printf("⚠️ Frame %d: Only read %zu/%d pixels\n", i, read_count, frame_size);
        }
        
        // bytes swapping 
        if (enable_byte_swap) {
            swap_float_array_endian(frame_start, frame_size);
        }
        
        // debug the first frame
        if (i == 0) {
            PRINT_VERBOSE("  Frame 0%s:\n", enable_byte_swap ? " after byte swap" : " (raw)");
            
            // re-read original bytes to compare
            fseek(fp, offset, SEEK_SET);
            unsigned char raw_bytes[40];
            fread(raw_bytes, 1, 40, fp);
            
            PRINT_VERBOSE("    Original bytes -> Values:\n");
            for (int j = 0; j < 10 && j < frame_size; j++) {
                PRINT_VERBOSE("    Pixel %d: %02X %02X %02X %02X -> %.1f\n",
                       j, raw_bytes[j*4], raw_bytes[j*4+1], 
                       raw_bytes[j*4+2], raw_bytes[j*4+3], frame_start[j]);
            }
            
            // 檢查是否有合理的值
            int valid_count = 0;
            for (int j = 0; j < frame_size; j++) {
                if (frame_start[j] > 600.0f && frame_start[j] < 700.0f) {
                    valid_count++;
                    if (valid_count <= 3) {
                        PRINT_VERBOSE("    Valid value at pixel %d: %.1f\n", j, frame_start[j]);
                    }
                }
            }
            PRINT_VERBOSE("    Total valid values (600-700 range): %d/%d\n", valid_count, frame_size);
        }
    }
    
    sif_file->data_loaded = 1;
    PRINT_VERBOSE("✓ Loaded %d frames%s\n", sif_file->frame_count, 
           enable_byte_swap ? " with endian correction" : "");
    return 0;
}
   

int sif_load_single_frame(SifFile *sif_file, int frame_index) {
    if (!sif_file || !sif_file->file_ptr || sif_file->frame_count == 0) {
        return -1;
    }
    
    if (frame_index < 0 || frame_index >= sif_file->frame_count) {
        printf("❌ Frame index %d out of range (0-%d)\n", 
               frame_index, sif_file->frame_count - 1);
        return -1;
    }
    
    // 如果已經加載了數據，先釋放
    if (sif_file->data_loaded) {
        sif_unload_data(sif_file);
    }
    
    int frame_size = sif_file->tiles[0].width * sif_file->tiles[0].height;
    int total_pixels = frame_size;  // 只加載一幀
    
    PRINT_VERBOSE("→ Loading single frame %d:\n", frame_index);
    PRINT_VERBOSE("  Frame size: %d x %d = %d pixels\n", 
           sif_file->tiles[0].width, sif_file->tiles[0].height, frame_size);
    
    // 分配記憶體
    sif_file->frame_data = malloc(total_pixels * sizeof(float));
    if (!sif_file->frame_data) {
        printf("❌ Failed to allocate memory for frame %d\n", frame_index);
        return -1;
    }
    
    FILE *fp = sif_file->file_ptr;
    
    // 讀取指定幀的數據
    long offset = sif_file->tiles[frame_index].offset;
    fseek(fp, offset, SEEK_SET);
    
    size_t read_count = fread(sif_file->frame_data, sizeof(float), frame_size, fp);
    
    if (read_count != frame_size) {
        printf("⚠️ Frame %d: Only read %zu/%d pixels\n", frame_index, read_count, frame_size);
        free(sif_file->frame_data);
        sif_file->frame_data = NULL;
        return -1;
    }
    
    // 字節序交換（如果需要）
    // 注意：這裡假設不需要字節序交換，因為通常 SIF 文件是小端序
    // 如果需要，可以添加 enable_byte_swap 參數
    
    PRINT_VERBOSE("✓ Loaded frame %d (%d pixels)\n", frame_index, frame_size);
    
    sif_file->data_loaded = 1;
    return 0;
}

float* sif_get_frame_data(SifFile *sif_file, int frame_index) {
    if (!sif_file || !sif_file->frame_data || 
        frame_index < 0 || frame_index >= sif_file->frame_count) {
        return NULL;
    }
    
    int frame_size = sif_file->tiles[0].width * sif_file->tiles[0].height;
    return sif_file->frame_data + frame_index * frame_size;
}

float sif_get_pixel_value(SifFile *sif_file, int frame_index, int row, int col) {
    if (!sif_file || !sif_file->frame_data) {
        return 0.0f;
    }
    
    if (frame_index < 0 || frame_index >= sif_file->frame_count ||
        row < 0 || row >= sif_file->tiles[0].height ||
        col < 0 || col >= sif_file->tiles[0].width) {
        return 0.0f;
    }
    
    int frame_size = sif_file->tiles[0].width * sif_file->tiles[0].height;
    int pixel_index = frame_index * frame_size + row * sif_file->tiles[0].width + col;
    
    return sif_file->frame_data[pixel_index];
}

// copy frame data to buffer of the user
int sif_copy_frame_data(SifFile *sif_file, int frame_index, float *output_buffer) {
    if (!sif_file || !sif_file->frame_data || !output_buffer) {
        return -1;
    }
    
    if (frame_index < 0 || frame_index >= sif_file->frame_count) {
        return -1;
    }
    
    int frame_size = sif_file->tiles[0].width * sif_file->tiles[0].height;
    float *frame_start = sif_get_frame_data(sif_file, frame_index);
    
    memcpy(output_buffer, frame_start, frame_size * sizeof(float));
    return 0;
}

void sif_unload_data(SifFile *sif_file) {
    if (!sif_file) return;
    
    if (sif_file->frame_data) {
        free(sif_file->frame_data);
        sif_file->frame_data = NULL;
    }
    sif_file->data_loaded = 0;
}

static void cleanup_sif_info(SifInfo *info) {
    if (!info) return;
    
    if (info->timestamps) {
        free(info->timestamps);
        info->timestamps = NULL;
    }
    
    if (info->calibration_data) {
        info->calibration_data[0] = '\0';  
    }
}
void sif_close(SifFile *sif_file) {
    if (!sif_file) return;
    
    PRINT_VERBOSE("→ Closing SIF file and freeing resources...\n");
    
    // 釋放幀數據
    sif_unload_data(sif_file);
    
    // 釋放 tiles 數組
    if (sif_file->tiles) {
        free(sif_file->tiles);
        sif_file->tiles = NULL;
        PRINT_VERBOSE("✓ Freed tiles array\n");
    }
    
    // clean the dynamic memory of info struct 
    cleanup_sif_info(&sif_file->info);
    
    // reset counter
    sif_file->frame_count = 0;
    sif_file->tile_count = 0;
    sif_file->data_loaded = 0;
    
    // note: not close file_ptr，this will be handled by the user in debugging
    sif_file->file_ptr = NULL;
    
    PRINT_VERBOSE("✓ SIF file closed successfully\n");
}
