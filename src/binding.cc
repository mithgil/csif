#include <napi.h>
#include "sif_parser.h"
#include "sif_json.h"
#include "sif_utils.h"
#include <stdio.h>
#include <string.h>

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

Napi::Object InitAll(Napi::Env env, Napi::Object exports) {
    exports.Set("sifFileToJson", Napi::Function::New(env, SifFileToJsonWrapped));
    return exports;
}

NODE_API_MODULE(sifaddon, InitAll)