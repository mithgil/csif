// sif_json_cli.c
#include "sif_parser.h"
#include "sif_json.h"
#include <stdio.h>
#include <stdlib.h>
// sif_cli_json.c
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <sif_file> [frame_number]\n", argv[0]);
        return 1;
    }
    
    int requested_frame = -1; // -1 表示所有幀
    if (argc == 3) {
        requested_frame = atoi(argv[2]);
    }
    
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file %s\n", argv[1]);
        return 1;
    }
    
    SifFile sif_file = {0};
    sif_set_verbose_level(SIF_SILENT);
    
    if (sif_open(fp, &sif_file) != 0) {
        fprintf(stderr, "Error: Failed to parse SIF file\n");
        fclose(fp);
        return 1;
    }
    
    JsonOutputOptions options = JSON_DEFAULT_OPTIONS;
    
    if (requested_frame >= 0) {
        // 加載單一幀
        //printf("Debug: Loading single frame %d\n", requested_frame);
        options.include_all_frames = 0;
        options.max_frames = 1;
        
        // 使用正確的函數名和參數
        if (sif_load_single_frame(&sif_file, requested_frame) != 0) {
            fprintf(stderr, "Error: Failed to load frame %d\n", requested_frame);
            sif_close(&sif_file);
            fclose(fp);
            return 1;
        }
    } else {
        // 加載所有幀
        printf("Debug: Loading all %d frames\n", sif_file.info.number_of_frames);
        options.include_all_frames = 1;
        
        if (sif_load_all_frames(&sif_file, 0) != 0) {
            fprintf(stderr, "Warning: Could not load all frame data\n");
        }
    }
    
    char *json = sif_file_to_json(&sif_file, options);
    if (json) {
        printf("%s", json);
        fflush(stdout);
        free(json);
    } else {
        fprintf(stderr, "Error: Failed to generate JSON\n");
    }
    
    sif_close(&sif_file);
    fclose(fp);
    return 0;
}