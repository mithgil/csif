#include "sif_utils.h"
#include <ctype.h>
#include <inttypes.h>  // 添加這個頭文件

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

void debug_print_some_lines(FILE* fp, long debug_pos, int num_lines){

    printf("→ Debug: Checking actual data format at 0x%lX\n", debug_pos);

    for (int i = 0; i < num_lines; i++) {  // 多讀幾行
        char debug_line[256];
        if (fgets(debug_line, sizeof(debug_line), fp) == NULL) break;
        trim_trailing_whitespace(debug_line);
        printf("  Line %d: '%s' (length: %lu)\n", i, debug_line, strlen(debug_line));
    }

    // 回到原來位置
    fseek(fp, debug_pos, SEEK_SET);
    printf("  Reset to position: 0x%lX\n", ftell(fp));
}

void debug_hex_dump(FILE* fp, long debug_pos, int num_bytes_to_dump) {
    // 保存當前位置
    long original_pos = ftell(fp);
    
    // 移動到指定位置
    fseek(fp, debug_pos, SEEK_SET);
    
    printf("→ Debug Hex Dump starting from position: 0x%lX\n", debug_pos);
    printf("Bytes to dump: %d\n\n", num_bytes_to_dump);
    
    // 讀取數據
    unsigned char *buffer = (unsigned char*)malloc(num_bytes_to_dump);
    if (!buffer) {
        printf("Error: Memory allocation failed for %d bytes\n", num_bytes_to_dump);
        fseek(fp, original_pos, SEEK_SET);
        return;
    }
    
    long bytes_read = fread(buffer, 1, num_bytes_to_dump, fp);
    
    printf("Bytes actually read: %ld\n\n", bytes_read);
    printf("Offset  Hex                                               ASCII\n");
    printf("------  ------------------------------------------------  ----------------\n");
    
    // 計算要顯示的行數
    long lines_to_display = (bytes_read + 15) / 16;
    
    for (long i = 0; i < lines_to_display; i++) {
        long offset = i * 16;
        long absolute_offset = debug_pos + offset;
        
        printf("%06lX  ", absolute_offset);
        
        // 顯示十六進制
        for (int j = 0; j < 16; j++) {
            if (offset + j < bytes_read) {
                printf("%02X ", buffer[offset + j]);
            } else {
                printf("   ");
            }
        }
        
        printf(" ");
        
        // 顯示 ASCII
        for (int j = 0; j < 16 && offset + j < bytes_read; j++) {
            unsigned char c = buffer[offset + j];
            if (isprint(c)) {
                printf("%c", c);
            } else {
                printf(".");
            }
        }
        
        printf("\n");
        
        // 標記特殊位置
        if (offset == 0) {
            printf("       ^-- Start of dump (position 0x%lX)\n", debug_pos);
        }
        
        // 標記浮點數數據模式
        if (offset + 4 <= bytes_read) {
            // 檢查是否可能是浮點數數據 (常見的 0x44 模式)
            if (buffer[offset + 2] == 0x1C && buffer[offset + 3] == 0x44) {
                printf("       ^-- Possible float data pattern: 1C 44\n");
            }
        }
    }
    
    // 顯示統計資訊
    printf("\n=== Debug Hex Dump Summary ===\n");
    printf("Start position: 0x%lX\n", debug_pos);
    printf("Bytes requested: %d\n", num_bytes_to_dump);
    printf("Bytes displayed: %ld\n", bytes_read);
    printf("End position: 0x%lX\n", debug_pos + bytes_read);
    
    // 清理並恢復位置
    free(buffer);
    fseek(fp, original_pos, SEEK_SET);
    printf("Reset to original position: 0x%lX\n", original_pos);
}

// 結合兩者的多功能調試函數
void debug_comprehensive(FILE* fp, long debug_pos, int num_lines, int hex_dump_bytes) {
    printf("=== Comprehensive Debug Analysis ===\n");
    printf("Starting from position: 0x%lX\n\n", debug_pos);
    
    // 1. 先顯示文字行
    debug_print_some_lines(fp, debug_pos, num_lines);
    
    printf("\n");
    
    // 2. 再顯示十六進制 dump
    debug_hex_dump(fp, debug_pos, hex_dump_bytes);
}

// 輔助函數：從文件流中讀取一個 4-byte (32-bit) 小端序整數
// 返回值：成功讀取的整數值；失敗則返回 -1
int32_t read_little_endian_int32(FILE *fp) {
    uint8_t bytes[4];
    size_t count = fread(bytes, 1, 4, fp);

    if (count != 4) {
        // 如果讀取不足 4 個字節，則返回錯誤
        return -1; 
    }

    // 構建 32 位整數 (小端序: byte[0] 是最低位)
    int32_t value = (int32_t)(
        (bytes[0] << 0) |
        (bytes[1] << 8) |
        (bytes[2] << 16) |
        (bytes[3] << 24)
    );

    return value;
}

