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
 
#include <stdio.h>
#include "sif_parser.h"
#include "sif_utils.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename> [options]\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    SifVerboseLevel level = SIF_NORMAL;

    // process after the second arg
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-q") == 0) level = SIF_QUIET;
        else if (strcmp(argv[i], "-v") == 0) level = SIF_VERBOSE;
        else if (strcmp(argv[i], "-d") == 0) level = SIF_DEBUG;
        else if (strcmp(argv[i], "-s") == 0) level = SIF_SILENT;
    }

    // set output level
    sif_set_verbose_level(level);  // or SIF_QUIET, SIF_VERBOSE etc
    
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("fopen fails");
        return -1;
    }

    PRINT_NORMAL("======Complete File Analysis:======\n");

    if (fp) {
        SifFile sif_file;
        if (sif_open(fp, &sif_file) == 0) {
            PRINT_NORMAL("\n");
            print_sif_info_summary(&sif_file.info);
            PRINT_NORMAL("\n");
            print_sif_file_structure(&sif_file);
            PRINT_NORMAL("\n");
            
            PRINT_NORMAL("Frames: %d, Image size: %dx%d\n", 
                sif_file.tile_count, 
                sif_file.tiles[0].width, 
                sif_file.tiles[0].height);

            //sif_load_all_frames(SifFile *sif_file, int byte_swap)
            if (sif_load_all_frames(&sif_file, 0) ==  0) {
                float *frame0 = sif_get_frame_data(&sif_file, 0);
                if (frame0) {
                    PRINT_NORMAL("Final result - Frame 0 first 20 pixels:\n");
                    for (int i = 0; i < 20; i++) {
                        PRINT_NORMAL("  Pixel %d: %.1f\n", i, frame0[i]);
                    }
                    
                    // check data value range
                    float min_val = frame0[0], max_val = frame0[0];
                    for (int i = 1; i < 1024; i++) {
                        if (frame0[i] < min_val) min_val = frame0[i];
                        if (frame0[i] > max_val) max_val = frame0[i];
                    }
                    PRINT_NORMAL("Data range: %.1f to %.1f\n", min_val, max_val);
                }
            }

            int calibration_size;
            double* calibration = retrieve_calibration(&sif_file.info, &calibration_size);
            
            if (calibration) {
                if (sif_file.info.has_frame_calibrations) {
                    // 2D data：number_of_frames × width
                    PRINT_NORMAL("Retrieved 2D calibration data (%d frames × %d pixels):\n", 
                        sif_file.info.number_of_frames, sif_file.info.detector_width);
                    
                    for (int frame = 0; frame < sif_file.info.number_of_frames; frame++) {
                        PRINT_NORMAL("  Frame %d: ", frame + 1);
                        for (int pixel = 0; pixel < 5; pixel++) { // shows 5 pixels only
                            PRINT_NORMAL("%f ", calibration[frame * sif_file.info.detector_width + pixel]);
                        }
                        PRINT_NORMAL("...\n");
                    }
                } else {
                    // 1D data
                    PRINT_NORMAL("Retrieved 1D calibration data (%d pixels):\n", calibration_size);
                    
                    // print the first 5 values
                    PRINT_NORMAL("    - First 5: ");
                    for (int i = 0; i < 5 && i < calibration_size; i++) {
                        PRINT_NORMAL("%f ", calibration[i]);
                    }
                    PRINT_NORMAL("\n");

                    // print the last 5
                    PRINT_NORMAL("    - Last 5:  ");
                    int start = (calibration_size > 5) ? calibration_size - 5 : 0;
                    for (int i = start; i < calibration_size; i++) {
                        PRINT_NORMAL("%f ", calibration[i]);
                    }
                    PRINT_NORMAL("\n");
                }
                
                free(calibration);
            } else {
                PRINT_NORMAL("No calibration data available\n");
            }
            sif_close(&sif_file);
        } else {
            PRINT_SILENT("Error: Failed to parse SIF file\n");
        }
        fclose(fp);
    } else {
        PRINT_SILENT("Error: Cannot open file %s\n", filename);
    }
    return 0;
}
