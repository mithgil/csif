#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Hex dump */

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
    
    printf("File size: %ld bytes\n\n", file_size);
    
    // 分析前1000字節的詳細結構
    unsigned char buffer[1000];
    long bytes_read = fread(buffer, 1, 1000, fp);
    
    printf("First 1000 bytes analysis:\n");
    printf("Offset  Hex                                               ASCII\n");  // 修正：去掉 = 號
    printf("------  ------------------------------------------------  ----------------\n");  // 修正：去掉 = 號
    
    for (int i = 0; i < bytes_read; i += 16) {
        printf("%06X  ", i);
        
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
            if (isprint(c)) {
                printf("%c", c);
            } else {
                printf(".");
            }
        }
        
        printf("\n");
        
        // 特別標記關鍵位置
        if (i == 0) printf("       ^-- Line 1: Magic string\n");
        if (i == 36) printf("       ^-- Line 2: 65538 1\n");
        if (i == 44) printf("       ^-- Line 3 starts\n");
        if (i == 72) printf("       ^-- Line 3 text ends, binary data starts\n");
        if (i == 266) printf("       ^-- Line 5: Detector info\n");
    }
    
    fclose(fp);
}

// ./bin/debug_detail_sif '/home/tim/Documents/AS/data/andor/20250908/monochrom_430_700_10_LED_2.sif'

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <sif_file>\n", argv[0]);
        return 1;
    }
    
    analyze_sif_structure(argv[1]);
    return 0;
}