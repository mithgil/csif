#include "sif_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// unified option
const JsonOutputOptions JSON_DEFAULT_OPTIONS = {
    .include_raw_data = 1,
    .include_calibration = 1,
    .include_metadata = 1,
    .pretty_print = 0,
    .max_data_points = 0,
    .include_all_frames = 1,    // includes all frame
    .max_frames = 0         
};

const JsonOutputOptions JSON_METADATA_ONLY_OPTIONS = {
    .include_raw_data = 0,
    .include_calibration = 1,
    .include_metadata = 1,
    .pretty_print = 0,
    .max_data_points = 0,
    .include_all_frames = 0,    // exclues all frame
    .max_frames = 0
};

const JsonOutputOptions JSON_FULL_DATA_OPTIONS = {
    .include_raw_data = 1,
    .include_calibration = 1,
    .include_metadata = 1,
    .pretty_print = 1,
    .max_data_points = 0,
    .include_all_frames = 1,    // includes all frame
    .max_frames = 0
};

// JSON buffer
typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} JsonBuffer;

static char* json_escape_string(const char *input);
static void json_buffer_init(JsonBuffer *buffer);
static void json_buffer_append(JsonBuffer *buffer, const char *format, ...);
static void json_buffer_free(JsonBuffer *buffer);


static void json_buffer_init(JsonBuffer *buffer) {
    buffer->capacity = 4096;
    buffer->data = malloc(buffer->capacity);
    buffer->length = 0;
    if (buffer->data) {
        buffer->data[0] = '\0';
    }
}

static void json_buffer_append(JsonBuffer *buffer, const char *format, ...) {
    va_list args;
    char temp[1024];
    
    va_start(args, format);
    int needed = vsnprintf(temp, sizeof(temp), format, args);
    va_end(args);
    
    if (needed < 0) return;
    
    size_t new_length = buffer->length + needed + 1;
    if (new_length > buffer->capacity) {
        while (new_length > buffer->capacity) {
            buffer->capacity *= 2;
        }
        char *new_data = realloc(buffer->data, buffer->capacity);
        if (!new_data) return;
        buffer->data = new_data;
    }
    
    strcpy(buffer->data + buffer->length, temp);
    buffer->length += needed;
}

static void json_buffer_free(JsonBuffer *buffer) {
    if (buffer->data) {
        free(buffer->data);
        buffer->data = NULL;
    }
    buffer->length = 0;
    buffer->capacity = 0;
}

// 添加字符串轉義函數
static char* json_escape_string(const char *input) {
    if (!input) return NULL;
    
    // calculate the size for buffer
    size_t len = strlen(input);
    size_t escaped_len = len + 1; // +1 for null terminator
    
    for (size_t i = 0; i < len; i++) {
        if (input[i] == '\\' || input[i] == '\"' || input[i] == '\n' || input[i] == '\r' || input[i] == '\t') {
            escaped_len++; // char for escaping
        }
    }
    
    char *escaped = malloc(escaped_len);
    if (!escaped) return NULL;
    
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        switch (input[i]) {
            case '\\':
                escaped[j++] = '\\';
                escaped[j++] = '\\';
                break;
            case '\"':
                escaped[j++] = '\\';
                escaped[j++] = '\"';
                break;
            case '\n':
                escaped[j++] = '\\';
                escaped[j++] = 'n';
                break;
            case '\r':
                escaped[j++] = '\\';
                escaped[j++] = 'r';
                break;
            case '\t':
                escaped[j++] = '\\';
                escaped[j++] = 't';
                break;
            default:
                escaped[j++] = input[i];
        }
    }
    escaped[j] = '\0';
    
    return escaped;
}

