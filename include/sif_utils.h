#ifndef SIF_UTILS_H
#define SIF_UTILS_H

#include <stdio.h>
#include "sif_parser.h"

// 工具函數宣告
void trim_trailing_whitespace(char *str);
void debug_print_some_lines(FILE* fp, long debug_pos, int num_lines);

int32_t read_little_endian_int32(FILE *fp);
int32_t read_big_endian_int32(FILE *fp);
void print_sif_first_line(const char *filename);
void print_sif_first_lines(const char *filename, int line_count);
void print_sif_info_summary(const SifInfo *info);
void print_sif_file_structure(const SifFile *sif_file);
void print_hex_dump(FILE *fp, int offset, int length);

#endif