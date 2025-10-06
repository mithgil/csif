#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sif_parser.h"
#include "sif_utils.h"
#include <inttypes.h>

void debug_parse_step_by_step(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        printf("Error: Cannot open file %s\n", filename);
        return;
    }
    
    printf("=== Step-by-Step SIF Debugging ===\n");
    printf("File: %s\n\n", filename);
    
    // 手動逐步解析來找到問題所在
    char line[512];
    long offset;
    
    // Line 1
    offset = ftell(fp);
    fgets(line, sizeof(line), fp);
    printf("Line 1 (offset %ld): %s", offset, line);
    
    // Line 2  
    offset = ftell(fp);
    fgets(line, sizeof(line), fp);
    printf("Line 2 (offset %ld): %s", offset, line);
    
    // Line 3 - 這行可能很長，需要特殊處理
    offset = ftell(fp);
    printf("Line 3 (offset %ld): ", offset);
    
    // 逐個讀取 Line 3 的數字
    int value;
    char c;
    int count = 0;
    while (count < 20) { // 限制讀取數量
        if (fscanf(fp, "%d", &value) == 1) {
            printf("%d ", value);
            count++;
            
            // 檢查下一個字符
            c = fgetc(fp);
            if (c == '\n') {
                printf("[END]");
                break;
            } else if (c == EOF) {
                printf("[EOF]");
                break;
            } else {
                ungetc(c, fp);
            }
        } else {
            break;
        }
    }
    printf("\n");
    
    // 檢查當前位置
    offset = ftell(fp);
    printf("After Line 3, position: %ld\n", offset);
    
    // Line 4
    if (fgets(line, sizeof(line), fp)) {
        printf("Line 4 (offset %ld): %s", offset, line);
    }
    
    // Line 5 - 特別關注這行
    offset = ftell(fp);
    if (fgets(line, sizeof(line), fp)) {
        printf("Line 5 (offset %ld): %s", offset, line);
        
        // 分析 Line 5 的內容
        printf("Line 5 analysis: ");
        char *token = strtok(line, " \t\r\n");
        int token_count = 0;
        while (token) {
            printf("Token%d='%s' ", token_count, token);
            token_count++;
            token = strtok(NULL, " \t\r\n");
        }
        printf("(Total tokens: %d)\n", token_count);
    }
    
    fclose(fp);
}

#include <stdbool.h>

typedef enum {
    VALIDATION_PASS = 0,
    VALIDATION_WARNING = 1,
    VALIDATION_ERROR = 2
} ValidationResult;

