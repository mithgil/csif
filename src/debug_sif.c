#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sif_parser.h"
#include "sif_utils.h"

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

// ./bin/debug_sif '/home/tim/Documents/AS/data/andor/20250917/1OD_500uW_sapphire_200umFiber_.sif'
// ./bin/debug_sif '/home/tim/Documents/AS/data/andor/20250908/monochrom_430_700_10_LED_2.sif'

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <sif_file>\n", argv[0]);
        return 1;
    }
    
    const char *filename = argv[1];
    
    // 1. 首先用調試工具分析文件結構
    debug_parse_step_by_step(filename);
    printf("\n");
    
    // 2. 然後嘗試正常解析
    printf("=== Trying Normal Parsing ===\n");
    FILE *fp = fopen(filename, "rb");
    if (fp) {
        SifFile sif_file;
        if (sif_open(fp, &sif_file) == 0) {
            printf("✓ SIF file parsed successfully!\n");
            print_sif_info_summary(&sif_file.info);
            sif_close(&sif_file);
        } else {
            printf("✗ Failed to parse SIF file\n");
        }
        fclose(fp);
    }
    
    return 0;
}