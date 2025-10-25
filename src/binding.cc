#include <napi.h>
#include "sif_parser.h"
#include "sif_json.h"
#include "sif_utils.h"
#include <stdio.h>
#include <string.h>
#include <vector>

Napi::Value SifFileToJsonWrapped(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "Expected a filename (string)").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::string filename = info[0].As<Napi::String>();
    
    FILE *fp = fopen(filename.c_str(), "rb");
    if (fp == NULL) {
        Napi::Error::New(env, "Cannot open file: " + filename).ThrowAsJavaScriptException();
        return env.Null();
    }

    SifFile sif_file;
    memset(&sif_file, 0, sizeof(SifFile));
    sif_file.file_ptr = fp;
    sif_file.data_loaded = 0;
    sif_file.frame_data = NULL;

    if (sif_open(fp, &sif_file) == 0) {
        if (sif_load_all_frames(&sif_file, 0) == 0) {
            sif_file.data_loaded = 1;
            
            JsonOutputOptions opts = JSON_FULL_DATA_OPTIONS;
            char* json_str = sif_file_to_json(&sif_file, opts);
            
            if (json_str) {
                Napi::String result = Napi::String::New(env, json_str);
                free(json_str);
                sif_close(&sif_file);
                fclose(fp);
                return result;
            }
        }
    }
    
    sif_close(&sif_file);
    fclose(fp);
    Napi::Error::New(env, "Failed to process SIF file").ThrowAsJavaScriptException();
    return env.Null();
}

// sif to binary buffer
Napi::Value SifFileToBinaryWrapped(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "Expected a filename (string)").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::string filename = info[0].As<Napi::String>();
    
    FILE *fp = fopen(filename.c_str(), "rb");
    if (fp == NULL) {
        Napi::Error::New(env, "Cannot open file: " + filename).ThrowAsJavaScriptException();
        return env.Null();
    }

    SifFile sif_file;
    memset(&sif_file, 0, sizeof(SifFile));
    sif_file.file_ptr = fp;
    sif_file.data_loaded = 0;
    sif_file.frame_data = NULL;

    if (sif_open(fp, &sif_file) != 0) {
        fclose(fp);
        Napi::Error::New(env, "Failed to open SIF file").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (sif_load_all_frames(&sif_file, 0) != 0) {
        sif_close(&sif_file);
        fclose(fp);
        Napi::Error::New(env, "Failed to load frame data").ThrowAsJavaScriptException();
        return env.Null();
    }

    sif_file.data_loaded = 1;
    
    // to obtain sif file info
    int width = sif_file.info.image_width;
    int height = sif_file.info.image_height;
    int total_frames = sif_file.info.number_of_frames;
    int pixels_per_frame = width * height;
    int total_data_points = total_frames * pixels_per_frame;
    
    printf("=== SIF Binary Export ===\n");
    printf("Dimensions: %dx%d, Frames: %d\n", width, height, total_frames);
    printf("Total data points: %d\n", total_data_points);
    
    // create ArrayBuffer
    size_t buffer_size = total_data_points * sizeof(double);
    Napi::ArrayBuffer array_buffer = Napi::ArrayBuffer::New(env, buffer_size);
    
    // get the pointer of the ArrayBuffer 
    double* buffer_data = static_cast<double*>(array_buffer.Data());
    
    // 將 float 數據轉換為 double 並複製到 buffer
    printf("Copying data from SIF frame_data to ArrayBuffer...\n");
    
    for (int i = 0; i < total_data_points; i++) {
        buffer_data[i] = static_cast<double>(sif_file.frame_data[i]);
    }
    
    // 創建 Float64Array
    Napi::TypedArray typed_array = Napi::TypedArrayOf<double>::New(env, 
        total_data_points, array_buffer, 0, napi_float64_array);
    
    printf("✓ Created Float64Array with %zu bytes\n", buffer_size);
    
    // 清理資源
    sif_close(&sif_file);
    fclose(fp);
    
    return typed_array;
}

