/**
 * GBS-Control ESP-IDF Port - Main Entry Point
 * Target: XIAO ESP32-C3 / ESP32-C6
 *
 * This file provides the ESP-IDF app_main() entry point which
 * calls the Arduino-style setup() and loop() functions from gbs_control.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

// Forward declarations from gbs_control component
extern void gbs_setup();
extern void gbs_loop();

// Shell (BLE serial console)
#include "shell.h"

// Geometry buttons (picture position control)
#include "geometry_buttons.h"

static const char *TAG = "gbs-main";

static void gbs_task(void *pvParameters)
{
    ESP_LOGI(TAG, "GBS-Control task starting...");
    gbs_setup();

    ESP_LOGI(TAG, "Entering main loop");
    while (true) {
        gbs_loop();
        // Small yield to feed the watchdog and allow other tasks
        vTaskDelay(1);
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "GBS-Control ESP-IDF Port");
    ESP_LOGI(TAG, "Target: XIAO ESP32-C3");

    // Initialize NVS (required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize BLE Shell (runs on separate task)
    shell_init();

    // Initialize Geometry Buttons (picture position control via GPIO)
    geometry_buttons_init();

    // Create the main GBS task with a large stack (main logic is stack-heavy)
    xTaskCreate(gbs_task, "gbs_task", 16384, NULL, 5, NULL);
}