// 讀取大端序 32 位整數
int32_t read_big_endian_int32(FILE *fp) {
    uint8_t bytes[4];
    size_t count = fread(bytes, 1, 4, fp);
    
    if (count != 4) {
        return -1;
    }
    
    // 大端序: byte[0] 是最高位
    int32_t value = (int32_t)(
        (bytes[0] << 24) |
        (bytes[1] << 16) | 
        (bytes[2] << 8) |
        (bytes[3] << 0)
    );
    
    return value;
}

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
    printf("Experiment Time: %d\n", info->experiment_time);
    printf("Detector Temperature: %.2f °C\n", info->detector_temperature);
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

    if (info->detector_temperature <= -998.0f) {
        printf(" [SENSOR OFFLINE]");
    }
    printf("Data Offset: 0x%08lX\n", info->data_offset);
    
    if (info->calibration_coeff_count > 0) {
        printf("Calibration Coefficients: ");
        for (int i = 0; i < info->calibration_coeff_count; i++) {
            printf("%.6f ", info->calibration_coefficients[i]);
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
    printf("Image Dimensions: %d x %d\n", sif_file->info.image_width, sif_file->info.image_height);
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

// 打印十六進制轉儲（支持從指定位置之前開始）
void print_hex_dump(FILE *fp, int target_offset, int before_bytes, int after_bytes) {
    if (!fp) return;
    
    // 計算實際開始位置
    int start_offset = target_offset - before_bytes;
    if (start_offset < 0) start_offset = 0;
    
    int total_length = before_bytes + after_bytes;
    
    fseek(fp, start_offset, SEEK_SET);
    
    unsigned char buffer[16];
    int bytes_read;
    int total_bytes = 0;
    
    printf("Hex Dump (offset 0x%08X, showing %d bytes before and %d bytes after):\n", 
           target_offset, before_bytes, after_bytes);
    printf("Offset    Hex Content                     ASCII\n");
    printf("--------  ------------------------------  ----------------\n");
    
    while (total_bytes < total_length && 
           (bytes_read = fread(buffer, 1, 16, fp)) > 0) {
        
        int current_offset = start_offset + total_bytes;
        
        // 顯示偏移量
        printf("%08X  ", current_offset);
        
        // 標記目標位置
        if (current_offset <= target_offset && (current_offset + bytes_read) > target_offset) {
            printf(">");
        } else {
            printf(" ");
        }
        
        // 顯示十六進制
        for (int i = 0; i < 16; i++) {
            if (i < bytes_read) {
                // 標記目標位置的字節
                if (current_offset + i == target_offset) {
                    printf("[%02X]", buffer[i]);
                } else {
                    printf("%02X ", buffer[i]);
                }
            } else {
                printf("   ");
            }
            
            if (i == 7) printf(" ");
        }
        
        printf(" ");
        
        // 顯示 ASCII
        for (int i = 0; i < bytes_read; i++) {
            if (current_offset + i == target_offset) {
                printf("["); // 開始標記
            }
            
            if (isprint(buffer[i])) {
                printf("%c", buffer[i]);
            } else {
                printf(".");
            }
            
            if (current_offset + i == target_offset) {
                printf("]"); // 結束標記
            }
        }
        
        printf("\n");
        
        total_bytes += bytes_read;
        if (total_bytes >= total_length) break;
    }
}



// 多項式計算函數
double evaluate_polynomial(const double* coefficients, int coeff_count, double x) {
    double result = 0.0;
    for (int i = 0; i < coeff_count; i++) {
        result += coefficients[i] * pow(x, i);
    }
    return result;
}

// 反轉係數陣列（對應 Julia 的 reverse）
void reverse_coefficients(double* coefficients, int coeff_count) {
    for (int i = 0; i < coeff_count / 2; i++) {
        double temp = coefficients[i];
        coefficients[i] = coefficients[coeff_count - 1 - i];
        coefficients[coeff_count - 1 - i] = temp;
    }
}

// 主函數：檢索校準數據
double* retrieve_calibration(SifInfo *info, int* calibration_size) {
    if (!info) {
        *calibration_size = 0;
        return NULL;
    }
    
    int width = info->detector_width;
    if (info->image_length > 0) {
        width = info->image_length;
    }
    
    printf("→ Retrieving calibration data (width: %d)\n", width);
    
    // 情況1: 有多個 frame 校準數據
    if (info->has_frame_calibrations && info->number_of_frames > 0) {
        printf("  Found frame-specific calibrations for %d frames\n", info->number_of_frames);
        
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
                
                // 反轉係數（對應 Julia 的 reverse_coef = reverse(meta[key])）
                reverse_coefficients(coefficients, frame_calib->coeff_count);
                
                // 再次反轉（對應 Julia 的 Polynomial(reverse(reverse_coef))）
                // 實際上等於恢復原狀，但保留邏輯對應
                reverse_coefficients(coefficients, frame_calib->coeff_count);
                
                printf("    Frame %d: %d coefficients -> ", frame + 1, frame_calib->coeff_count);
                for (int i = 0; i < frame_calib->coeff_count; i++) {
                    printf("%f ", coefficients[i]);
                }
                printf("\n");
                
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
        printf("  Found global calibration data: %d coefficients\n", info->calibration_coeff_count);
        
        // 分配 1D 陣列：width
        double* calibration = malloc(width * sizeof(double));
        if (!calibration) {
            *calibration_size = 0;
            return NULL;
        }
        
        // 複製係數以便反轉
        double coefficients[MAX_COEFFICIENTS];
        memcpy(coefficients, info->calibration_coefficients, info->calibration_coeff_count * sizeof(double));
        
        // 反轉係數（對應 Julia 的 reverse_coef = reverse(meta["Calibration_data"])）
        reverse_coefficients(coefficients, info->calibration_coeff_count);
        
        // 再次反轉（對應 Julia 的 Polynomial(reverse(reverse_coef))）
        reverse_coefficients(coefficients, info->calibration_coeff_count);
        
        printf("    Coefficients: ");
        for (int i = 0; i < info->calibration_coeff_count; i++) {
            printf("%f ", coefficients[i]);
        }
        printf("\n");
        
        // 計算多項式值（對應 Julia 的 p.(1:width)）
        for (int x = 0; x < width; x++) {
            calibration[x] = evaluate_polynomial(coefficients, info->calibration_coeff_count, x + 1);
        }
        
        *calibration_size = width;
        return calibration;
    }
    // 情況3: 沒有校準數據
    else {
        printf("  No calibration data found\n");
        *calibration_size = 0;
        return NULL;
    }
}
