# GBS-Control ESP-IDF Port
# Supported targets: XIAO ESP32-C3 / ESP32-C6 (RISC-V)
# Ported from: gbs-control (ESP8266 Arduino)
#
# ターゲットは idf.py set-target で切り替えてください:
#   idf.py set-target esp32c3
#   idf.py set-target esp32c6
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(gbs-control)