Napi::Value SifFileToObjectWrapped(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "Expected a filename (string)").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::string filename = info[0].As<Napi::String>();
    
    FILE *fp = fopen(filename.c_str(), "rb");
    if (fp == NULL) {
        Napi::Error::New(env, "Cannot open file: " + filename).ThrowAsJavaScriptException();
        return env.Null();
    }

    SifFile sif_file;
    memset(&sif_file, 0, sizeof(SifFile));
    sif_file.file_ptr = fp;
    sif_file.data_loaded = 0;
    sif_file.frame_data = NULL;

    if (sif_open(fp, &sif_file) != 0) {
        fclose(fp);
        Napi::Error::New(env, "Failed to open SIF file").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (sif_load_all_frames(&sif_file, 0) != 0) {
        sif_close(&sif_file);
        fclose(fp);
        Napi::Error::New(env, "Failed to load frame data").ThrowAsJavaScriptException();
        return env.Null();
    }

    sif_file.data_loaded = 1;
    
    // 獲取文件信息
    int width = sif_file.info.image_width;
    int height = sif_file.info.image_height;
    int total_frames = sif_file.info.number_of_frames;
    int pixels_per_frame = width * height;
    int total_data_points = total_frames * pixels_per_frame;
    
    printf("=== SIF Object Export ===\n");
    printf("Dimensions: %dx%d, Frames: %d\n", width, height, total_frames);
    printf("Total data points: %d\n", total_data_points);
    
    // 創建返回對象
    Napi::Object result = Napi::Object::New(env);
    
    // 創建元數據對象
    Napi::Object metadata = Napi::Object::New(env);
    
    // 添加基本維度信息
    metadata.Set("width", Napi::Number::New(env, width));
    metadata.Set("height", Napi::Number::New(env, height));
    metadata.Set("numberOfFrames", Napi::Number::New(env, total_frames));
    metadata.Set("exposureTime", Napi::Number::New(env, sif_file.info.exposure_time));
    metadata.Set("detectorTemperature", Napi::Number::New(env, sif_file.info.detector_temperature));
    
    // 添加探測器尺寸
    Napi::Array detector_dims = Napi::Array::New(env, 2);
    detector_dims.Set((uint32_t)0, Napi::Number::New(env, sif_file.info.detector_width));
    detector_dims.Set((uint32_t)1, Napi::Number::New(env, sif_file.info.detector_height));
    metadata.Set("detectorDimensions", detector_dims);
    
    // 添加字符串字段（如果非空）
    if (sif_file.info.detector_type[0] != '\0') {
        metadata.Set("detectorType", Napi::String::New(env, sif_file.info.detector_type));
    }
    
    if (sif_file.info.original_filename[0] != '\0') {
        metadata.Set("originalFilename", Napi::String::New(env, sif_file.info.original_filename));
    }
    
    if (sif_file.info.spectrograph[0] != '\0') {
        metadata.Set("spectrograph", Napi::String::New(env, sif_file.info.spectrograph));
    }
    
    if (sif_file.info.data_type[0] != '\0') {
        metadata.Set("dataType", Napi::String::New(env, sif_file.info.data_type));
    }
    
    if (sif_file.info.frame_axis[0] != '\0') {
        metadata.Set("frameAxis", Napi::String::New(env, sif_file.info.frame_axis));
    }
    
    // 添加其他數值字段
    metadata.Set("sifVersion", Napi::Number::New(env, sif_file.info.sif_version));
    metadata.Set("sifCalbVersion", Napi::Number::New(env, sif_file.info.sif_calb_version));
    metadata.Set("experimentTime", Napi::Number::New(env, sif_file.info.experiment_time));
    metadata.Set("accumulatedCycles", Napi::Number::New(env, sif_file.info.accumulated_cycles));
    metadata.Set("numberOfSubimages", Napi::Number::New(env, sif_file.info.number_of_subimages));
    
    // 添加校準信息（如果可用）
    if (sif_file.info.calibration_coeff_count > 0) {
        Napi::Object calibration = Napi::Object::New(env);
        Napi::Array coefficients = Napi::Array::New(env, sif_file.info.calibration_coeff_count);
        
        for (int i = 0; i < sif_file.info.calibration_coeff_count; i++) {
            coefficients.Set((uint32_t)i, Napi::Number::New(env, sif_file.info.calibration_coefficients[i]));
        }
        
        calibration.Set("coefficients", coefficients);
        calibration.Set("frameAxis", Napi::String::New(env, sif_file.info.frame_axis));
        metadata.Set("calibration", calibration);
    }
    
    // 創建二進制數據 - 使用 Float32Array
    size_t buffer_size = total_data_points * sizeof(float);
    Napi::ArrayBuffer array_buffer = Napi::ArrayBuffer::New(env, buffer_size);
    float* buffer_data = static_cast<float*>(array_buffer.Data());

    // 直接複製 float 數據（無需轉換）
    memcpy(buffer_data, sif_file.frame_data, buffer_size);

    // 創建 Float32Array
    Napi::TypedArray binary_data = Napi::TypedArrayOf<float>::New(env, 
        total_data_points, array_buffer, 0, napi_float32_array);
    
    printf("✓ Created Float32Array with %zu bytes\n", buffer_size);
    
    // 關鍵修復：把 binary_data 設置到返回對象中！
    result.Set("metadata", metadata);
    result.Set("binaryData", binary_data); 

    // 清理資源
    sif_close(&sif_file);
    fclose(fp);
    
    return result;  // 返回包含 metadata 和 binaryData 的對象
}