ValidationResult validate_sif_structure(const SifFile *sif_file) {
    if (!sif_file) {
        printf("❌ Error: SifFile is NULL\n");
        return VALIDATION_ERROR;
    }
    
    const SifInfo *info = &sif_file->info;
    int error_count = 0;
    int warning_count = 0;
    
    printf("=== SIF Structure Validation ===\n");
    
    // 1. 基本文件信息驗證
    printf("1. Basic File Info:\n");
    
    if (info->sif_version <= 0 || info->sif_version > 70000) {
        printf("   ❌ Invalid SIF version: %d\n", info->sif_version);
        error_count++;
    } else {
        printf("   ✓ SIF version: %d\n", info->sif_version);
    }
    
    if (strlen(info->detector_type) == 0) {
        printf("   ⚠️  Detector type is empty\n");
        warning_count++;
    } else {
        printf("   ✓ Detector type: %s\n", info->detector_type);
    }
    
    // 2. 探測器尺寸驗證
    printf("2. Detector Dimensions:\n");
    
    if (info->detector_width <= 0 || info->detector_width > 10000) {
        printf("   ❌ Invalid detector width: %d\n", info->detector_width);
        error_count++;
    } else {
        printf("   ✓ Detector width: %d\n", info->detector_width);
    }
    
    if (info->detector_height <= 0 || info->detector_height > 10000) {
        printf("   ❌ Invalid detector height: %d\n", info->detector_height);
        error_count++;
    } else {
        printf("   ✓ Detector height: %d\n", info->detector_height);
    }
    
    // 3. 圖像尺寸驗證
    printf("3. Image Dimensions:\n");
    
    if (info->image_width <= 0) {
        printf("   ❌ Invalid image width: %d\n", info->image_width);
        error_count++;
    } else if (info->image_width > info->detector_width) {
        printf("   ❌ Image width %d exceeds detector width %d\n", 
               info->image_width, info->detector_width);
        error_count++;
    } else {
        printf("   ✓ Image width: %d\n", info->image_width);
    }
    
    if (info->image_height <= 0) {
        printf("   ❌ Invalid image height: %d\n", info->image_height);
        error_count++;
    } else if (info->image_height > info->detector_height) {
        printf("   ❌ Image height %d exceeds detector height %d\n", 
               info->image_height, info->detector_height);
        error_count++;
    } else {
        printf("   ✓ Image height: %d\n", info->image_height);
    }
    
    // 4. 幀數和子圖像驗證
    printf("4. Frame and Subimage Info:\n");
    
    if (info->number_of_frames < 0) {
        printf("   ❌ Invalid number of frames: %d\n", info->number_of_frames);
        error_count++;
    } else if (info->number_of_frames == 0) {
        printf("   ⚠️  No frames in file\n");
        warning_count++;
    } else {
        printf("   ✓ Number of frames: %d\n", info->number_of_frames);
    }
    
    if (info->number_of_subimages < 0) {
        printf("   ❌ Invalid number of subimages: %d\n", info->number_of_subimages);
        error_count++;
    } else if (info->number_of_subimages == 0) {
        printf("   ⚠️  No subimages defined\n");
        warning_count++;
    } else {
        printf("   ✓ Number of subimages: %d\n", info->number_of_subimages);
    }
    
    // 5. 時間參數驗證
    printf("5. Timing Parameters:\n");
    
    if (info->exposure_time < 0) {
        printf("   ❌ Invalid exposure time: %.6f\n", info->exposure_time);
        error_count++;
    } else if (info->exposure_time > 3600) {
        printf("   ⚠️  Unusually long exposure time: %.6f seconds\n", info->exposure_time);
        warning_count++;
    } else {
        printf("   ✓ Exposure time: %.6f s\n", info->exposure_time);
    }
    
    if (info->cycle_time < info->exposure_time) {
        printf("   ❌ Cycle time (%.6f) less than exposure time (%.6f)\n", 
               info->cycle_time, info->exposure_time);
        error_count++;
    } else {
        printf("   ✓ Cycle time: %.6f s\n", info->cycle_time);
    }
    
    // 6. 溫度驗證
    printf("6. Detector Temperature:\n");
    
    if (info->detector_temperature < -273.15) {
        printf("   ❌ Impossible temperature: %.2f°C\n", info->detector_temperature);
        error_count++;
    } else if (info->detector_temperature < -100) {
        printf("   ⚠️  Very low temperature: %.2f°C\n", info->detector_temperature);
        warning_count++;
    } else if (info->detector_temperature > 100) {
        printf("   ⚠️  Very high temperature: %.2f°C\n", info->detector_temperature);
        warning_count++;
    } else {
        printf("   ✓ Detector temperature: %.2f°C\n", info->detector_temperature);
    }
    
    // 7. 數據偏移驗證
    printf("7. Data Offset:\n");
    
    if (info->data_offset <= 0) {
        printf("   ❌ Invalid data offset: 0x%08lX\n", info->data_offset);
        error_count++;
    } else {
        printf("   ✓ Data offset: 0x%08lX\n", info->data_offset);
        
        // 檢查數據偏移是否合理（通常應該在文件頭之後）
        if (info->data_offset < 100) {
            printf("   ⚠️  Data offset seems too small\n");
            warning_count++;
        }
    }
    
    // 8. 子圖像數據驗證
    if (info->number_of_subimages > 0 && info->subimages) {
        printf("8. Subimage Details:\n");
        
        for (int i = 0; i < info->number_of_subimages; i++) {
            const SubImageInfo *sub = &info->subimages[i];
            
            printf("   Subimage %d:\n", i);
            
            // 檢查坐標範圍
            if (sub->x0 < 0 || sub->x1 >= info->detector_width || 
                sub->y0 < 0 || sub->y1 >= info->detector_height) {
                printf("      ❌ Coordinates out of bounds: (%d,%d)-(%d,%d)\n",
                       sub->x0, sub->y0, sub->x1, sub->y1);
                error_count++;
            } else {
                printf("      ✓ Coordinates: (%d,%d)-(%d,%d)\n",
                       sub->x0, sub->y0, sub->x1, sub->y1);
            }
            
            // 檢查 binning
            if (sub->xbin <= 0 || sub->ybin <= 0) {
                printf("      ❌ Invalid binning: %dx%d\n", sub->xbin, sub->ybin);
                error_count++;
            } else {
                printf("      ✓ Binning: %dx%d\n", sub->xbin, sub->ybin);
            }
            
            // 檢查計算後的尺寸
            int calc_width = (1 + sub->x1 - sub->x0) / sub->xbin;
            int calc_height = (1 + sub->y1 - sub->y0) / sub->ybin;
            
            if (calc_width != sub->width || calc_height != sub->height) {
                printf("      ❌ Size mismatch: stored=%dx%d, calculated=%dx%d\n",
                       sub->width, sub->height, calc_width, calc_height);
                error_count++;
            } else {
                printf("      ✓ Size: %dx%d\n", sub->width, sub->height);
            }
        }
    }
    
    // 9. 時間戳驗證
    if (info->number_of_frames > 0 && info->timestamps) {
        printf("9. Timestamps:\n");
        
        int64_t first_timestamp = info->timestamps[0];
        printf("   ✓ First timestamp: %" PRId64 "\n", first_timestamp);
        
        // 檢查時間戳是否遞增
        for (int i = 1; i < info->number_of_frames; i++) {
            if (info->timestamps[i] < info->timestamps[i-1]) {
                printf("   ❌ Timestamps not monotonically increasing at frame %d\n", i);
                error_count++;
                break;
            }
        }
    }
    
    // 10. 瓦片數據驗證
    if (sif_file->tile_count > 0 && sif_file->tiles) {
        printf("10. Tile Information:\n");
        
        if (sif_file->tile_count != info->number_of_frames) {
            printf("   ❌ Tile count (%d) doesn't match frame count (%d)\n",
                   sif_file->tile_count, info->number_of_frames);
            error_count++;
        } else {
            printf("   ✓ Tile count matches frame count: %d\n", sif_file->tile_count);
        }
        
        for (int i = 0; i < sif_file->tile_count; i++) {
            const ImageTile *tile = &sif_file->tiles[i];
            
            if (tile->width != info->image_width || tile->height != info->image_height) {
                printf("   ❌ Tile %d size mismatch: %dx%d vs expected %dx%d\n",
                       i, tile->width, tile->height, info->image_width, info->image_height);
                error_count++;
            }
        }
    }
    
    // 總結
    printf("\n=== Validation Summary ===\n");
    printf("Errors: %d, Warnings: %d\n", error_count, warning_count);
    
    if (error_count == 0 && warning_count == 0) {
        printf("✅ All checks passed! SIF structure appears valid.\n");
        return VALIDATION_PASS;
    } else if (error_count == 0) {
        printf("⚠️  Some warnings found, but no critical errors.\n");
        return VALIDATION_WARNING;
    } else {
        printf("❌ Critical errors found! File may be corrupted or parsing incorrect.\n");
        return VALIDATION_ERROR;
    }
}

