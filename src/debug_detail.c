#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include "sif_utils.h"

/* Hex dump */
void sif_hex_dump(const char *filename, int num_bytes_to_dump, int print_all) {

    FILE *fp = fopen(filename, "rb");

    if (!fp) {
        printf("Error: Cannot open file %s\n", filename);
        return;
    }
    
    printf("=== Detailed SIF Structure Analysis ===\n");
    printf("File: %s\n\n", filename);
    
    // 獲取文件大小
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    printf("File size: %ld bytes\n\n", file_size);
    
    // 決定要讀取多少字節
    long bytes_to_read;
    if (print_all) {
        bytes_to_read = file_size;
        printf("Printing ALL bytes (full file content)\n");
    } else {
        bytes_to_read = (num_bytes_to_dump > 0) ? num_bytes_to_dump : 2000; // 預設2000
        if (bytes_to_read > file_size) {
            bytes_to_read = file_size;
        }
        printf("Printing first %ld bytes\n", bytes_to_read);
    }
    
    // 動態分配緩衝區
    unsigned char *buffer = (unsigned char*)malloc(bytes_to_read);
    if (!buffer) {
        printf("Error: Memory allocation failed for %ld bytes\n", bytes_to_read);
        fclose(fp);
        return;
    }
    
    long bytes_read = fread(buffer, 1, bytes_to_read, fp);
    
    printf("Bytes actually read: %ld bytes\n\n", bytes_read);
    
    printf("Hex Dump:\n");
    printf("Offset  Hex                                               ASCII\n");
    printf("------  ------------------------------------------------  ----------------\n");
    
    // 計算要顯示的行數
    long lines_to_display = (bytes_read + 15) / 16; // 每行16字節
    
    for (long i = 0; i < lines_to_display; i++) {
        long offset = i * 16;
        printf("%06lX  ", offset);
        
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
        
        // 標記關鍵位置（只在部分顯示時）
        if (!print_all) {
            if (offset == 0) printf("       ^-- Line 1: Magic string\n");
            if (offset == 0x10) printf("       ^-- Line 2: 65538 1\n");
            if (offset == 0x20) printf("       ^-- Line 3 starts\n");
            if (offset == 0xB30) printf("       ^-- Data region starts around here\n");
        }
    }
    
    // 如果沒有顯示完整文件，顯示統計資訊
    if (!print_all && bytes_read < file_size) {
        printf("\n... (truncated, %ld bytes not shown)\n", file_size - bytes_read);
        printf("Use print_all=1 to see full file content\n");
    }
    
    printf("\n=== Summary ===\n");
    printf("Total file size: %ld bytes\n", file_size);
    printf("Bytes displayed: %ld bytes\n", bytes_read);
    printf("Remaining bytes: %ld bytes\n", file_size - bytes_read);
    
    free(buffer);
    fclose(fp);
}

