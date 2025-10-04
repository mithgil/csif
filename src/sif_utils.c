#include "sif_utils.h"
#include <ctype.h>
#include <inttypes.h>  // 添加這個頭文件

// 打印 SIF 文件的第一行
void print_sif_first_line(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        printf("Error: Cannot open file %s\n", filename);
        return;
    }
    
    char line[256];
    if (fgets(line, sizeof(line), fp)) {
        printf("First line of %s:\n", filename);
        printf("Hex: ");
        for (int i = 0; i < strlen(line) && i < 50; i++) {
            printf("%02X ", (unsigned char)line[i]);
        }
        printf("\nText: ");
        for (int i = 0; i < strlen(line) && i < 50; i++) {
            if (isprint((unsigned char)line[i])) {
                printf("%c", line[i]);
            } else {
                printf(".");
            }
        }
        printf("\n");
    }
    
    fclose(fp);
}

// 打印 SIF 文件的前 N 行
void print_sif_first_lines(const char *filename, int line_count) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        printf("Error: Cannot open file %s\n", filename);
        return;
    }
    
    printf("First %d lines of %s:\n", line_count, filename);
    printf("========================================\n");
    
    char line[512];
    for (int i = 0; i < line_count; i++) {
        if (!fgets(line, sizeof(line), fp)) {
            break;
        }
        
        printf("Line %2d (offset 0x%08lX):\n", i + 1, ftell(fp) - strlen(line));
        
        // 顯示十六進制
        printf("  Hex: ");
        int line_len = strlen(line);
        for (int j = 0; j < line_len && j < 60; j++) {
            printf("%02X ", (unsigned char)line[j]);
            if ((j + 1) % 16 == 0) printf("\n       ");
        }
        printf("\n");
        
        // 顯示文本內容
        printf("  Text: ");
        for (int j = 0; j < line_len && j < 60; j++) {
            unsigned char c = line[j];
            if (isprint(c) && c != '\r' && c != '\n') {
                printf("%c", c);
            } else if (c == '\r' || c == '\n') {
                printf("\\n");
                if (c == '\r' && j + 1 < line_len && line[j + 1] == '\n') {
                    j++; // 跳過 \n
                }
                break;
            } else {
                printf(".");
            }
        }
        printf("\n");
        
        // 如果是第一行，特別檢查魔數
        if (i == 0) {
            if (strncmp(line, SIF_MAGIC, strlen(SIF_MAGIC)) == 0) {
                printf("  ✓ Valid SIF magic string found\n");
            } else {
                printf("  ✗ Invalid SIF magic string\n");
            }
        }
        
        printf("\n");
    }
    
    fclose(fp);
}

// 打印 SIF 文件資訊摘要
void print_sif_info_summary(const SifInfo *info) {
    if (!info) {
        printf("Error: No SIF info provided\n");
        return;
    }
    
    printf("SIF File Information Summary:\n");
    printf("=============================\n");
    printf("Detector Type: %s\n", info->detector_type);
    printf("Original Filename: %s\n", info->original_filename);
    printf("Spectrograph: %s\n", info->spectrograph);
    printf("SIF Version: %d\n", info->sif_version);
    printf("SIF Calibration Version: %d\n", info->sif_calb_version);
    printf("Detector Dimensions: %d x %d\n", info->detector_width, info->detector_height);
    printf("Image Size: %d x %d\n", info->image_width, info->image_height);
    printf("Number of Frames: %d\n", info->number_of_frames);
    printf("Number of Subimages: %d\n", info->number_of_subimages);
    printf("Exposure Time: %.6f s\n", info->exposure_time);
    printf("Cycle Time: %.6f s\n", info->cycle_time);
    printf("Detector Temperature: %.2f °C\n", info->detector_temperature);
    printf("Data Offset: 0x%08lX\n", info->data_offset);
    
    if (info->calibration_data_count > 0) {
        printf("Calibration Coefficients: ");
        for (int i = 0; i < info->calibration_data_count; i++) {
            printf("%.6f ", info->calibration_data[i]);
        }
        printf("\n");
    }
    
    printf("Frame Axis: %s\n", info->frame_axis);
    printf("Data Type: %s\n", info->data_type);
    printf("Image Axis: %s\n", info->image_axis);
    
    // 顯示時間戳記
    if (info->timestamps && info->number_of_frames > 0) {
        printf("First 5 timestamps: ");
        for (int i = 0; i < 5 && i < info->number_of_frames; i++) {
            printf("%" PRId64 " ", info->timestamps[i]);  // 使用跨平台的格式說明符
        }
        printf("\n");
    }
}

// 打印 SIF 文件結構資訊
void print_sif_file_structure(const SifFile *sif_file) {
    if (!sif_file) {
        printf("Error: No SIF file provided\n");
        return;
    }
    
    printf("SIF File Structure:\n");
    printf("===================\n");
    printf("Total Frames: %d\n", sif_file->frame_count);
    printf("Image Dimensions: %d x %d\n", sif_file->image_width, sif_file->image_height);
    printf("Tile Count: %d\n", sif_file->tile_count);
    
    if (sif_file->tiles && sif_file->tile_count > 0) {
        printf("\nTile Information:\n");
        for (int i = 0; i < sif_file->tile_count && i < 5; i++) {
            printf("  Tile %d: offset=0x%08lX, size=%dx%d\n", 
                   i, sif_file->tiles[i].offset,
                   sif_file->tiles[i].width, sif_file->tiles[i].height);
        }
        if (sif_file->tile_count > 5) {
            printf("  ... and %d more tiles\n", sif_file->tile_count - 5);
        }
    }
    
    // 子圖像資訊
    if (sif_file->info.subimages && sif_file->info.number_of_subimages > 0) {
        printf("\nSubimage Information:\n");
        for (int i = 0; i < sif_file->info.number_of_subimages; i++) {
            const SubImageInfo *sub = &sif_file->info.subimages[i];
            printf("  Subimage %d: area=(%d,%d)-(%d,%d), binning=%dx%d, size=%dx%d\n",
                   i, sub->x0, sub->y0, sub->x1, sub->y1,
                   sub->xbin, sub->ybin, sub->width, sub->height);
        }
    }
}

// 打印十六進制轉儲
void print_hex_dump(FILE *fp, int offset, int length) {
    if (!fp) return;
    
    fseek(fp, offset, SEEK_SET);
    
    unsigned char buffer[16];
    int bytes_read;
    int total_bytes = 0;
    
    printf("Hex Dump (offset 0x%08X, length %d):\n", offset, length);
    printf("Offset    Hex Content                     ASCII\n");
    printf("--------  ------------------------------  ----------------\n");
    
    while (total_bytes < length && 
           (bytes_read = fread(buffer, 1, 16, fp)) > 0) {
        
        // 顯示偏移量
        printf("%08X  ", offset + total_bytes);
        
        // 顯示十六進制
        for (int i = 0; i < 16; i++) {
            if (i < bytes_read) {
                printf("%02X ", buffer[i]);
            } else {
                printf("   ");
            }
            
            if (i == 7) printf(" ");
        }
        
        printf(" ");
        
        // 顯示 ASCII
        for (int i = 0; i < bytes_read; i++) {
            if (isprint(buffer[i])) {
                printf("%c", buffer[i]);
            } else {
                printf(".");
            }
        }
        
        printf("\n");
        
        total_bytes += bytes_read;
        if (total_bytes >= length) break;
    }
}