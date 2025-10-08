#include "sif_utils.h"
#include "sif_parser.h"
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



void debug_hex_dump(FILE* fp, long debug_pos, int num_bytes_to_dump) {

    if (current_verbose_level < SIF_DEBUG) {
        return;  // 如果不是 DEBUG 級別，直接返回
    }

    // 保存當前位置
    long original_pos = ftell(fp);
    
    // 移動到指定位置
    fseek(fp, debug_pos, SEEK_SET);
    
    PRINT_DEBUG("→ Debug Hex Dump starting from position: 0x%lX\n", debug_pos);
    PRINT_DEBUG("Bytes to dump: %d\n\n", num_bytes_to_dump);
    
    // 讀取數據
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
        
        // 顯示十六進制
        for (int j = 0; j < 16; j++) {
            if (offset + j < bytes_read) {
                PRINT_DEBUG("%02X ", buffer[offset + j]);
            } else {
                PRINT_DEBUG("   ");
            }
        }
        
        PRINT_DEBUG(" ");
        
        // 顯示 ASCII
        for (int j = 0; j < 16 && offset + j < bytes_read; j++) {
            unsigned char c = buffer[offset + j];
            if (isprint(c)) {
                PRINT_DEBUG("%c", c);
            } else {
                PRINT_DEBUG(".");
            }
        }
        
        PRINT_DEBUG("\n");
        
        // 標記特殊位置
        if (offset == 0) {
            PRINT_DEBUG("       ^-- Start of dump (position 0x%lX)\n", debug_pos);
        }
        
        // 標記浮點數數據模式
        if (offset + 4 <= bytes_read) {
            // 檢查是否可能是浮點數數據 (常見的 0x44 模式)
            if (buffer[offset + 2] == 0x1C && buffer[offset + 3] == 0x44) {
                PRINT_DEBUG("       ^-- Possible float data pattern: 1C 44\n");
            }
        }
    }
    
    // 顯示統計資訊
    PRINT_DEBUG("\n=== Debug Hex Dump Summary ===\n");
    PRINT_DEBUG("Start position: 0x%lX\n", debug_pos);
    PRINT_DEBUG("Bytes requested: %d\n", num_bytes_to_dump);
    PRINT_DEBUG("Bytes displayed: %ld\n", bytes_read);
    PRINT_DEBUG("End position: 0x%lX\n", debug_pos + bytes_read);
    
    // 清理並恢復位置
    free(buffer);
    fseek(fp, original_pos, SEEK_SET);
    PRINT_DEBUG("Reset to original position: 0x%lX\n", original_pos);
}

// 結合兩者的多功能調試函數
void debug_comprehensive(FILE* fp, long debug_pos, int num_lines, int hex_dump_bytes) {
    if (current_verbose_level < SIF_DEBUG) {
        return;  // 如果不是 DEBUG 級別，直接返回
    }

    PRINT_DEBUG("=== Comprehensive Debug Analysis ===\n");
    PRINT_DEBUG("Starting from position: 0x%lX\n\n", debug_pos);
    
    // 1. 先顯示文字行
    debug_print_some_lines(fp, debug_pos, num_lines);
    
    PRINT_DEBUG("\n");
    
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
void print_sif_first_line(const char *filename, SifInfo *info) {
    if (current_verbose_level < SIF_DEBUG) {
        return;  // 如果不是 DEBUG 級別，直接返回
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

// 工具函數內部的輸出也使用 sif_print
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
                    i, sif_file->info.subimages[i].x0,     
                        sif_file->info.subimages[i].y0,     
                        sif_file->info.subimages[i].x1,     
                        sif_file->info.subimages[i].y1,     
                        sif_file->info.subimages[i].xbin,   
                        sif_file->info.subimages[i].ybin,   
                        sif_file->info.subimages[i].width, 
                        sif_file->info.subimages[i].height);
    }
}

// 除錯函數使用 DEBUG 級別
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


// 打印十六進制轉儲（支持從指定位置之前開始）
void print_hex_dump(FILE *fp, int target_offset, int before_bytes, int after_bytes) {

    // 這個函數本身輸出很多除錯資訊，應該只在 DEBUG 級別顯示
    if (current_verbose_level < SIF_DEBUG) {
        return;  
    }
    

    if (!fp) return;
    
    // 計算實際開始位置
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
        
        // 顯示偏移量
        PRINT_DEBUG("%08X  ", current_offset);
        
        // 標記目標位置
        if (current_offset <= target_offset && (current_offset + bytes_read) > target_offset) {
            PRINT_DEBUG(">");
        } else {
            PRINT_DEBUG(" ");
        }
        
        // 顯示十六進制
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
        
        // 顯示 ASCII
        for (int i = 0; i < bytes_read; i++) {
            if (current_offset + i == target_offset) {
                PRINT_DEBUG("["); // 開始標記
            }
            
            if (isprint(buffer[i])) {
                PRINT_DEBUG("%c", buffer[i]);
            } else {
                PRINT_DEBUG(".");
            }
            
            if (current_offset + i == target_offset) {
                PRINT_DEBUG("]"); // 結束標記
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
                
                // 反轉係數（對應 Julia 的 reverse_coef = reverse(meta[key])）
                reverse_coefficients(coefficients, frame_calib->coeff_count);
                
                // 再次反轉（對應 Julia 的 Polynomial(reverse(reverse_coef))）
                // 實際上等於恢復原狀，但保留邏輯對應
                reverse_coefficients(coefficients, frame_calib->coeff_count);
                
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
        
        // 反轉係數（對應 Julia 的 reverse_coef = reverse(meta["Calibration_data"])）
        reverse_coefficients(coefficients, info->calibration_coeff_count);
        
        // 再次反轉（對應 Julia 的 Polynomial(reverse(reverse_coef))）
        reverse_coefficients(coefficients, info->calibration_coeff_count);
        
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
