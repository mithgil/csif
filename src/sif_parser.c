#include "sif_parser.h"
#include "sif_utils.h"
#include <ctype.h>
#include <inttypes.h>

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
    
    // 跳過前導空格
    while (input[i] != '\0' && isspace((unsigned char)input[i])) {
        i++;
    }
    
    // 複製字母字符直到遇到數字
    while (input[i] != '\0' && out_index < max_length - 1) {
        unsigned char c = input[i];
        
        if (isalpha(c) || c == ' ') {
            output[out_index++] = c;
            found_text = 1;
        } else if (isdigit(c)) {
            // 遇到數字，如果已經找到文本就停止
            if (found_text) break;
            // 如果還沒找到文本，繼續（跳過前導數字）
        } else {
            // 其他字符（標點等），如果已經找到文本就停止
            if (found_text) break;
        }
        i++;
    }
    
    output[out_index] = '\0';
    trim_trailing_whitespace(output);
    
    // 如果提取失敗，使用默認值
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

// 跳過空格和換行符 
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

    memset(sif_file, 0, sizeof(SifFile));
    sif_file->file_ptr = fp;
    
    // 明確初始化 SifInfo 內部的指針
    memset(&sif_file->info, 0, sizeof(SifInfo));

    info->raman_ex_wavelength = NAN;
    info->calibration_data[0] = '\0';  
    info->calibration_coeff_count = 0;
    info->has_frame_calibrations = 0;

    
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
    
    for (int i = 0; i < 3; i++) {
        int temp = read_int(fp);
        printf("  Skipped int %d: %d\n", i, temp);
    }

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
            info->user_text_length = user_text_length;
            printf("  User text: %d bytes\n", info->user_text_length);
        }
    }
    discard_line(fp); // 讀取換行符

    // Line 9: Shutter Time 信息
    printf("→ Line 9: Reading shutter time...\n");
    long line9_start = ftell(fp);
    printf("  Line 9 starts at offset: 0x%lX\n", line9_start);

    // 讀取標記並驗證
    int line9_marker = read_int(fp);
    if (line9_marker != 65538) {
        printf("  ⚠️ Unexpected marker in Line 9: %d (expected 65538)\n", line9_marker);
    }

    printf("  Line 9 marker: %d\n", line9_marker);

    discard_bytes(fp, 8);
    printf("  Skipped 8 bytes\n");

    info->shutter_time[0] = read_float(fp);
    info->shutter_time[1] = read_float(fp);

    // 檢查讀取的浮點數是否合理
    if (isnan(info->shutter_time[0]) || isnan(info->shutter_time[1])) {
        printf("  ❌ Failed to read shutter time values\n");
        return -1;
    }

    printf("✓ Shutter Time: %.6f, %.6f\n", info->shutter_time[0], info->shutter_time[1]);

    skip_spaces(fp); 
    printf("  After Line 9, position: 0x%lX\n", ftell(fp));

    printf("→ Version-specific skipping logic...\n");
    printf("  SIF Version: %d\n", info->sif_version);

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
    if (fgets(calib_line, sizeof(calib_line), fp) == NULL) {
        printf("  Warning: Failed to read calibration data line\n");
        info->calibration_data[0] = '\0'; // 設為空字串
    } else {
        trim_trailing_whitespace(calib_line);
        printf("✓ Calibration Data: %s\n", calib_line);
        
        // 正確複製字串到結構體
        strncpy(info->calibration_data, calib_line, sizeof(info->calibration_data) - 1);
        info->calibration_data[sizeof(info->calibration_data) - 1] = '\0';
    }

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

    printf("→ Skipping 4 lines after Raman wavelength...\n");
    for (int i = 0; i < 4; i++) {
        discard_line(fp);
    }

    long after_calib_pos = ftell(fp);
    printf("  Skipped 4 lines position: 0x%lX\n", after_calib_pos);

    // Frame Axis, Data Type, Image Axis
    printf("→ Reading axes as simple text lines...\n");

    // 讀取原始行
    if (fgets(info->frame_axis, sizeof(info->frame_axis), fp) == NULL) return -1;
    trim_trailing_whitespace(info->frame_axis);
    printf("  Raw Frame Axis: '%s'\n", info->frame_axis);

    if (fgets(info->data_type, sizeof(info->data_type), fp) == NULL) return -1;
    trim_trailing_whitespace(info->data_type);
    printf("  Raw Data Type: '%s'\n", info->data_type);

    if (fgets(info->image_axis, sizeof(info->image_axis), fp) == NULL) return -1;
    trim_trailing_whitespace(info->image_axis);
    printf("  Raw Image Axis: '%s'\n", info->image_axis);

    // 提取純文本部分 
    extract_text_part_robust(info->frame_axis, info->frame_axis, sizeof(info->frame_axis)); 
    extract_text_part_robust(info->data_type, info->data_type, sizeof(info->data_type));  

    // 保存純文字部分到臨時變數
    char temp[MAX_STRING_LENGTH];
    extract_text_part_robust(info->image_axis, temp, sizeof(temp));
    printf("  Text part: '%s'\n", temp);

    // 但保留原始字符串用於解析數字
    char *number_part = info->image_axis + strlen(temp);
    printf("  Number part: '%s'\n", number_part);

    printf("✓ Frame Axis: '%s'\n", info->frame_axis);
    printf("✓ Data Type: '%s'\n", info->data_type);

    // 解析數字
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

        // 設置數值到結構體
        info->number_of_frames = values[5];
        info->number_of_subimages = values[6];
        info->total_length = values[7];
        info->image_length = values[8];
        
    }
    
    // now can safely write the string to image_axis
    strcpy(info->image_axis, temp);
    printf("✓ Image Axis: '%s'\n", info->image_axis);

    printf("✓ Image info:\n");
    printf("  %-15s %d\n", "Frames:", info->number_of_frames);
    printf("  %-15s %d\n", "Subimages:", info->number_of_subimages);
    printf("  %-15s %d\n", "Total length:", info->total_length);
    printf("  %-15s %d\n", "Image length:", info->image_length);

    if (info->number_of_subimages > 0) {
        printf("→ Reading %d subimage(s) for binning information...\n", info->number_of_subimages);
        
        info->subimages = malloc(info->number_of_subimages * sizeof(SubImageInfo));
        
        for (int i = 0; i < info->number_of_subimages; i++) {
            SubImageInfo *sub = &info->subimages[i];
            
            int sub_marker = read_int(fp);
            printf("  Subimage %d marker: %d\n", i, sub_marker);
            
            // 讀取子圖像區域和 binning
            sub->x0 = read_int(fp);
            sub->y1 = read_int(fp);
            sub->x1 = read_int(fp);
            sub->y0 = read_int(fp);
            sub->ybin = read_int(fp);  // ← 這裡獲取 ybin
            sub->xbin = read_int(fp);  // ← 這裡獲取 xbin
            
            printf("    Area: (%d,%d)-(%d,%d), Binning: %dx%d\n",
                sub->x0, sub->y0, sub->x1, sub->y1, sub->xbin, sub->ybin);
            
            // 計算子圖像尺寸
            sub->width = (1 + sub->x1 - sub->x0) / sub->xbin;
            sub->height = (1 + sub->y1 - sub->y0) / sub->ybin;
            
            printf("    Size: %dx%d\n", sub->width, sub->height);
            
            // 設置全局 binning（通常所有子圖像的 binning 相同）
            if (i == 0) {
                info->xbin = sub->xbin;
                info->ybin = sub->ybin;
                info->image_width = sub->width;
                info->image_height = sub->height;
            }
        }
        
        printf("✓ Final image configuration:\n");
        printf("  Size: %dx%d pixels\n", info->image_width, info->image_height);
        printf("  Binning: %dx%d\n", info->xbin, info->ybin);
    }

    printf("  After layout parsing, position: 0x%lX\n", ftell(fp));
   
    //printf("→ Parsing until Image Axis and image info is complete\n");

    printf("→ Reading timestamps for %d frames...\n", info->number_of_frames);

    discard_line(fp);
    printf("  After skipping a line, position: 0x%lX\n", ftell(fp));

    // 讀取時間戳
    if (info->number_of_frames > 0) {
        info->timestamps = malloc(info->number_of_frames * sizeof(int64_t));
        if (!info->timestamps) {
            printf("❌ Failed to allocate memory for timestamps\n");
            return -1;
        }
        
        for (int f = 0; f < info->number_of_frames; f++) {
            char timestamp_str[64];
            if (fgets(timestamp_str, sizeof(timestamp_str), fp) == NULL) {
                printf("❌ Failed to read timestamp for frame %d\n", f);
                info->timestamps[f] = 0;
            } else {
                info->timestamps[f] = atoll(timestamp_str);
                printf("  Frame %d timestamp: %" PRId64 "\n", f, info->timestamps[f]);
            }
        }
    }
    printf("  After timestamps, position: 0x%lX\n", ftell(fp));

    printf("→ Determining data offset...\n");

    long before_data = ftell(fp);
    info->data_offset = before_data; // 默認數據偏移

    // 使用文字模式檢查
    //debug_print_some_lines(fp, before_data, 10);
    // 使用十六進制模式檢查
    //debug_hex_dump(fp, before_data, 4*1024);

    // 檢查是否有額外的標記，
    char line[256];

    printf("→ Reading data flag line at position: 0x%lX\n", before_data);

    if (fgets(line, sizeof(line), fp) != NULL) {
        // 去除換行符
        line[strcspn(line, "\n")] = '\0';
        
        printf("  Raw line content: '%s' (length: %lu)\n", line, strlen(line));
        
        // 解析整數
        int data_flag = 0;
        if (sscanf(line, "%d", &data_flag) == 1) {
            printf("  Parsed data flag: %d\n", data_flag);
            
            if (data_flag == 0) {
                info->data_offset = ftell(fp);  // 現在已經在下一行開頭
                printf("✓ Data starts after flag 0 at offset: 0x%lX\n", info->data_offset);
            } else if (data_flag == 1 && info->sif_version == 65567) {
                printf("  SIF 65567: skipping %d additional lines\n", info->number_of_frames);
                for (int i = 0; i < info->number_of_frames; i++) {
                    if (fgets(line, sizeof(line), fp) == NULL) break;
                    printf("    Skipped line %d: '%s'\n", i, line);
                }
                info->data_offset = ftell(fp);
                printf("✓ Data starts after version-specific data at offset: 0x%lX\n", info->data_offset);
            } else {
                // 其他情況，回到標記前
                fseek(fp, before_data, SEEK_SET);
                printf("✓ Data starts at original offset: 0x%lX\n", info->data_offset);
            }
        } else {
            // 解析整數失敗
            printf("  Failed to parse integer from line\n");
            fseek(fp, before_data, SEEK_SET);
            printf("✓ Data starts at original offset: 0x%lX\n", info->data_offset);
        }
    } else {
        // 讀取行失敗
        printf("  Failed to read line\n");
        fseek(fp, before_data, SEEK_SET);
        printf("✓ Data starts at original offset: 0x%lX\n", info->data_offset);
    }
        

    printf("→ Initializing SifFile structure and tiles...\n");

    sif_file->frame_count = info->number_of_frames;
    sif_file->tile_count = info->number_of_frames;
    
    // 分配和初始化 tiles
    if (sif_file->tile_count > 0) {
        sif_file->tiles = malloc(sif_file->tile_count * sizeof(ImageTile));
        if (sif_file->tiles) {
            // 計算每個幀的數據大小（width*height*no_subimages*4）
            int pixels_per_frame = info->image_width * info->image_height * info->number_of_subimages;
            int bytes_per_pixel = 4; // 32-bit float 
            
            printf("  Tile configuration:\n");
            printf("    Pixels per frame: %d\n", pixels_per_frame);
            printf("    Bytes per pixel: %d\n", bytes_per_pixel);
            printf("    Total bytes per frame: %d\n", pixels_per_frame * bytes_per_pixel);
            
            for (int f = 0; f < sif_file->tile_count; f++) {
                sif_file->tiles[f].offset = info->data_offset + f * pixels_per_frame * bytes_per_pixel;
                sif_file->tiles[f].width = info->image_width;
                sif_file->tiles[f].height = info->image_height;
                sif_file->tiles[f].frame_index = f;
                
                printf("    Tile %d: offset=0x%08lX, size=%dx%d\n", 
                    f, sif_file->tiles[f].offset,
                    sif_file->tiles[f].width, sif_file->tiles[f].height);
            }
            printf("✓ Allocated %d image tiles\n", sif_file->tile_count);
        } else {
            printf("❌ Failed to allocate memory for tiles\n");
            return -1;
        }
    }

    // 在 sif_open 中，呼叫 extract_user_text 之前：
    printf("  Before extract_user_text:\n");
    printf("    user_text pointer: %p\n", info->user_text);
    printf("    user_text[0]: 0x%02X\n", (unsigned char)info->user_text[0]);
    printf("    strlen(user_text): %lu\n", strlen(info->user_text));
    printf("    user_text_length: %d\n", info->user_text_length);

    // 手動檢查前幾個字節
    printf("    First 10 bytes: ");
    for (int i = 0; i < 10 && i < user_text_length; i++) {
        printf("%02X ", (unsigned char)info->user_text[i]);
    }
    printf("\n");


    // 清理和提取 user_text 中的校準數據
    extract_user_text(info);

    printf("✓ SIF file parsing successfully");

    return 0;
}


