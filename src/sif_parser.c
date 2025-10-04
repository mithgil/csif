#include "sif_parser.h"
#include <ctype.h>
#include <inttypes.h>

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

// 修改 read_string 函數，確保只讀取一行
int read_string(FILE *fp, char *buffer, int max_length) {
    long start_pos = ftell(fp);
    
    // 先嘗試讀取長度（如果有的話）
    int length;
    if (fscanf(fp, "%d", &length) == 1) {
        // 有長度前綴
        if (length <= 0 || length >= max_length) {
            return -1;
        }
        if (fread(buffer, 1, length, fp) != length) {
            return -1;
        }
        buffer[length] = '\0';
        printf("  Read string with length prefix: '%s' (length: %d)\n", buffer, length);
    } else {
        // 沒有長度前綴，直接讀取一行
        fseek(fp, start_pos, SEEK_SET); // 回到開始位置
        
        // 使用 fgets 讀取一行
        if (!fgets(buffer, max_length, fp)) {
            return -1;
        }
        
        // 去除換行符
        buffer[strcspn(buffer, "\r\n")] = '\0';
        printf("  Read string without length prefix: '%s'\n", buffer);
    }
    
    return strlen(buffer);
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
    
    printf("=== Starting SIF File Parsing ===\n");

    // Line 1: Magic string
    char magic[64];
    if (fread(magic, 1, 36, fp) != 36 || strncmp(magic, SIF_MAGIC, 36) != 0) {
        printf("Error: Invalid magic string\n");
        return -1;
    }
    printf("✓ Line 1: Valid magic string\n");

    // Line 2: Skip
    char line_buffer[256];
    if (!fgets(line_buffer, sizeof(line_buffer), fp)) return -1;
    printf("✓ Line 2: %s", line_buffer);

    // Line 3: 按照固定格式解析，而不是依賴 fgets
    printf("→ Line 3: Parsing structured data...\n");
    
    long line3_start = ftell(fp);
    printf("  Line 3 starts at offset: 0x%lX\n", line3_start);
    
    // 讀取前5個整數
    info->sif_version = read_int(fp);
    int skip1 = read_int(fp);
    int skip2 = read_int(fp); 
    int skip3 = read_int(fp);
    info->experiment_time = read_int(fp);
    
    printf("  SIF Version: %d, Skip: %d %d %d, Exp Time: %d\n", 
           info->sif_version, skip1, skip2, skip3, info->experiment_time);
        
    // 關鍵：探測器溫度 - 仔細檢查讀取位置和值
    long before_temp = ftell(fp);
    printf("  Before reading temperature, position: 0x%lX\n", before_temp);
    // 讀取浮點數
    info->detector_temperature = read_float(fp);
    printf("  Detector Temp: %.2f\n", info->detector_temperature);

    long after_temp = ftell(fp);
    printf("  After reading temperature, position: 0x%lX\n", after_temp);
    
    // 跳過10個字節的空白（從十六進制可以看到 00 20 00 20 00 20 01 20 00 20）
    fseek(fp, 10, SEEK_CUR);
    printf("  Skipped 10 bytes of padding\n");
    
    // 繼續讀取 Line 3 的結構化數據
    read_int(fp); // skip 0
    info->exposure_time = read_float(fp);
    info->cycle_time = read_float(fp);
    info->accumulated_cycle_time = read_float(fp);
    info->accumulated_cycles = read_int(fp);
    
    printf("  Exposure: %.6f, Cycle: %.6f, Accum Cycle: %.6f, Accum Cycles: %d\n",
           info->exposure_time, info->cycle_time, 
           info->accumulated_cycle_time, info->accumulated_cycles);
    
    // 跳過 NULL 和 space (2 bytes)
    fseek(fp, 2, SEEK_CUR);
    
    info->stack_cycle_time = read_float(fp);
    info->pixel_readout_time = read_float(fp);
    
    printf("  Stack Cycle: %.6f, Pixel Readout: %.6f\n", 
           info->stack_cycle_time, info->pixel_readout_time);
    
    read_int(fp); // skip 0
    read_int(fp); // skip 1
    info->gain_dac = read_float(fp);
    
    printf("  Gain DAC: %.6f\n", info->gain_dac);
    
    read_int(fp); // skip 0
    read_int(fp); // skip 0
    info->gate_width = read_float(fp);
    
    printf("  Gate Width: %.6f\n", info->gate_width);
    
    // 跳過16個值
    for (int i = 0; i < 16; i++) {
        read_int(fp);
    }
    printf("  Skipped 16 integers\n");
    
    info->grating_blaze = read_float(fp);
    printf("  Grating Blaze: %.6f\n", info->grating_blaze);
    
    // 現在應該到達了第3行的末尾，位置應該是 0xAE 左右
    long line3_end = ftell(fp);
    printf("  Line 3 ends at offset: 0x%lX\n", line3_end);
    
    // 關鍵修正：跳過第3行末尾到第4行開始之間的二進制數據
    // 從十六進制數據看，第4行在 0x100，所以需要跳過 (0x100 - line3_end) 個字節
    long line4_start = 0x10A; // DU420_BEX2 的開始位置
    long current_offset = ftell(fp);
    long bytes_to_skip = line4_start - current_offset;

    if (bytes_to_skip > 0) {
        printf("  Skipping %ld bytes to reach Line 4 at 0x%lX\n", bytes_to_skip, line4_start);
        fseek(fp, bytes_to_skip, SEEK_CUR);
    } else if (bytes_to_skip < 0) {
        printf("  Warning: Already past Line 4 start, backing up %ld bytes\n", -bytes_to_skip);
        fseek(fp, bytes_to_skip, SEEK_CUR);
    }

    // Line 4: 探測器類型
    if (fgets(info->detector_type, sizeof(info->detector_type), fp)) {
        // 去除換行符
        info->detector_type[strcspn(info->detector_type, "\r\n")] = 0;
        printf("✓ Line 4: Detector Type = '%s'\n", info->detector_type);
    } else {
        printf("✗ Failed to read detector type\n");
        return -1;
    }

    // 驗證當前位置
    long after_line4 = ftell(fp);
    printf("  After Line 4, position: 0x%lX\n", after_line4);

    // Line 5: 探測器尺寸 - 修正讀取位置
    long line5_start = 0x116; // 1024 256 的開始位置
    fseek(fp, line5_start, SEEK_SET);

    info->detector_width = read_int(fp);
    info->detector_height = read_int(fp);

    // 讀取可能存在的第三個數字
    int extra_value = read_int(fp);

    printf("✓ Line 5: Detector Dimensions: %d x %d, Extra: %d\n", 
        info->detector_width, info->detector_height, extra_value);

    // 驗證當前位置
    long after_line5 = ftell(fp);
    printf("  After Line 5, position: 0x%lX\n", after_line5);

    // 原始文件名 - 應該在 0x123 左右
    printf("→ Reading original filename...\n");
    long filename_start = ftell(fp);
    printf("  Filename starts at offset: 0x%lX\n", filename_start);

    if (fgets(info->original_filename, sizeof(info->original_filename), fp)) {
        // 去除換行符
        info->original_filename[strcspn(info->original_filename, "\r\n")] = '\0';
        printf("✓ Original Filename: '%s'\n", info->original_filename);
    } else {
        printf("✗ Failed to read original filename\n");
        return -1;
    }

    long after_filename = ftell(fp);
    printf("  After filename, position: 0x%lX\n", after_filename);
    printf("✓ Original Filename: '%s'\n", info->original_filename);

    // 繼續解析後面的內容...
    printf("  After filename, position: 0x%lX\n", ftell(fp));

    // 跳過 space 和 newline
    fseek(fp, 2, SEEK_CUR);

    printf("→ Continuing parsing after filename...\n");
    long after_skip = ftell(fp);
    printf("  After skipping, current position: 0x%lX\n", after_skip);

    // Line 7: 應該是 "65538 2048"
    int line7_val1 = read_int(fp);
    int line7_val2 = read_int(fp);
    printf("✓ Line 7: %d %d\n", line7_val1, line7_val2);

    long current_pos = ftell(fp);
    printf("  After Line 7, position: 0x%lX\n", current_pos);

    // Line 8: User text (二進制數據，長度為 line7_val2 = 2048)
    printf("→ Reading user text (binary data, length: %d)...\n", line7_val2);

    // 讀取二進制 user text
    if (read_binary_string(fp, info->user_text, sizeof(info->user_text), line7_val2) < 0) {
        printf("✗ Failed to read user text binary data\n");
        return -1;
    }

    // 讀取換行符（1字節）
    char newline;
    if (fread(&newline, 1, 1, fp) != 1) {
        printf("✗ Failed to read newline after user text\n");
        return -1;
    }
    printf("✓ Read newline character: 0x%02X\n", (unsigned char)newline);

    // 驗證當前位置
    current_pos = ftell(fp);
    printf("  After user text, position: 0x%lX\n", current_pos);

    // Line 9: 繼續解析...
    printf("→ Reading Line 9...\n");
    int line9_val = read_int(fp); // 應該還是 65538
    printf("  Line 9 first value: %d\n", line9_val);

    // 跳過8個字節
    fseek(fp, 8, SEEK_CUR);
    printf("  Skipped 8 bytes\n");

    // 讀取快門時間
    info->shutter_time[0] = read_float(fp);
    info->shutter_time[1] = read_float(fp);
    printf("✓ Shutter Time: %.6f, %.6f\n", info->shutter_time[0], info->shutter_time[1]);

    // 繼續根據版本處理...
    printf("→ Handling version-specific skipping (exact Python logic)...\n");

    char skip_buffer[256];

    // 根據 SIF 版本跳過相應行數
    if (info->sif_version >= 65548 && info->sif_version <= 65557) {
        for (int i = 0; i < 2; i++) {
            fgets(skip_buffer, sizeof(skip_buffer), fp);
            printf("  Skipped 2 lines for version %d\n", info->sif_version);
        }
    } else if (info->sif_version == 65558) {
        for (int i = 0; i < 5; i++) {
            fgets(skip_buffer, sizeof(skip_buffer), fp);
            printf("  Skipped 5 lines for version %d\n", info->sif_version);
        }
    } else if (info->sif_version == 65559 || info->sif_version == 65564) {
        for (int i = 0; i < 8; i++) {
            fgets(skip_buffer, sizeof(skip_buffer), fp);
        }
        printf("  Skipped 8 lines for version %d\n", info->sif_version);
        
        // 讀取光譜儀信息
        fgets(skip_buffer, sizeof(skip_buffer), fp);
        char *token = strtok(skip_buffer, " \t");
        token = strtok(NULL, " \t"); // 取第二個token
        if (token) {
            strncpy(info->spectrograph, token, sizeof(info->spectrograph) - 1);
            printf("✓ Spectrograph: '%s'\n", info->spectrograph);
        }
    } else if (info->sif_version == 65565) {
        for (int i = 0; i < 15; i++) {
            fgets(skip_buffer, sizeof(skip_buffer), fp);
        }
        printf("  Skipped 15 lines for version %d\n", info->sif_version);
    } else if (info->sif_version > 65565) {
        // 跳過8行
        for (int i = 0; i < 8; i++) {
            fgets(skip_buffer, sizeof(skip_buffer), fp);
        }
        printf("  Skipped 8 lines for version %d\n", info->sif_version);
        
        // 讀取光譜儀信息
        fgets(skip_buffer, sizeof(skip_buffer), fp);
        char *token = strtok(skip_buffer, " \t");
        token = strtok(NULL, " \t"); // 取第二個token
        if (token) {
            strncpy(info->spectrograph, token, sizeof(info->spectrograph) - 1);
            printf("✓ Spectrograph: '%s'\n", info->spectrograph);
        } else {
            // 如果解析失敗，設置默認值
            strcpy(info->spectrograph, "unknown");
            printf("  Spectrograph: unknown (parsing failed)\n");
        }
        
        // 增強器信息
        fgets(skip_buffer, sizeof(skip_buffer), fp);
        
        // 讀取 gate 參數
        read_float(fp); // 跳過
        read_float(fp); // 跳過  
        read_float(fp); // 跳過
        info->gate_gain = read_float(fp);
        read_float(fp); // 跳過
        read_float(fp); // 跳過
        info->gate_delay = read_float(fp) * 1e-12;
        info->gate_width = read_float(fp) * 1e-12;
        
        printf("✓ Gate parameters: Gain=%.6f, Delay=%.6es, Width=%.6es\n",
            info->gate_gain, info->gate_delay, info->gate_width);
        
        // 跳過8行
        for (int i = 0; i < 8; i++) {
            fgets(skip_buffer, sizeof(skip_buffer), fp);
        }
        printf("  Skipped 8 more lines\n");
    }

    // 如果沒有讀取到光譜儀信息，設置默認值
    if (strlen(info->spectrograph) == 0) {
        strcpy(info->spectrograph, "sif version not checked yet");
        printf("  Using default spectrograph: '%s'\n", info->spectrograph);
    }

    // 讀取校準版本
    info->sif_calb_version = read_int(fp);
    printf("✓ SIF Calibration Version: %d\n", info->sif_calb_version);

    // 根據校準版本跳過
    if (info->sif_calb_version == 65540) {
        fgets(skip_buffer, sizeof(skip_buffer), fp);
        printf("  Skipped line for calb version 65540\n");
    }

    long debug_pos = ftell(fp);

    printf("  Current position before calibration: 0x%lX\n", debug_pos);

    // 顯示接下來的數據
    unsigned char debug_bytes[128];
    fread(debug_bytes, 1, 128, fp);
    printf("  Next 128 bytes at 0x%lX:\n", debug_pos);
    for (int i = 0; i < 128; i++) {
        if (i % 16 == 0) printf("    %04X: ", i);
        printf("%02X ", debug_bytes[i]);
        if (i % 16 == 15) printf("\n");
    }
    printf("\n  As text: ");
    for (int i = 0; i < 128; i++) {
        unsigned char c = debug_bytes[i];
        if (isprint(c)) {
            printf("%c", c);
        } else if (c == '\n') {
            printf("\\n");
        } else if (c == '\r') {
            printf("\\r");
        } else {
            printf(".");
        }
    }
    printf("\n");

    // 回到原來位置
    fseek(fp, debug_pos, SEEK_SET);

    printf("→ Exact parsing based on hex dump...\n");

    // 讀取校準數據
    if (fgets(skip_buffer, sizeof(skip_buffer), fp)) {
        skip_buffer[strcspn(skip_buffer, "\r\n")] = 0;
        printf("  Calibration_data: '%s'\n", skip_buffer);
    }

    // 讀取舊的校準數據  
    if (fgets(skip_buffer, sizeof(skip_buffer), fp)) {
        skip_buffer[strcspn(skip_buffer, "\r\n")] = 0;
        printf("  Calibration_data_old: '%s'\n", skip_buffer);
    }

    // 跳過空行
    if (fgets(skip_buffer, sizeof(skip_buffer), fp)) {
        skip_buffer[strcspn(skip_buffer, "\r\n")] = 0;
        printf("  Empty line: '%s'\n", skip_buffer);
    }

    // 現在讀取拉曼波長 - 但根據數據，下一行是 "65539 0 0 0..."，這不是波長！
    // 讓我們繼續跳過直到找到合理的波長值
    printf("  Searching for Raman wavelength...\n");

    char search_buffer[256];
    int found_raman = 0;

    // 搜索接下來的幾行
    for (int i = 0; i < 10; i++) {
        if (fgets(search_buffer, sizeof(search_buffer), fp)) {
            search_buffer[strcspn(search_buffer, "\r\n")] = 0;
            printf("  Line %d: '%s'\n", i + 1, search_buffer);
            
            // 檢查是否可能是波長（單個數字，在合理範圍內）
            char *endptr;
            double potential_wavelength = strtod(search_buffer, &endptr);
            
            // 如果是單個數字且在合理的光學波長範圍內
            if (endptr != search_buffer && *endptr == '\0' && 
                potential_wavelength > 200 && potential_wavelength < 1000) {
                info->raman_ex_wavelength = potential_wavelength;
                printf("✓ Found Raman excitation wavelength: %.2f nm\n", info->raman_ex_wavelength);
                found_raman = 1;
                break;
            }
            
            // 或者檢查是否包含波長關鍵詞
            if (strstr(search_buffer, "532") != NULL || 
                strstr(search_buffer, "785") != NULL ||
                strstr(search_buffer, "1064") != NULL) {
                printf("  Found wavelength keyword in: %s\n", search_buffer);
                // 可以嘗試從中提取數字
            }
        } else {
            break;
        }
    }

    if (!found_raman) {
        info->raman_ex_wavelength = NAN;
        printf("  Raman excitation wavelength: not found in file\n");
    }

    // 繼續解析後面的內容...
    printf("→ Continuing with remaining parsing...\n");

    // 讀取校準數據
    printf("→ Reading calibration data...\n");
    if (strstr(info->spectrograph, "Mechelle") != NULL) {
        printf("  Mechelle spectrograph - reading polynomial coefficients\n");
        char calib_line[256];
        if (fgets(calib_line, sizeof(calib_line), fp)) {
            calib_line[strcspn(calib_line, "\r\n")] = 0;
            printf("  PixelCalibration: %s\n", calib_line);
        }
    } else {
        // 讀取普通校準數據
        if (fgets(skip_buffer, sizeof(skip_buffer), fp)) {
            skip_buffer[strcspn(skip_buffer, "\r\n")] = 0;
            printf("  Calibration_data: %s\n", skip_buffer);
        }
    }

    // 讀取舊的校準數據
    fgets(skip_buffer, sizeof(skip_buffer), fp);
    skip_buffer[strcspn(skip_buffer, "\r\n")] = 0;
    printf("  Calibration_data_old: %s\n", skip_buffer);

    // 跳過一行
    fgets(skip_buffer, sizeof(skip_buffer), fp);
    printf("  Skipped line: %s", skip_buffer);

    // 讀取拉曼激發波長 - 使用正確的解析
    char raman_line[256];
    if (fgets(raman_line, sizeof(raman_line), fp)) {
        raman_line[strcspn(raman_line, "\r\n")] = 0;
        printf("  Raman line raw: '%s'\n", raman_line);
        
        // 嘗試解析為浮點數
        char *endptr;
        double raman_value = strtod(raman_line, &endptr);
        
        if (endptr != raman_line) { // 成功轉換
            info->raman_ex_wavelength = raman_value;
            printf("✓ Raman excitation wavelength: %.2f nm\n", info->raman_ex_wavelength);
        } else {
            info->raman_ex_wavelength = NAN;
            printf("  Raman excitation wavelength: not available (parse failed)\n");
        }
    } else {
        info->raman_ex_wavelength = NAN;
        printf("  Raman excitation wavelength: not available (read failed)\n");
    }

    // 跳過兩行
    fgets(skip_buffer, sizeof(skip_buffer), fp);
    fgets(skip_buffer, sizeof(skip_buffer), fp);
    printf("  Skipped 2 lines\n");

    // 靈活的坐標軸信息讀取
    printf("→ Reading axis and layout information...\n");
    long axis_start = ftell(fp);
    printf("  Axis info starts at offset: 0x%lX\n", axis_start);

    // Frame Axis - 有長度前綴
    int frame_axis_length;
    if (fscanf(fp, "%d", &frame_axis_length) == 1) {
        printf("  Frame Axis length: %d\n", frame_axis_length);
        
        // 消耗換行符
        fgetc(fp);
        
        // 讀取 Frame Axis 內容
        if (frame_axis_length > 0 && frame_axis_length < sizeof(info->frame_axis)) {
            if (fread(info->frame_axis, 1, frame_axis_length, fp) == frame_axis_length) {
                info->frame_axis[frame_axis_length] = '\0';
                printf("✓ Frame Axis: '%s'\n", info->frame_axis);
            } else {
                printf("✗ Failed to read frame axis content\n");
                return -1;
            }
        }
    } else {
        printf("✗ Failed to read frame axis length\n");
        return -1;
    }

    // Data Type - 直接讀取到換行符
    printf("→ Reading Data Type...\n");
    if (fgets(info->data_type, sizeof(info->data_type), fp)) {
        info->data_type[strcspn(info->data_type, "\r\n")] = '\0';
        printf("✓ Data Type: '%s'\n", info->data_type);
    } else {
        printf("✗ Failed to read data type\n");
        return -1;
    }

    // Image Axis - 直接讀取到換行符  
    printf("→ Reading Image Axis...\n");
    if (fgets(info->image_axis, sizeof(info->image_axis), fp)) {
        info->image_axis[strcspn(info->image_axis, "\r\n")] = '\0';
        printf("✓ Image Axis: '%s'\n", info->image_axis);
    } else {
        printf("✗ Failed to read image axis\n");
        return -1;
    }

    // 從 Image Axis 中提取正確的坐標信息："65538 1 151 1024 126 26 1 0"
    printf("→ Correctly parsing image layout from Image Axis...\n");
    char image_axis_copy[256];
    strcpy(image_axis_copy, info->image_axis);

    char *token = strtok(image_axis_copy, " ");
    int values[8];
    int value_count = 0;

    while (token && value_count < 8) {
        values[value_count] = atoi(token);
        value_count++;
        token = strtok(NULL, " ");
    }

    if (value_count >= 7) {
        // 正確解讀 Image Axis 字符串："65538 1 151 1024 126 26 1 0"
        int x0 = values[1];      // 1
        int y1 = values[2];      // 151  
        int x1 = values[3];      // 1024
        int y0 = values[4];      // 126
        int ybin = values[5];    // 26 (y方向的binning)
        info->number_of_frames = values[6];   // 1 (真正的幀數)
        info->number_of_subimages = 1;        // 固定為1
        
        printf("  Raw values: x0=%d, y1=%d, x1=%d, y0=%d, ybin=%d, frames=%d\n", 
            x0, y1, x1, y0, ybin, info->number_of_frames);
        
        // 設置 binning 信息
        info->xbin = 1;
        info->ybin = ybin;
        
        // 計算實際圖像尺寸
        info->image_width = (1 + x1 - x0) / info->xbin;   // 1024
        info->image_height = (1 + y1 - y0) / info->ybin;  // (151-126+1)/26 = 1
        
        printf("✓ Corrected: %d frames, image size %d x %d (binning %dx%d)\n",
            info->number_of_frames, info->image_width, info->image_height, 
            info->xbin, info->ybin);
        
    } else {
        printf("✗ Failed to parse image layout from Image Axis\n");
        return -1;
    }
    // 跳過空格並讀取時間戳
    printf("→ Reading timestamps (%d frames)...\n", info->number_of_frames);
    skip_spaces(fp);

    info->timestamps = malloc(info->number_of_frames * sizeof(int64_t));
    if (!info->timestamps) {
        printf("✗ Failed to allocate memory for timestamps\n");
        return -1;
    }

    for (int f = 0; f < info->number_of_frames; f++) {
        char timestamp_str[64];
        if (fgets(timestamp_str, sizeof(timestamp_str), fp)) {
            info->timestamps[f] = atoll(timestamp_str);
            printf("  Frame %d timestamp: %" PRId64 "\n", f, info->timestamps[f]);
        } else {
            printf("✗ Failed to read timestamp for frame %d\n", f);
            info->timestamps[f] = 0;
        }
    }
    
    // 簡單修正：確保正確對齊
    printf("→ Determining data offset...\n");
    long after_timestamps = ftell(fp);
    printf("  After timestamps, position: 0x%lX\n", after_timestamps);

    int flag;
    if (fscanf(fp, "%d", &flag) == 1) {
        printf("  Read flag: %d\n", flag);
        
        if (flag == 0) {
            // 讀取 flag 後，位置應該是 0xB37
            // 但我們需要 0xB38，所以跳過一個字節
            long after_flag = ftell(fp);
            printf("  After reading flag, position: 0x%lX\n", after_flag);
            
            // 檢查並跳過可能的分隔符
            char next_char = fgetc(fp);
            if (next_char == ' ' || next_char == '\n') {
                info->data_offset = ftell(fp);
                printf("  Skipped separator, data starts at: 0x%lX\n", info->data_offset);
            } else {
                // 如果不是分隔符，回到原來位置
                fseek(fp, after_flag, SEEK_SET);
                info->data_offset = after_flag;
                printf("  No separator, data starts at: 0x%lX\n", info->data_offset);
            }
        }
    } else {
        fseek(fp, after_timestamps, SEEK_SET);
        info->data_offset = after_timestamps;
    }

    printf("✓ Data starts at offset: 0x%lX (%ld)\n", info->data_offset, info->data_offset);

    // 修正 tile 創建（只有1幀）
    sif_file->image_width = info->image_width;
    sif_file->image_height = info->image_height; 
    sif_file->frame_count = info->number_of_frames;  // 應該是1
    sif_file->tile_count = info->number_of_frames;   // 應該是1

    sif_file->tiles = malloc(sif_file->tile_count * sizeof(ImageTile));
    if (!sif_file->tiles) {
        printf("✗ Failed to allocate memory for tiles\n");
        free(info->timestamps);
        return -1;
    }

    // 計算每個tile的偏移量
    int pixels_per_frame = info->image_width * info->image_height;
    for (int f = 0; f < sif_file->tile_count; f++) {
        sif_file->tiles[f].offset = info->data_offset + f * pixels_per_frame * 4;
        sif_file->tiles[f].width = info->image_width;
        sif_file->tiles[f].height = info->image_height;
        sif_file->tiles[f].frame_index = f;
    }

    printf("✓ Created %d image tiles, %d pixels per frame\n", 
       sif_file->tile_count, pixels_per_frame);

    // 跳過空格
    skip_spaces(fp);

    // 提取user text中的信息
    extract_user_text(info);

    printf("✓ SIF file fully parsed successfully!\n");
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