// ./bin/debug_sif '/home/tim/Documents/AS/data/andor/20250917/1OD_500uW_sapphire_200umFiber_.sif'
// ./bin/debug_sif '/home/tim/Documents/AS/data/andor/20250908/monochrom_430_700_10_LED_2.sif'

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <sif_file>\n", argv[0]);
        return 1;
    }
    
    const char *filename = argv[1];
    
    // 1. 首先用調試工具分析文件結構
    //debug_parse_step_by_step(filename);
    //printf("\n");
    
    // 2. 然後嘗試正常解析
    printf("===== SIF File Analysis =====\n");
    FILE *fp = fopen(filename, "rb");
    if (fp) {
        SifFile sif_file;
        
        // 1. 解析文件
        if (sif_open(fp, &sif_file) == 0) {
            printf("✓ SIF file parsed successfully!\n\n");
            
            // 2. 顯示基本信息
            printf("=== File Information ===\n");
            print_sif_info_summary(&sif_file.info);
            printf("\n");
            
            // 3. 顯示結構信息
            printf("=== File Structure ===\n");
            print_sif_file_structure(&sif_file);
            printf("\n");
            
            // 4. 驗證結構
            printf("=== Structure Validation ===\n");
            ValidationResult result = validate_sif_structure(&sif_file);
            
            switch (result) {
                case VALIDATION_PASS:
                    printf("✅ File is ready for data processing.\n");
                    break;
                case VALIDATION_WARNING:
                    printf("⚠️  File has some issues but may be usable.\n");
                    break;
                case VALIDATION_ERROR:
                    printf("❌ File has critical issues, processing not recommended.\n");
                    break;
            }
            printf("\n");
            
            // 5. 顯示數據區域（可選）
            printf("=== Data Region Preview ===\n");
            print_hex_dump(fp, sif_file.info.data_offset, 128);
            
            sif_close(&sif_file);
        } else {
            printf("✗ Failed to parse SIF file\n");
        }
        fclose(fp);
    } else {
        printf("Error: Cannot open file %s\n", filename);
    }
        
    return 0;
}