void extract_frame_calibrations(SifInfo *info, int start_pos) {
    if (!info || !info->user_text || start_pos < 0 || start_pos >= info->user_text_length) {
        return;
    }
    
    printf("→ Extracting frame calibration data from position %d\n", start_pos);
    
    // 複製 user_text 到可修改的緩衝區
    char* text_copy = malloc(info->user_text_length + 1);
    if (!text_copy) {
        printf("  Memory allocation failed\n");
        return;
    }
    memcpy(text_copy, info->user_text, info->user_text_length);
    text_copy[info->user_text_length] = '\0';
    
    char* current_pos = text_copy + start_pos;
    
    // 為每個 frame 提取校準數據
    for (int frame = 1; frame <= info->number_of_frames; frame++) {
        // 構建目標關鍵字 "Calibration data for frame X"
        char target[64];
        snprintf(target, sizeof(target), "Calibration data for frame %d", frame);
        
        // 尋找這個 frame 的校準數據
        char* frame_start = strstr(current_pos, target);
        if (!frame_start) {
            printf("  ✗ Calibration data for frame %d not found\n", frame);
            continue;
        }
        
        // 移動到數據部分（跳過關鍵字和冒號）
        char* data_start = frame_start + strlen(target);
        
        // 跳過空白和冒號
        while (*data_start && (isspace((unsigned char)*data_start) || *data_start == ':')) {
            data_start++;
        }
        
        if (!*data_start) {
            printf("  ✗ No data after calibration marker for frame %d\n", frame);
            continue;
        }
        
        // 找到行尾或數據結束
        char* data_end = data_start;
        while (*data_end && *data_end != '\n' && *data_end != '\r') {
            data_end++;
        }
        
        // 提取數據字串
        size_t data_len = data_end - data_start;
        char data_str[256];
        if (data_len >= sizeof(data_str)) {
            data_len = sizeof(data_str) - 1;
        }
        memcpy(data_str, data_start, data_len);
        data_str[data_len] = '\0';
        
        printf("  Frame %d calibration data: '%s'\n", frame, data_str);
        
        // 解析係數（逗號分隔）
        parse_frame_calibration_coefficients(info, frame, data_str);
        
        // 移動到下一個位置
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
    
    printf("    Parsing coefficients for frame %d: '%s'\n", frame, data_str);
    
    // 複製字串以便修改（使用 strtok 會修改原字串）
    char* data_copy = strdup(data_str);
    if (!data_copy) {
        printf("    Memory allocation failed for frame %d\n", frame);
        return;
    }
    
    char* token;
    char* rest = data_copy;
    int coeff_count = 0;
    double coefficients[20]; // 假設最多20個係數
    
    // 去除前後空白
    char* trimmed = rest;
    while (isspace((unsigned char)*trimmed)) trimmed++;
    char* end = trimmed + strlen(trimmed) - 1;
    while (end > trimmed && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    
    rest = trimmed;
    
    // 使用逗號分割（對應 Julia 的 split(..., ',')）
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
                
                if (endptr != token_trim) { // 成功轉換
                    coefficients[coeff_count++] = value;
                    printf("      Coefficient %d: %f\n", coeff_count, value);
                } else {
                    printf("      Warning: Failed to parse '%s' as float\n", token_trim);
                }
            }
        }
    }
    
    // 保存係數到 info 結構
    if (coeff_count > 0) {
        // 確保不會超出陣列範圍
        if (frame <= MAX_FRAMES) {
            info->frame_calibrations[frame-1].coeff_count = coeff_count;
            memcpy(info->frame_calibrations[frame-1].coefficients, coefficients, 
                   coeff_count * sizeof(double));
            printf("    ✓ Frame %d: %d coefficients parsed and saved\n", frame, coeff_count);
        } else {
            printf("    ✗ Frame %d: frame number exceeds maximum (%d)\n", frame, MAX_FRAMES);
        }
    } else {
        printf("    ✗ Frame %d: no valid coefficients found\n", frame);
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
        info->calibration_coeff_count = 0;  // 標記為失敗
        return;
    }
    
    printf("→ Parsing calibration coefficients from: '%s'\n", info->calibration_data);
    
    // 複製字串以便修改
    char* data_copy = strdup(info->calibration_data);
    if (!data_copy) {
        info->calibration_coeff_count = 0;
        return;
    }
    
    char* token;
    char* rest = data_copy;
    int coeff_count = 0;
    double coefficients[10];
    
    // 去除前後空白
    char* trimmed = rest;
    while (isspace((unsigned char)*trimmed)) trimmed++;
    char* end = trimmed + strlen(trimmed) - 1;
    while (end > trimmed && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    
    rest = trimmed;
    
    // 使用空白分割
    while ((token = strtok_r(rest, " \t", &rest))) {
        if (coeff_count < 10) {
            char* endptr;
            double value = strtod(token, &endptr);
            
            if (endptr != token) { // 成功轉換
                coefficients[coeff_count++] = value;
                printf("    Coefficient %d: %f\n", coeff_count, value);
            } else {
                // 解析失敗
                printf("    Failed to parse '%s' as float\n", token);
                free(data_copy);
                info->calibration_coeff_count = 0;
                return;
            }
        }
    }
    
    free(data_copy);
    
    if (coeff_count > 0) {
        // 保存解析後的係數
        info->calibration_coeff_count = coeff_count;
        memcpy(info->calibration_coefficients, coefficients, coeff_count * sizeof(double));

    } else {
        info->calibration_coeff_count = 0; // 失敗
    }
}

