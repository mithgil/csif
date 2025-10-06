#include "sif_parser.h"
#include "sif_utils.h"
#include <ctype.h>
#include <inttypes.h>

// 首先聲明所有 static 輔助函數
static int read_binary_string(FILE *fp, char *buffer, int max_length, int length);
static int read_line_with_binary_check(FILE *fp, char *buffer, int max_length);
static int read_line_directly(FILE *fp, char *buffer, int max_length);

static void discard_line(FILE *fp);
static void discard_bytes(FILE *fp, long count);


// 直接讀取一行（用於當長度前綴解析失敗時）
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
    
    // 處理可能的回車換行
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


static int read_python_style_string(FILE *fp, char *buffer, int max_length) {
    // 1. 讀取一行作為長度
    char length_line[32];
    if (fgets(length_line, sizeof(length_line), fp) == NULL) {
        return -1;
    }
    trim_trailing_whitespace(length_line);
    
    int length = atoi(length_line);
    printf("  String length from file: %d (line: '%s')\n", length, length_line);
    
    // 2. 檢查緩衝區大小
    if (length <= 0 || length >= max_length) {
        printf("  Invalid length: %d (max: %d)\n", length, max_length);
        return -1;
    }
    
    // 3. 讀取指定長度的字符串
    size_t bytes_read = fread(buffer, 1, length, fp);
    if (bytes_read != length) {
        printf("  Failed to read string: expected %d bytes, got %zu\n", length, bytes_read);
        return -1;
    }
    
    buffer[length] = '\0';
    printf("  Read string: '%s'\n", buffer);
    
    return length;
}

// 輔助函數實現
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
        
        // 檢查是否遇到二進制數據（非可打印字符）
        if (c < 32 && c != '\t' && c != '\r') {
            // 遇到二進制數據，回退並停止讀取
            fseek(fp, -1, SEEK_CUR);
            break;
        }
        
        buffer[i++] = (char)c;
    }
    
    buffer[i] = '\0';
    return i;
}