// Float32Array 版本（內存減半）
Napi::Value SifFileToFloat32Wrapped(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "Expected a filename (string)").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::string filename = info[0].As<Napi::String>();
    
    FILE *fp = fopen(filename.c_str(), "rb");
    if (fp == NULL) {
        Napi::Error::New(env, "Cannot open file: " + filename).ThrowAsJavaScriptException();
        return env.Null();
    }

    SifFile sif_file;
    memset(&sif_file, 0, sizeof(SifFile));
    sif_file.file_ptr = fp;
    sif_file.data_loaded = 0;
    sif_file.frame_data = NULL;

    if (sif_open(fp, &sif_file) != 0) {
        fclose(fp);
        Napi::Error::New(env, "Failed to open SIF file").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (sif_load_all_frames(&sif_file, 0) != 0) {
        sif_close(&sif_file);
        fclose(fp);
        Napi::Error::New(env, "Failed to load frame data").ThrowAsJavaScriptException();
        return env.Null();
    }

    sif_file.data_loaded = 1;
    
    int width = sif_file.info.image_width;
    int height = sif_file.info.image_height;
    int total_frames = sif_file.info.number_of_frames;
    int total_data_points = total_frames * width * height;
    
    printf("=== SIF Float32 Export ===\n");
    printf("Creating Float32Array with %d elements\n", total_data_points);
    
    // 創建 Float32Array（內存減半）
    size_t buffer_size = total_data_points * sizeof(float);
    Napi::ArrayBuffer array_buffer = Napi::ArrayBuffer::New(env, buffer_size); //在 V8 堆中分配一塊 10.24 MB (2500 frames) 的原始二進制內存
    float* buffer_data = static_cast<float*>(array_buffer.Data()); 
    
    // 直接複製 float 數據（無需轉換）
    memcpy(buffer_data, sif_file.frame_data, buffer_size);
    // 創建TypedArray 視圖，讓 JavaScript 能夠以正確的類型來讀取 ArrayBuffer 中的數據
    Napi::TypedArray typed_array = Napi::TypedArrayOf<float>::New(env, 
        total_data_points, array_buffer, 0, napi_float32_array);
    
    printf("✓ Created Float32Array with %zu bytes\n", buffer_size);
    
    sif_close(&sif_file);
    fclose(fp);
    
    return typed_array;
}

Napi::Object InitAll(Napi::Env env, Napi::Object exports) {
    // 原有 JSON 方法
    exports.Set("sifFileToJson", Napi::Function::New(env, SifFileToJsonWrapped));
    
    // 新的二進制方法
    exports.Set("sifFileToBinary", Napi::Function::New(env, SifFileToBinaryWrapped));
    exports.Set("sifFileToObject", Napi::Function::New(env, SifFileToObjectWrapped));
    exports.Set("sifFileToFloat32", Napi::Function::New(env, SifFileToFloat32Wrapped));
    
    return exports;
}

NODE_API_MODULE(sifaddon, InitAll)