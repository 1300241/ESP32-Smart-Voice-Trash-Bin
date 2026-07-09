# ESP32 智能语音垃圾分类垃圾桶
# 顶层 CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# ESP-IDF 项目必须将项目名传递给 idf_build_process
set(PROJECT_NAME "smart_trash_bin")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(${PROJECT_NAME})