void extract_user_text(SifInfo *info) {
    if (!info || !info->user_text || info->user_text_length == 0) {
        printf("  Skip: no user text to process\n");
        return;
    }
    
    printf("→ extract_user_text analysis:\n");
    printf("  user_text_length: %d\n", info->user_text_length);
    printf("  calibration_data: '%s'\n", info->calibration_data);
    
    // 搜尋 "Calibration data for" - 只在 user_text 的前20個字節中搜尋！
    const char* target = "Calibration data for";
    int found = 0;
    
    int search_limit = (info->user_text_length < 20) ? info->user_text_length : 20;
    
    for (int i = 0; i <= search_limit - strlen(target); i++) {
        if (strncmp(&info->user_text[i], target, strlen(target)) == 0) {
            printf("  ✓ Found '%s' in first %d bytes of user_text at position %d\n", 
                   target, search_limit, i);
            found = 1;
            
            extract_frame_calibrations(info, i);
            
            info->calibration_data[0] = '\0';
            info->calibration_coeff_count = 0;
            
            break;
        }
    }
    
    if (!found) {
        printf("  ✗ '%s' not found in first %d bytes of user_text\n", target, search_limit);
        
        // 情況2: 沒有找到目標，處理現有的 calibration_data
        if (info->calibration_data[0] != '\0') {
            printf("  calibration_data is a string: '%s'\n", info->calibration_data);
            
            // 嘗試解析校準係數
            parse_calibration_coefficients(info);  // 現在是 void 函數
            
            // 檢查是否解析成功（通過 calibration_coeff_count）
            if (info->calibration_coeff_count > 0) {
                printf("  ✓ Successfully parsed %d calibration coefficients\n", 
                       info->calibration_coeff_count);
            } else {
                // 解析失敗，對應 Julia 的 delete!(meta, "Calibration_data")
                printf("  ✗ Failed to parse calibration coefficients, clearing data\n");
                info->calibration_data[0] = '\0';
                info->calibration_coeff_count = 0;
            }
        } else {
            printf("  calibration_data is empty or not a string, clearing\n");
            info->calibration_coeff_count = 0;
        }
    }
    
    info->user_text_processed = 1;
    printf("✓ User text processing completed\n");
}