// main json output
char* sif_file_to_json(SifFile *sif_file, JsonOutputOptions options) {
    printf("=== ENTERING sif_file_to_json ===\n");
    
    if (!sif_file) {
        printf("❌ sif_file is NULL\n");
        return NULL;
    }
    
    printf("  sif_file pointer: %p\n", sif_file);
    printf("  data_loaded: %d\n", sif_file->data_loaded);
    printf("  frame_data: %p\n", sif_file->frame_data);
    printf("  frame_count: %d\n", sif_file->frame_count);
    printf("  tiles: %p\n", sif_file->tiles);
    
    if (sif_file->tiles) {
        printf("  tile[0]: width=%d, height=%d\n", 
               sif_file->tiles[0].width, sif_file->tiles[0].height);
    }
    
    JsonBuffer buffer;
    printf("→ Initializing JSON buffer...\n");
    json_buffer_init(&buffer);
    
    printf("→ Starting JSON generation...\n");
    
    //Begin JSON object
    json_buffer_append(&buffer, "{");
    
    if (options.pretty_print) {
        json_buffer_append(&buffer, "\n  ");
    }
    
    // metadata
    if (options.include_metadata) {
        printf("→ Generating metadata...\n");

        json_buffer_append(&buffer, "\"metadata\": {");
        if (options.pretty_print) json_buffer_append(&buffer, "\n    ");
        
        json_buffer_append(&buffer, "\"detectorDimensions\": [%d, %d],", 
                        sif_file->info.detector_width, sif_file->info.detector_height);
        if (options.pretty_print) json_buffer_append(&buffer, "\n    ");
        
        json_buffer_append(&buffer, "\"numberOfFrames\": %d,", sif_file->info.number_of_frames);
        if (options.pretty_print) json_buffer_append(&buffer, "\n    ");
        
        json_buffer_append(&buffer, "\"exposureTime\": %.6f,", sif_file->info.exposure_time);
        if (options.pretty_print) json_buffer_append(&buffer, "\n    ");
        
        json_buffer_append(&buffer, "\"detectorTemperature\": %.2f,", sif_file->info.detector_temperature);
        if (options.pretty_print) json_buffer_append(&buffer, "\n    ");
        
        // escape camera name
        char *escaped_camera = json_escape_string(sif_file->info.detector_type);
        json_buffer_append(&buffer, "\"cameraModel\": \"%s\",", escaped_camera ? escaped_camera : "");
        if (escaped_camera) free(escaped_camera);
        if (options.pretty_print) json_buffer_append(&buffer, "\n    ");
        
        // escape the raw filename
        char *escaped_filename = json_escape_string(sif_file->info.original_filename);
        json_buffer_append(&buffer, "\"originalFilename\": \"%s\",", escaped_filename ? escaped_filename : "");
        if (escaped_filename) free(escaped_filename);
        if (options.pretty_print) json_buffer_append(&buffer, "\n    ");
        
        // Escape datatype
        char *escaped_datatype = json_escape_string(sif_file->info.data_type);
        json_buffer_append(&buffer, "\"dataType\": \"%s\"", escaped_datatype ? escaped_datatype : "");
        if (escaped_datatype) free(escaped_datatype);
        
        if (options.pretty_print) json_buffer_append(&buffer, "\n  ");
        json_buffer_append(&buffer, "},");
        if (options.pretty_print) json_buffer_append(&buffer, "\n  ");
    }

    // calibration
    if (options.include_calibration && sif_file->info.calibration_coeff_count > 0) {
        printf("→ Generating calibration...\n");
        json_buffer_append(&buffer, "\"calibration\": {");
        if (options.pretty_print) json_buffer_append(&buffer, "\n    ");
        
        json_buffer_append(&buffer, "\"coefficients\": [");
        for (int i = 0; i < sif_file->info.calibration_coeff_count; i++) {
            json_buffer_append(&buffer, "%.10f", sif_file->info.calibration_coefficients[i]);
            if (i < sif_file->info.calibration_coeff_count - 1) {
                json_buffer_append(&buffer, ", ");
            }
        }
        json_buffer_append(&buffer, "],");
        if (options.pretty_print) json_buffer_append(&buffer, "\n    ");
        
        // escape frameAxis
        char *escaped_frameaxis = json_escape_string(sif_file->info.frame_axis);
        json_buffer_append(&buffer, "\"frameAxis\": \"%s\"", escaped_frameaxis ? escaped_frameaxis : "");
        if (escaped_frameaxis) free(escaped_frameaxis);
        
        if (options.pretty_print) json_buffer_append(&buffer, "\n  ");
        json_buffer_append(&buffer, "},");
        if (options.pretty_print) json_buffer_append(&buffer, "\n  ");
    }
        
    // image data
    json_buffer_append(&buffer, "\"dimensions\": {");
    if (options.pretty_print) json_buffer_append(&buffer, "\n    ");
    json_buffer_append(&buffer, "\"width\": %d,", sif_file->info.image_width);
    if (options.pretty_print) json_buffer_append(&buffer, "\n    ");
    json_buffer_append(&buffer, "\"height\": %d", sif_file->info.image_height);
    if (options.pretty_print) json_buffer_append(&buffer, "\n  ");
    json_buffer_append(&buffer, "},");
    if (options.pretty_print) json_buffer_append(&buffer, "\n  ");
    
    // raw data
    printf("→ Generating data array...\n");
    printf("  include_raw_data: %d\n", options.include_raw_data);
    printf("  frame_data exists: %d\n", sif_file->frame_data != NULL);
    printf("  data_loaded: %d\n", sif_file->data_loaded);

    if (options.include_raw_data && sif_file->frame_data && sif_file->data_loaded) {
        printf("✓ Outputting real data\n");
        
        int frame_size = sif_file->info.image_width * sif_file->info.image_height;
        int total_frames = sif_file->info.number_of_frames;
        int total_data_points = total_frames * frame_size;
        
        printf("  Frame size: %d x %d = %d pixels\n", 
            sif_file->info.image_width, sif_file->info.image_height, frame_size);
        printf("  Total frames: %d, Total data points: %d\n", total_frames, total_data_points);
        
        float *frame0 = sif_file->frame_data; // the beginning position of the frist frame
        printf("  Frame 0 pointer: %p\n", frame0);
        
        // display the first 10 values
        printf("  First 10 values from frame_data:\n");
        for (int i = 0; i < 10 && i < frame_size; i++) {
            printf("    [%d] = %.1f\n", i, frame0[i]);
        }
        
        json_buffer_append(&buffer, "\"data\": [", 9);
        
        // output data
        int output_points = frame_size;
        
        for (int i = 0; i < output_points; i++) {
            float value = frame0[i];
            
            // use char to construct
            char num_str[32];
            if (value == (int)value) {
                snprintf(num_str, sizeof(num_str), "%d", (int)value);
            } else {
                snprintf(num_str, sizeof(num_str), "%.1f", value);
            }
            json_buffer_append(&buffer, num_str, strlen(num_str));
            
            if (i < output_points - 1) {
                json_buffer_append(&buffer, ", ", 2);
            }
        }
        
        json_buffer_append(&buffer, "]", 1);
        printf("✓ Output %d data points\n", output_points);
        
    } else {
        printf("⚠️ Outputting empty data array\n");
        printf("  Reason: include_raw_data=%d, frame_data=%p, data_loaded=%d\n",
            options.include_raw_data, sif_file->frame_data, sif_file->data_loaded);
        json_buffer_append(&buffer, "\"data\": []", 10);
    }
        
    if (options.pretty_print) {
        json_buffer_append(&buffer, "\n");
    }
    json_buffer_append(&buffer, "}");

    printf("✓ JSON generation completed\n");
    printf("  Buffer size: %zu\n", buffer.length);
    printf("=== EXITING sif_file_to_json ===\n");
    
    return buffer.data;
}

char* sif_file_to_json_simple(SifFile *sif_file) {
    return sif_file_to_json(sif_file, JSON_DEFAULT_OPTIONS);
}

char* sif_file_metadata_to_json(SifFile *sif_file) {
    return sif_file_to_json(sif_file, JSON_METADATA_ONLY_OPTIONS);
}

int sif_save_as_json(SifFile *sif_file, const char *filename, JsonOutputOptions options) {
    char *json = sif_file_to_json(sif_file, options);
    if (!json) return 0;
    
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        free(json);
        return 0;
    }
    
    fputs(json, fp);
    fclose(fp);
    free(json);
    return 1;
}