cmake_minimum_required(VERSION 3.10)
project(SIF_Parser VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

# 設置輸出目錄
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 創建庫目標
add_library(sif_parser_obj OBJECT 
    src/sif_parser.c 
    src/sif_utils.c
    src/sif_json.c
)

set_target_properties(sif_parser_obj PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
)

# 設置包含目錄
target_include_directories(sif_parser_obj PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 可執行文件 - 使用對象庫
add_executable(read_sif src/main.c)
target_link_libraries(read_sif PRIVATE sif_parser_obj m)  # 這裡也要加 m

# 或者更好的方式：也使用對象庫
add_executable(debug_sif src/debug_sif.c)
target_link_libraries(debug_sif PRIVATE sif_parser_obj m)

add_executable(debug_detail_sif src/debug_detail.c)
target_link_libraries(debug_detail_sif PRIVATE sif_parser_obj m)

#add_executable(sif_json src/sif_cli_json.c)
#target_link_libraries(sif_json PRIVATE sif_parser_obj m)

# 共享庫
add_library(sif_parser_shared SHARED $<TARGET_OBJECTS:sif_parser_obj>)
target_link_libraries(sif_parser_shared PUBLIC m) 
set_target_properties(sif_parser_shared PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    OUTPUT_NAME "sifparser"
)

# 靜態庫
add_library(sif_parser_static STATIC $<TARGET_OBJECTS:sif_parser_obj>)
set_target_properties(sif_parser_static PROPERTIES
    OUTPUT_NAME "sifparser"
)

# 別名目標
add_library(sif_parser INTERFACE)
target_link_libraries(sif_parser INTERFACE 
    $<IF:$<BOOL:${BUILD_SHARED_LIBS}>,sif_parser_shared,sif_parser_static>
)

# 安裝規則
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    install(TARGETS read_sif debug_sif debug_detail_sif
        RUNTIME DESTINATION bin
    )

    install(TARGETS sif_parser_shared sif_parser_static
        EXPORT SIFParserTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )

    install(FILES 
        include/sif_parser.h
        include/sif_utils.h
        include/sif_json.h
        DESTINATION include
    )

    install(EXPORT SIFParserTargets
        FILE SIFParserTargets.cmake
        NAMESPACE SIFParser::
        DESTINATION lib/cmake/SIFParser
    )
endif()