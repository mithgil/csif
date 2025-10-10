// sif_json_cli.c
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
    
    // 設置為靜默模式
    sif_set_verbose_level(SIF_SILENT);
    
    if (sif_open(fp, &sif_file) != 0) {
        fprintf(stderr, "Error: Failed to parse SIF file\n");
        fclose(fp);
        return 1;
    }
    
    // 加載所有幀數據
    if (sif_load_all_frames(&sif_file, 0) != 0) {
        // 即使加載失敗，也輸出 JSON（但數據為空）
        // 不輸出警告到 stdout
    }
    
    // 輸出 JSON - 不包含任何調試信息
    JsonOutputOptions options = JSON_DEFAULT_OPTIONS;
    options.include_all_frames = 1;  // 包含所有幀
    options.max_frames = 10000;      // 設置足夠大的值
    
    char *json = sif_file_to_json(&sif_file, options);
    if (json) {
        // 只輸出純 JSON，沒有任何其他輸出
        printf("%s\n", json);
        fflush(stdout);  // 確保輸出
        free(json);
    } else {
        // 錯誤信息輸出到 stderr，不污染 stdout
        fprintf(stderr, "Error: Failed to generate JSON\n");
    }
    
    sif_close(&sif_file);
    fclose(fp);
    return 0;
}