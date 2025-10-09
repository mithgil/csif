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
    
    if (sif_open(fp, &sif_file) != 0) {
        fprintf(stderr, "Error: Failed to parse SIF file\n");
        fclose(fp);
        return 1;
    }
    
    // 加載第一幀數據
    if (sif_load_single_frame(&sif_file, 0) != 0) {
        fprintf(stderr, "Warning: Could not load frame data\n");
    }
    
    // 輸出 JSON
    JsonOutputOptions options = JSON_DEFAULT_OPTIONS;
    options.pretty_print = 0;  // 壓縮格式，適合程序讀取
    
    char *json = sif_file_to_json(&sif_file, options);
    if (json) {
        printf("%s\n", json);
        free(json);
    } else {
        fprintf(stderr, "Error: Failed to generate JSON\n");
    }
    
    sif_close(&sif_file);
    fclose(fp);
    return 0;
}