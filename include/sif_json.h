#ifndef SIF_JSON_H
#define SIF_JSON_H

#include "sif_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

// JSON 輸出選項
typedef struct {
    int include_raw_data;      // 是否包含原始像素數據
    int include_calibration;   // 是否包含校準信息
    int include_metadata;      // 是否包含元數據
    int pretty_print;          // 是否美化輸出（換行和縮進）
    int max_data_points;       // 最大數據點數（0表示全部）
    int include_all_frames;    // 是否包含所有幀數據
    int max_frames;            // 最大輸出幀數
} JsonOutputOptions;

// 默認選項
extern const JsonOutputOptions JSON_DEFAULT_OPTIONS;
extern const JsonOutputOptions JSON_METADATA_ONLY_OPTIONS;
extern const JsonOutputOptions JSON_FULL_DATA_OPTIONS;

// 主要的 JSON 輸出函數
char* sif_file_to_json(SifFile *sif_file, JsonOutputOptions options);
char* sif_info_to_json(SifInfo *info);
char* sif_frame_data_to_json(SifFile *sif_file, int frame_index, JsonOutputOptions options);

// 文件輸出函數
int sif_save_as_json(SifFile *sif_file, const char *filename, JsonOutputOptions options);

// 便利函數
char* sif_file_to_json_simple(SifFile *sif_file);  // 使用默認選項
char* sif_file_metadata_to_json(SifFile *sif_file); // 僅元數據

#ifdef __cplusplus
}
#endif

#endif