void analyze_sif_structure(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        printf("Error: Cannot open file %s\n", filename);
        return;
    }
    
    printf("=== Detailed SIF Structure Analysis ===\n");
    printf("File: %s\n\n", filename);
    
    // 讀取整個文件來分析結構
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    printf("File size: %ld bytes (0x%08lX)\n\n", file_size, file_size);
    
    // 重點分析時間戳附近的區域 (0x2000 - 0x2200)
    long focus_start = 0x2000;
    long focus_end = 0x2200;
    
    if (focus_end > file_size) {
        focus_end = file_size;
    }
    
    printf("=== Focus Analysis: Timestamps and Data Region (0x%04lX - 0x%04lX) ===\n", 
           focus_start, focus_end);
    
    fseek(fp, focus_start, SEEK_SET);
    long region_size = focus_end - focus_start;
    unsigned char *buffer = malloc(region_size);
    long bytes_read = fread(buffer, 1, region_size, fp);
    
    printf("Offset    Hex                                               ASCII\n");
    printf("--------  ------------------------------------------------  ----------------\n");
    
    for (long i = 0; i < bytes_read; i += 16) {
        long offset = focus_start + i;
        printf("%08lX  ", offset);
        
        // 顯示十六進制
        for (int j = 0; j < 16; j++) {
            if (i + j < bytes_read) {
                printf("%02X ", buffer[i + j]);
            } else {
                printf("   ");
            }
        }
        
        printf(" ");
        
        // 顯示 ASCII
        for (int j = 0; j < 16 && i + j < bytes_read; j++) {
            unsigned char c = buffer[i + j];
            if (isprint(c) && c != '\n' && c != '\r') {
                printf("%c", c);
            } else {
                printf(".");
            }
        }
        
        printf("\n");
        
        // 特別標記關鍵位置
        if (offset == 0x20A9) {
            printf("        ^-- After timestamps, before data (0x20A9)\n");
        }
        if (offset == 0x20AA) {
            printf("        ^-- Current data offset (0x20AA)\n");
        }
        
        // 檢查時間戳模式 (500個時間戳應該在附近)
        if (offset >= 0x2000 && offset <= 0x20A8) {
            // 檢查是否可能是時間戳 (數字字符)
            int is_timestamp = 1;
            for (int j = 0; j < 8 && i + j < bytes_read; j++) {
                if (!isdigit(buffer[i + j]) && buffer[i + j] != '\n' && buffer[i + j] != '\r') {
                    is_timestamp = 0;
                    break;
                }
            }
            if (is_timestamp && i + 8 < bytes_read) {
                char timestamp_str[9] = {0};
                memcpy(timestamp_str, &buffer[i], 8);
                printf("        ^-- Possible timestamp: %s\n", timestamp_str);
            }
        }
        
        // 檢查數據開始模式
        if (offset >= 0x20AA) {
            // 檢查是否可能是浮點數數據
            int could_be_float = 1;
            for (int j = 0; j < 16 && i + j < bytes_read; j += 4) {
                // 簡單檢查：連續4字節不全是可打印字符
                int printable_count = 0;
                for (int k = 0; k < 4 && i + j + k < bytes_read; k++) {
                    if (isprint(buffer[i + j + k]) && buffer[i + j + k] != '\n' && buffer[i + j + k] != '\r') {
                        printable_count++;
                    }
                }
                if (printable_count >= 3) {
                    could_be_float = 0;
                    break;
                }
            }
            
            if (could_be_float && i == 0) {
                printf("        ^-- Potential binary data start\n");
            }
        }
    }
    
    free(buffer);
    
    // 特別分析數據區域的頭幾個浮點數
    printf("\n=== Data Region Analysis ===\n");
    
    // 從當前推測的數據偏移開始
    long data_offsets[] = {0x20AA, 0x2100, 0x2200, 0x2300};
    
    for (int d = 0; d < 4; d++) {
        long test_offset = data_offsets[d];
        if (test_offset >= file_size) continue;
        
        fseek(fp, test_offset, SEEK_SET);
        float test_floats[10];
        size_t read_count = fread(test_floats, sizeof(float), 10, fp);
        
        if (read_count == 10) {
            printf("At offset 0x%08lX:\n", test_offset);
            printf("  As raw floats: ");
            for (int i = 0; i < 5; i++) {
                printf("%.1f ", test_floats[i]);
            }
            printf("\n");
            
            printf("  With byte swap: ");
            for (int i = 0; i < 5; i++) {
                uint32_t temp;
                memcpy(&temp, &test_floats[i], sizeof(uint32_t));
                temp = ((temp & 0xFF) << 24) | ((temp & 0xFF00) << 8) |
                       ((temp & 0xFF0000) >> 8) | ((temp & 0xFF000000) >> 24);
                float swapped;
                memcpy(&swapped, &temp, sizeof(float));
                printf("%.1f ", swapped);
            }
            printf("\n");
            
            // 顯示原始字節
            fseek(fp, test_offset, SEEK_SET);
            unsigned char bytes[20];
            fread(bytes, 1, 20, fp);
            printf("  Raw bytes: ");
            for (int i = 0; i < 20; i++) {
                printf("%02X ", bytes[i]);
            }
            printf("\n\n");
        }
    }
    
    // 搜索真正的數據開始位置
    printf("=== Searching for Real Data Start ===\n");
    
    for (long search_pos = 0x20AA; search_pos < file_size - 1000; search_pos += 4) {
        fseek(fp, search_pos, SEEK_SET);
        
        float test_values[5];
        if (fread(test_values, sizeof(float), 5, fp) == 5) {
            // 檢查是否在合理的光譜數據範圍內
            int reasonable_count = 0;
            for (int i = 0; i < 5; i++) {
                if (test_values[i] >= 600.0f && test_values[i] <= 700.0f) {
                    reasonable_count++;
                }
            }
            
            if (reasonable_count >= 3) {
                printf("FOUND: Potential data start at 0x%08lX\n", search_pos);
                printf("  Values: ");
                for (int i = 0; i < 5; i++) {
                    printf("%.1f ", test_values[i]);
                }
                printf("\n");
                break;
            }
            
            // 也檢查字節交換後的版本
            reasonable_count = 0;
            for (int i = 0; i < 5; i++) {
                uint32_t temp;
                memcpy(&temp, &test_values[i], sizeof(uint32_t));
                temp = ((temp & 0xFF) << 24) | ((temp & 0xFF00) << 8) |
                       ((temp & 0xFF0000) >> 8) | ((temp & 0xFF000000) >> 24);
                float swapped;
                memcpy(&swapped, &temp, sizeof(float));
                
                if (swapped >= 600.0f && swapped <= 700.0f) {
                    reasonable_count++;
                }
            }
            
            if (reasonable_count >= 3) {
                printf("FOUND: Potential data start at 0x%08lX (with byte swap)\n", search_pos);
                printf("  Values: ");
                for (int i = 0; i < 5; i++) {
                    uint32_t temp;
                    memcpy(&temp, &test_values[i], sizeof(uint32_t));
                    temp = ((temp & 0xFF) << 24) | ((temp & 0xFF00) << 8) |
                           ((temp & 0xFF0000) >> 8) | ((temp & 0xFF000000) >> 24);
                    float swapped;
                    memcpy(&swapped, &temp, sizeof(float));
                    printf("%.1f ", swapped);
                }
                printf("\n");
                break;
            }
        }
    }
    
    fclose(fp);
}

// ./bin/debug_detail_sif '/home/tim/Documents/AS/data/andor/20250908/monochrom_430_700_10_LED_2.sif'

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <sif_file>\n", argv[0]);
        return 1;
    }

    char filename[256];
    snprintf(filename, sizeof(filename), "%s", argv[1]);

    //FILE *fp = fopen(filename, "rb");
    //print_hex_dump(fp, 0x20AA, 2, 70);

    // 顯示前 2000 字節
    sif_hex_dump(filename, 2000, 0);
    // 顯示全部字節
    sif_hex_dump(filename, 2000, 1);
    
    
    //analyze_sif_structure(argv[1]);
    return 0;
}