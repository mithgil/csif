#ifndef SIF_UTILS_H
#define SIF_UTILS_H

#include <stdio.h>
#include "sif_parser.h"

// tool functions declaration
void trim_trailing_whitespace(char *str);
void debug_print_some_lines(FILE* fp, long debug_pos, int num_lines);
void debug_hex_dump(FILE* fp, long debug_pos, int num_bytes_to_dump);
void debug_comprehensive(FILE* fp, long debug_pos, int num_lines, int hex_dump_bytes);

int32_t read_little_endian_int32(FILE *fp);
int32_t read_big_endian_int32(FILE *fp);

// These functions need the SifInfo parameter to get the output level.
void print_sif_first_line(const char *filename, SifInfo *info);
void print_sif_info_summary(const SifInfo *info);
void print_sif_file_structure(const SifFile *sif_file);
void print_hex_dump(FILE *fp, int target_offset, int before_bytes, int after_bytes);
double* retrieve_calibration(SifInfo *info, int* calibration_size);

void sif_utils_set_verbose_level(SifVerboseLevel level); 

#endif