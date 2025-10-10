#include "sif_parser.h"
#include "sif_json.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <sif_file>\n", argv[0]);
        return 1;
    }
    
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file %s\n", argv[1]);
        return 1;
    }
    
    SifFile sif_file = {0};
    
    // 設置為靜默模式，不輸出調試信息
    sif_set_verbose_level(SIF_SILENT);
    
    if (sif_open(fp, &sif_file) != 0) {
        fprintf(stderr, "Error: Failed to parse SIF file\n");
        fclose(fp);
        return 1;
    }
    
    // 使用現有的 sif_load_all_frames 函數
    if (sif_load_all_frames(&sif_file, 0) != 0) {
        // 即使加載失敗，我們仍然可以輸出元數據
        // 不輸出警告到 stdout，只輸出到 stderr
        fprintf(stderr, "Warning: Could not load frame data\n");
    }
    
    // 輸出 JSON
    JsonOutputOptions options = JSON_DEFAULT_OPTIONS;
    options.pretty_print = 0;  // 壓縮格式，適合程序讀取
    
    char *json = sif_file_to_json(&sif_file, options);
    if (json) {
        // 只輸出純 JSON，沒有其他信息
        printf("%s\n", json);
        free(json);
    } else {
        fprintf(stderr, "Error: Failed to generate JSON\n");
    }
    
    sif_close(&sif_file);
    fclose(fp);
    return 0;
}