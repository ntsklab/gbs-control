# GBS-Control ESP-IDF Port
# Target: XIAO ESP32-C3 (RISC-V)
# Ported from: gbs-control (ESP8266 Arduino)
cmake_minimum_required(VERSION 3.16)

# Set default target to ESP32-C3
set(IDF_TARGET "esp32c3" CACHE STRING "IDF Build Target")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(gbs-control)