// 專門的字節序交換函數
static void swap_float_array_endian(float *data, int count) {
    for (int i = 0; i < count; i++) {
        uint32_t temp;
        memcpy(&temp, &data[i], sizeof(uint32_t));
        temp = ((temp & 0xFF) << 24) | ((temp & 0xFF00) << 8) |
               ((temp & 0xFF0000) >> 8) | ((temp & 0xFF000000) >> 24);
        memcpy(&data[i], &temp, sizeof(float));
    }
}

// 修正的數據加載函數，帶字節序交換控制
int sif_load_all_frames(SifFile *sif_file, int enable_byte_swap) {
    if (!sif_file || !sif_file->file_ptr || sif_file->frame_count == 0) {
        return -1;
    }
    
    if (sif_file->data_loaded) {
        sif_unload_data(sif_file);
    }
    
    int frame_size = sif_file->tiles[0].width * sif_file->tiles[0].height;
    int total_pixels = sif_file->frame_count * frame_size;
    
    printf("→ Loading frame data%s:\n", enable_byte_swap ? " with endian correction" : "");
    printf("  Frame size: %d x %d = %d pixels\n", 
           sif_file->tiles[0].width, sif_file->tiles[0].height, frame_size);
    printf("  Byte swap: %s\n", enable_byte_swap ? "ENABLED" : "DISABLED");
    
    // 分配記憶體
    sif_file->frame_data = malloc(total_pixels * sizeof(float));
    if (!sif_file->frame_data) {
        printf("❌ Failed to allocate memory\n");
        return -1;
    }
    
    FILE *fp = sif_file->file_ptr;
    
    // 直接讀取所有數據
    for (int i = 0; i < sif_file->frame_count; i++) {
        long offset = sif_file->tiles[i].offset;
        fseek(fp, offset, SEEK_SET);
        
        float *frame_start = sif_file->frame_data + i * frame_size;
        size_t read_count = fread(frame_start, sizeof(float), frame_size, fp);
        
        if (read_count != frame_size) {
            printf("⚠️ Frame %d: Only read %zu/%d pixels\n", i, read_count, frame_size);
        }
        
        // 根據參數決定是否進行字節序交換
        if (enable_byte_swap) {
            swap_float_array_endian(frame_start, frame_size);
        }
        
        // 調試第一幀
        if (i == 0) {
            printf("  Frame 0%s:\n", enable_byte_swap ? " after byte swap" : " (raw)");
            
            // 重新讀取原始字節來對比
            fseek(fp, offset, SEEK_SET);
            unsigned char raw_bytes[40];
            fread(raw_bytes, 1, 40, fp);
            
            printf("    Original bytes -> Values:\n");
            for (int j = 0; j < 10 && j < frame_size; j++) {
                printf("    Pixel %d: %02X %02X %02X %02X -> %.1f\n",
                       j, raw_bytes[j*4], raw_bytes[j*4+1], 
                       raw_bytes[j*4+2], raw_bytes[j*4+3], frame_start[j]);
            }
            
            // 檢查是否有合理的值
            int valid_count = 0;
            for (int j = 0; j < frame_size; j++) {
                if (frame_start[j] > 600.0f && frame_start[j] < 700.0f) {
                    valid_count++;
                    if (valid_count <= 3) {
                        printf("    Valid value at pixel %d: %.1f\n", j, frame_start[j]);
                    }
                }
            }
            printf("    Total valid values (600-700 range): %d/%d\n", valid_count, frame_size);
        }
    }
    
    sif_file->data_loaded = 1;
    printf("✓ Loaded %d frames%s\n", sif_file->frame_count, 
           enable_byte_swap ? " with endian correction" : "");
    return 0;
}

// 獲取幀數據指針
float* sif_get_frame_data(SifFile *sif_file, int frame_index) {
    if (!sif_file || !sif_file->frame_data || 
        frame_index < 0 || frame_index >= sif_file->frame_count) {
        return NULL;
    }
    
    int frame_size = sif_file->tiles[0].width * sif_file->tiles[0].height;
    return sif_file->frame_data + frame_index * frame_size;
}

// 獲取像素值
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

// 複製幀數據到用戶緩衝區
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
    
    printf("→ Closing SIF file and freeing resources...\n");
    
    // 釋放幀數據
    sif_unload_data(sif_file);
    
    // 釋放 tiles 數組
    if (sif_file->tiles) {
        free(sif_file->tiles);
        sif_file->tiles = NULL;
        printf("✓ Freed tiles array\n");
    }
    
    // 清理 info 結構體中的動態內存
    cleanup_sif_info(&sif_file->info);
    
    // 重置計數器
    sif_file->frame_count = 0;
    sif_file->tile_count = 0;
    sif_file->data_loaded = 0;
    
    // 注意：不關閉 file_ptr，由調用者管理文件指針
    sif_file->file_ptr = NULL;
    
    printf("✓ SIF file closed successfully\n");
}