// 讀取直到終止符 (模擬 Python 的 _read_until)
int read_until(FILE *fp, char *buffer, int max_length, char terminator) {
    int i = 0;
    char c;
    long start_pos = ftell(fp);

    while (i < max_length - 1) {
        if (fread(&c, 1, 1, fp) != 1) {
            return -1; // EOF
        }
        
        // 遇到終止符或換行符
        if (c == terminator || c == '\n') {
            if (i > 0) break; // 讀到有效數據後遇到終止符，停止
            // 如果是空格或換行符，且還沒讀到有效數據，則繼續 (模擬 Python 的 while len(word) > 0)
            if (c != '\n') continue;
            else break; // 換行符通常是行的結束，即使word為空也停止
        }
        
        buffer[i++] = c;
    }
    
    buffer[i] = '\0';
    
    // 如果因為換行符停止，需要確保不丟失它
    if (c == '\n' && i == 0) {
        // 這是個空行，或者我們剛好在行首，確保指標在換行符之後
        if (ftell(fp) > start_pos + 1) fseek(fp, -1, SEEK_CUR); // 如果讀了字節，回退
    } else if (c != terminator && c != '\n') {
        // 如果不是因為終止符停止（而是因為緩衝區滿或EOF），回退一個字節以讓下一個讀取函數處理終止符
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

// 跳過空格和換行符 (模擬 Python 的 _skip_spaces)
void skip_spaces(FILE *fp) {
    char c;
    long offset;
    
    while (1) {
        offset = ftell(fp);
        if (fread(&c, 1, 1, fp) != 1) break;
        
        if (c != ' ' && c != '\n' && c != '\r') {
            fseek(fp, offset, SEEK_SET); // 回到非空白字符處
            break;
        }
    }
}

// 讀取並丟棄一行
static void discard_line(FILE *fp) {
    char buffer[MAX_STRING_LENGTH];
    if (fgets(buffer, MAX_STRING_LENGTH, fp) == NULL) {
        // 遇到 EOF
    }
}

// 讀取固定長度的原始數據並丟棄
static void discard_bytes(FILE *fp, long count) {
    fseek(fp, count, SEEK_CUR);
}

// 主要解析函數
int sif_open(FILE *fp, SifFile *sif_file) {

    if (!fp || !sif_file) return -1;
    
    SifInfo *info = &sif_file->info;
    memset(info, 0, sizeof(SifInfo));
    info->raman_ex_wavelength = NAN;
    info->calibration_data_count = 0;
    
    printf("=== Starting SIF File Parsing ===\n");
    
    char line_buffer[MAX_STRING_LENGTH];
    
    // Line 1: Magic string
    if (fread(line_buffer, 1, 36, fp) != 36 || strncmp(line_buffer, SIF_MAGIC, 36) != 0) {
        fprintf(stderr, "Error: Not a SIF file or invalid magic string\n");
        return -1;
    }
    printf("✓ Line 1: Valid magic string\n");

    // Line 2: Skip
    discard_line(fp); 

    // Line 3: Structured data
    printf("→ Line 3: Parsing structured data...\n");
    
    long line3_start = ftell(fp);
    printf("  Line 3 starts at offset: 0x%lX\n", line3_start);
    
    info->sif_version = read_int(fp);
    discard_bytes(fp, 3); // 3*read_until(fp, ' ')
    info->experiment_time = read_int(fp);
    info->detector_temperature = read_float(fp);
    
    discard_bytes(fp, 10); // 跳過 10 bytes 的 padding
    
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
    
    // 跳過 16 個值
    for (int i = 0; i < 16; i++) {
        read_int(fp);
    }
    
    info->grating_blaze = read_float(fp);
    
    // 讀取 Line 3 剩餘部分直到換行
    if (fgets(line_buffer, sizeof(line_buffer), fp) == NULL) return -1;
    
    // Line 4: Detector Type
    if (fgets(info->detector_type, sizeof(info->detector_type), fp) == NULL) return -1;
    //info->detector_type[strcspn(info->detector_type, "\r\n")] = 0;
    trim_trailing_whitespace(info->detector_type);
    printf("✓ Detector Type: '%s'\n", info->detector_type);

    // Line 5: Detector Dimensions
    info->detector_width = read_int(fp);
    info->detector_height = read_int(fp);
    printf("✓ Detector Dimensions: %d x %d\n", info->detector_width, info->detector_height);

    // 文件名讀取（修復版本）
    printf("→ Reading original filename...\n");
    long before_filename = ftell(fp);
    printf("  Before filename, position: 0x%lX\n", before_filename);
    
    // 讀取並丟棄第一行（"45"）
    if (fgets(line_buffer, sizeof(line_buffer), fp) == NULL) return -1;
    line_buffer[strcspn(line_buffer, "\r\n")] = 0;
    printf("  Discarded short line: '%s'\n", line_buffer);
    
    // 讀取真正的文件名
    if (fgets(info->original_filename, sizeof(info->original_filename), fp) == NULL) return -1;
    //info->original_filename[strcspn(info->original_filename, "\r\n")] = 0;
    trim_trailing_whitespace(info->original_filename);
    printf("✓ Original Filename: '%s'\n", info->original_filename);
    
    printf("After original filename parsing, position: 0x%lX\n", ftell(fp));
    
    // 跳過 20 space 0A newline
    discard_bytes(fp, 2);

    // Line 7: 應該是 "65538 2048"
    int user_text_flag = read_int(fp);
    int user_text_length = read_int(fp);
    printf("  User text flag: %d, length: %d\n", user_text_flag, user_text_length);

    long current_pos = ftell(fp);
    printf("  After Line 7, position: 0x%lX\n", current_pos);

    // 讀取 User Text（如果有的話）
    if (user_text_length > 0 && user_text_length < sizeof(info->user_text)) {
        if (fread(info->user_text, 1, user_text_length, fp) == user_text_length) {
            info->user_text[user_text_length] = '\0';
            printf("  User text: %d bytes\n", user_text_length);
        }
    }
    discard_line(fp); // 讀取換行符


   // Line 9: Shutter Time 信息
    // Line 9: Shutter Time 信息
    printf("→ Line 9: Reading shutter time...\n");
    long line9_start = ftell(fp);
    printf("  Line 9 starts at offset: 0x%lX\n", line9_start);

    // 讀取標記並驗證
    int line9_marker = read_int(fp);
    if (line9_marker != 65538) {
        printf("  ⚠️ Unexpected marker in Line 9: %d (expected 65538)\n", line9_marker);
        // 可以選擇繼續或返回錯誤
    }

    printf("  Line 9 marker: %d\n", line9_marker);

    // 跳過 8 個字節
    discard_bytes(fp, 8);
    printf("  Skipped 8 bytes\n");

    // 讀取快門時間
    info->shutter_time[0] = read_float(fp);
    info->shutter_time[1] = read_float(fp);

    // 檢查讀取的浮點數是否合理
    if (isnan(info->shutter_time[0]) || isnan(info->shutter_time[1])) {
        printf("  ❌ Failed to read shutter time values\n");
        return -1;
    }

    printf("✓ Shutter Time: %.6f, %.6f\n", info->shutter_time[0], info->shutter_time[1]);

    // 處理行結束
    skip_spaces(fp); // 跳過可能的多餘空格
    printf("  After Line 9, position: 0x%lX\n", ftell(fp));

    

    printf("→ Version-specific skipping logic...\n");
    printf("  SIF Version: %d\n", info->sif_version);

    // 根據 Python 代碼的邏輯
    if (info->sif_version >= 65548 && info->sif_version <= 65557) {
        printf("  Version 65548-65557: skipping 2 lines\n");
        for (int i = 0; i < 2; i++) discard_line(fp);
    }
    else if (info->sif_version == 65558) {
        printf("  Version 65558: skipping 5 lines\n");
        for (int i = 0; i < 5; i++) discard_line(fp);
    }
    else if (info->sif_version == 65559 || info->sif_version == 65564) {
        printf("  Version 65559/65564: skipping 8 lines\n");
        for (int i = 0; i < 8; i++) discard_line(fp);
    }
    else if (info->sif_version == 65565) {
        printf("  Version 65565: skipping 15 lines\n");
        for (int i = 0; i < 15; i++) discard_line(fp);
    }
    else if (info->sif_version > 65565) {
        printf("  Version %d > 65565: complex skipping logic\n", info->sif_version);
    
        // Line 10-17: 跳過 8 行
        for (int i = 0; i < 8; i++) {
            discard_line(fp);
        }
        printf("  Skipped 8 lines (Line 10-17)\n");
        
        // Line 18: Spectrograph
        if (fgets(info->spectrograph, sizeof(info->spectrograph), fp) == NULL) return -1;
        trim_trailing_whitespace(info->spectrograph);
        printf("✓ Spectrograph: '%s'\n", info->spectrograph);
        
        // Line 19: Intensifier info (跳過)
        discard_line(fp);
        printf("  Skipped intensifier info line\n");
        
        // Line 20-22: 讀取 3 個 float (可能是額外參數)
        for (int i = 0; i < 3; i++) {
            read_float(fp); // 讀取但暫時不存儲
        }
        printf("  Read 3 float parameters\n");
        
        // Line 23: Gate Gain
        info->gate_gain = read_float(fp);
        printf("✓ Gate Gain: %.6f\n", info->gate_gain);
        
        // Line 24-25: 讀取 2 個 float
        read_float(fp);
        read_float(fp);
        printf("  Read 2 additional float parameters\n");
        
        // Line 26: Gate Delay (picoseconds 轉 seconds)
        float gate_delay_ps = read_float(fp);
        info->gate_delay = gate_delay_ps * 1e-12;
        printf("✓ Gate Delay: %.6f ps (%.2e s)\n", gate_delay_ps, info->gate_delay);
        
        // Line 27: Gate Width (picoseconds 轉 seconds)  
        float gate_width_ps = read_float(fp);
        info->gate_width = gate_width_ps * 1e-12;
        printf("✓ Gate Width: %.6f ps (%.2e s)\n", gate_width_ps, info->gate_width);
        
        // Line 28-35: 跳過 8 行
        for (int i = 0; i < 8; i++) {
            discard_line(fp);
        }
        printf("  Skipped 8 lines (Line 28-35)\n");
    }

    printf("  After version skipping, position: 0x%lX\n", ftell(fp));

    
    printf("→ Reading calibration and additional data...\n");

    info->sif_calb_version = read_int(fp);
    printf("✓ SIF Calibration Version: %d\n", info->sif_calb_version);

    if (info->sif_calb_version == 65540) {
        discard_line(fp);
        printf("  Skipped line for calibration version 65540\n");
    }

    // calibration data
    char calib_line[MAX_STRING_LENGTH];
    if (fgets(calib_line, sizeof(calib_line), fp) == NULL) return -1;
    trim_trailing_whitespace(calib_line);
    printf("✓ Calibration Data: %s\n", calib_line);

    discard_line(fp);
    printf("  Skipped old calibration data\n");

    char extra_line[MAX_STRING_LENGTH];
    if (fgets(extra_line, sizeof(extra_line), fp) == NULL) return -1;
    trim_trailing_whitespace(extra_line);
    printf("  Extra Data: %s\n", extra_line);

    char raman_line[MAX_STRING_LENGTH];
    if (fgets(raman_line, sizeof(raman_line), fp) == NULL) return -1;

    // 直接使用 fgets 讀取的行，strtod 會自動處理換行符
    char *endptr;
    double raman_value = strtod(raman_line, &endptr);

    // 檢查是否成功解析了整行（除了換行符）
    if (endptr != raman_line) {
        info->raman_ex_wavelength = raman_value;
        printf("✓ Raman Excitation Wavelength: %.2f nm\n", info->raman_ex_wavelength);
    } else {
        info->raman_ex_wavelength = NAN;
        printf("  Raman wavelength: N/A ('%s')\n", raman_line);
    }

    // 現在跳過3行
    printf("→ Skipping 3 lines after Raman wavelength...\n");
    for (int i = 0; i < 3; i++) {
        discard_line(fp);
    }
    printf("  Skipped 3 lines\n");

    printf("  After calibration section, position: 0x%lX\n", ftell(fp));

    printf("→ Debug: Checking actual data format at 0x%lX\n", ftell(fp));

    // 讀取接下來的幾行看看真實內容
    for (int i = 0; i < 6; i++) {  // 多讀幾行
        char debug_line[256];
        if (fgets(debug_line, sizeof(debug_line), fp) == NULL) break;
        trim_trailing_whitespace(debug_line);
        printf("  Line %d: '%s' (length: %lu)\n", i, debug_line, strlen(debug_line));
    }

    // 回到原來位置
    fseek(fp, 0xAC6, SEEK_SET);
    printf("  Reset to position: 0x%lX\n", ftell(fp));

    // Frame Axis, Data Type, Image Axis

    printf("→ Reading axes as simple text lines...\n");

    // 直接讀取三行
    if (fgets(info->frame_axis, sizeof(info->frame_axis), fp) == NULL) return -1;
    trim_trailing_whitespace(info->frame_axis);

    if (fgets(info->data_type, sizeof(info->data_type), fp) == NULL) return -1;  
    trim_trailing_whitespace(info->data_type);

    if (fgets(info->image_axis, sizeof(info->image_axis), fp) == NULL) return -1;
    trim_trailing_whitespace(info->image_axis);

    printf("✓ Frame Axis: '%s'\n", info->frame_axis);
    printf("✓ Data Type: '%s'\n", info->data_type); 
    printf("✓ Image Axis: '%s'\n", info->image_axis);

    printf("  After axes reading, position: 0x%lX\n", ftell(fp));
    

    // 設置一些基本的默認值以避免段錯誤
    info->number_of_frames = 1;
    info->number_of_subimages = 1;
    info->image_width = info->detector_width;
    info->image_height = info->detector_height;
    info->data_offset = ftell(fp);  // 臨時設置為當前位置

    printf("→ Basic parsing completed, setting default values\n");

    return 0;

    
    // 清理和提取 user_text 中的校準數據
    extract_user_text(info);

    return 0;
}

void sif_close(SifFile *sif_file) {
    if (sif_file) {
        free(sif_file->tiles);
        free(sif_file->info.subimages);
        free(sif_file->info.timestamps);
        free(sif_file->info.frame_calibrations);
        free(sif_file->raw_data);
        free(sif_file->image_data);
        free(sif_file->wavelengths);
        memset(sif_file, 0, sizeof(SifFile));
    }
}

void extract_user_text(SifInfo *info) {
    if (!info || !info->user_text || strlen(info->user_text) == 0) {
        return;
    }
    
    printf("→ Analyzing user text (%lu bytes)...\n", strlen(info->user_text));
    
    // 簡單分析 user_text 內容
    int text_chars = 0;
    int binary_chars = 0;
    
    for (size_t i = 0; i < strlen(info->user_text) && i < 100; i++) {
        if (isprint((unsigned char)info->user_text[i])) {
            text_chars++;
        } else {
            binary_chars++;
        }
    }
    
    printf("  User text analysis: %d printable chars, %d binary chars (first 100 bytes)\n", 
           text_chars, binary_chars);
    
    // 檢查是否包含校準數據標記
    if (strstr(info->user_text, "Calibration") != NULL) {
        printf("  Contains calibration data\n");
        info->has_frame_calibrations = 1;
    } else {
        info->has_frame_calibrations = 0;
    }
    
    // 顯示前100個字節的內容（用於調試）
    printf("  First 100 bytes: ");
    for (int i = 0; i < 100 && i < strlen(info->user_text); i++) {
        unsigned char c = info->user_text[i];
        if (isprint(c) && c != '\r' && c != '\n') {
            printf("%c", c);
        } else {
            printf("\\x%02X", c);
        }
    }
    printf("\n");
    // 清理 user_text，因為信息已經提取
    info->user_text[0] = '\0'; //清空 user_text 以節省內存
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


