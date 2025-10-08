
#include <stdio.h>
#include "sif_parser.h"
#include "sif_utils.h"

//  ./bin/read_sif /home/tim/Documents/AS/data/andor/20250917/1OD_500uW_sapphire_200umFiber_.sif
//  ./bin/read_sif /home/tim/Documents/AS/data/andor/20250908/monochrom_430_700_10_LED_2.sif

int main(int argc, char *argv[]) {
    // 默認級別
    SifVerboseLevel level = SIF_NORMAL;
    
    // 根據命令行參數調整
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            level = SIF_QUIET;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            level = SIF_VERBOSE;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) {
            level = SIF_DEBUG;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--silent") == 0) {
            level = SIF_SILENT;
        }
    }

    const char *filename = argv[1];

    // 設置輸出級別（可以在 main 函數開始時設置）
    sif_set_verbose_level(level);  // 或者 SIF_QUIET, SIF_VERBOSE 等

    PRINT_NORMAL("======Complete File Analysis:======\n");
    FILE *fp = fopen(filename, "rb");
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
                    
                    // 檢查數據範圍
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
                    // 2D 數據：number_of_frames × width
                    PRINT_NORMAL("Retrieved 2D calibration data (%d frames × %d pixels):\n", 
                        sif_file.info.number_of_frames, sif_file.info.detector_width);
                    
                    for (int frame = 0; frame < sif_file.info.number_of_frames; frame++) {
                        PRINT_NORMAL("  Frame %d: ", frame + 1);
                        for (int pixel = 0; pixel < 5; pixel++) { // 只顯示前5個像素
                            PRINT_NORMAL("%f ", calibration[frame * sif_file.info.detector_width + pixel]);
                        }
                        PRINT_NORMAL("...\n");
                    }
                } else {
                    // 1D 數據
                    PRINT_NORMAL("Retrieved 1D calibration data (%d pixels):\n", calibration_size);
                    
                    // 印出頭5個值
                    PRINT_NORMAL("    - First 5: ");
                    for (int i = 0; i < 5 && i < calibration_size; i++) {
                        PRINT_NORMAL("%f ", calibration[i]);
                    }
                    PRINT_NORMAL("\n");

                    // 印出末5個值
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
