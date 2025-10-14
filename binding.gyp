{
  "targets": [
    {
      "target_name": "sifaddon",
      "sources": [
        "src/binding.cc",
        "src/sif_parser.c",
        "src/sif_json.c",
        "src/sif_utils.c"
      ],
      "include_dirs": [
        "include",
        "node_modules/node-addon-api"
      ],
      "cflags_cc": ["-std=c++17", "-fexceptions"],
      "cflags_c": ["-std=c99", "-DDEBUG"],
      "defines": [
        "NODE_ADDON_API_CPP_EXCEPTIONS"
      ],
      "libraries": ["-lm"]
    }
  ]
}