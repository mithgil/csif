#ifndef SIF_UTILS_H
#define SIF_UTILS_H

#include <stdio.h>
#include "sif_parser.h"

// 工具函數宣告
void trim_trailing_whitespace(char *str);
void debug_print_some_lines(FILE* fp, long debug_pos, int num_lines);
void debug_hex_dump(FILE* fp, long debug_pos, int num_bytes_to_dump);
void debug_comprehensive(FILE* fp, long debug_pos, int num_lines, int hex_dump_bytes);

int32_t read_little_endian_int32(FILE *fp);
int32_t read_big_endian_int32(FILE *fp);

// 這些函數需要添加 SifInfo 參數來獲取輸出級別
void print_sif_first_line(const char *filename, SifInfo *info);
void print_sif_info_summary(const SifInfo *info);
void print_sif_file_structure(const SifFile *sif_file);
void print_hex_dump(FILE *fp, int target_offset, int before_bytes, int after_bytes);
double* retrieve_calibration(SifInfo *info, int* calibration_size);

// 新的工具函數（可選）
void sif_utils_set_verbose_level(SifVerboseLevel level);  // 如果工具函數需要獨立級別

#endif