
#include <stdio.h>
#include "sif_parser.h"
#include "sif_utils.h"

//  ./bin/read_sif /home/tim/Documents/AS/data/andor/20250917/1OD_500uW_sapphire_200umFiber_.sif
//  ./bin/read_sif /home/tim/Documents/AS/data/andor/20250908/monochrom_430_700_10_LED_2.sif

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <sif_file>\n", argv[0]);
        return 1;
    }
    
    const char *filename = argv[1];
    
    printf("=== SIF File Analysis Tool ===\n\n");
    
    // 1. 打印第一行
    printf("1. First Line Analysis:\n");
    print_sif_first_line(filename);
    printf("\n");
       
    // 2. 完整解析並顯示資訊
    printf("3. Complete File Analysis:\n");
    FILE *fp = fopen(filename, "rb");
    if (fp) {
        SifFile sif_file;
        if (sif_open(fp, &sif_file) == 0) {
            printf("\n");
            print_sif_info_summary(&sif_file.info);
            printf("\n");
            print_sif_file_structure(&sif_file);
            printf("\n");
            
            // 3. 顯示數據區域的十六進制轉儲
            //printf("3. Data Region Hex Dump (first 256 bytes):\n");
            //print_hex_dump(fp, sif_file.info.data_offset, 256);
            
            sif_close(&sif_file);
        } else {
            printf("Error: Failed to parse SIF file\n");
        }
        fclose(fp);
    } else {
        printf("Error: Cannot open file %s\n", filename);
    }
    
    return 0;
}

// 簡單的單獨工具程式
void print_first_line_only(const char *filename) {
    print_sif_first_line(filename);
}

void print_first_seven_lines(const char *filename) {
    print_sif_first_lines(filename, 7);
}
