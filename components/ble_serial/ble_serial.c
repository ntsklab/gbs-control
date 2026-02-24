/**
 * @file ble_serial.c
 * @brief BLE UART-like transport (NimBLE).
 *
 * Provides a Nordic UART Service (NUS) compatible BLE GATT service
 * for serial console access. Used by the shell component.
 */

#include "ble_serial.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if CONFIG_BT_ENABLED && CONFIG_BT_NIMBLE_ENABLED

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "store/config/ble_store_config.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "ble_serial";

#define BLE_SERIAL_DEV_NAME_BASE "GBS-Control"
#define BLE_SERIAL_LINE_BUF_SIZE 128
#define BLE_SERIAL_TX_CHUNK_MAX  180

extern void ble_store_config_init(void);

static ble_serial_line_cb_t s_line_cb = NULL;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_tx_val_handle = 0;
static uint8_t  s_own_addr_type = BLE_OWN_ADDR_PUBLIC;
static char     s_dev_name[32] = BLE_SERIAL_DEV_NAME_BASE;
static bool     s_ble_initialized = false;

static char    s_line_buf[BLE_SERIAL_LINE_BUF_SIZE];
static size_t  s_line_len = 0;
static uint8_t s_rx_tmp[BLE_SERIAL_LINE_BUF_SIZE];
static char    s_ctrl_msg[BLE_SERIAL_LINE_BUF_SIZE + 3];
static uint8_t s_esc_state = 0;

/* Nordic UART Service UUIDs */
static const ble_uuid128_t s_uuid_service =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);
static const ble_uuid128_t s_uuid_rx =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);
static const ble_uuid128_t s_uuid_tx =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

static void ble_serial_advertise(void);

static void ble_serial_build_device_name(void)
{
    uint8_t mac[6] = {0};
    esp_err_t err = esp_efuse_mac_get_default(mac);
    if (err != ESP_OK) {
        snprintf(s_dev_name, sizeof(s_dev_name), "%s", BLE_SERIAL_DEV_NAME_BASE);
        return;
    }
    snprintf(s_dev_name, sizeof(s_dev_name), "%s-%02X%02X%02X",
             BLE_SERIAL_DEV_NAME_BASE, mac[3], mac[4], mac[5]);
}

static void ble_serial_handle_rx_bytes(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uint8_t c = data[i];

        /* ESC sequence handling */
        if (s_esc_state == 1) {
            if (c == '[') {
                s_esc_state = 2;
                continue;
            }
            s_esc_state = 0;
        } else if (s_esc_state == 2) {
            s_esc_state = 0;
            if (c == 'A' || c == 'B' || c == 'C' || c == 'D') {
                if (s_line_cb) {
                    s_ctrl_msg[0] = BLE_SERIAL_CTRL_PREFIX;
                    s_ctrl_msg[1] = 'K';
                    s_ctrl_msg[2] = (char)c;
                    s_ctrl_msg[3] = '\0';
                    s_line_cb(s_ctrl_msg);
                }
                continue;
            }
        }

        if (c == 0x1B) {
            s_esc_state = 1;
            continue;
        }

        /* Ctrl-C */
        if (c == 0x03) {
            if (s_line_cb) {
                s_ctrl_msg[0] = BLE_SERIAL_CTRL_PREFIX;
                s_ctrl_msg[1] = 0x03;
                s_ctrl_msg[2] = '\0';
                s_line_cb(s_ctrl_msg);
            }
            continue;
        }

        /* Tab and ? : send as control with current line buffer */
        if (c == '?' || c == '\t') {
            if (s_line_cb) {
                s_ctrl_msg[0] = BLE_SERIAL_CTRL_PREFIX;
                s_ctrl_msg[1] = (char)c;
                memcpy(&s_ctrl_msg[2], s_line_buf, s_line_len);
                s_ctrl_msg[s_line_len + 2] = '\0';
                s_line_cb(s_ctrl_msg);
            }
            continue;
        }

        /* Enter */
        if (c == '\r' || c == '\n') {
            if (s_line_len > 0) {
                s_line_buf[s_line_len] = '\0';
                if (s_line_cb) {
                    s_line_cb(s_line_buf);
                }
                s_line_len = 0;
            } else if (s_line_cb) {
                s_ctrl_msg[0] = BLE_SERIAL_CTRL_PREFIX;
                s_ctrl_msg[1] = '\r';
                s_ctrl_msg[2] = '\0';
                s_line_cb(s_ctrl_msg);
            }
            continue;
        }

        /* Backspace */
        if (c == '\b' || c == 0x7f) {
            if (s_line_len > 0) {
                s_line_len--;
                ble_serial_send("\b \b", 3);
            }
            continue;
        }

        /* Printable character */
        if (c >= 0x20 && c <= 0x7e) {
            if (s_line_len < (BLE_SERIAL_LINE_BUF_SIZE - 1)) {
                s_line_buf[s_line_len++] = (char)c;
                char ch = (char)c;
                ble_serial_send(&ch, 1);
            }
        }
    }
}

static int ble_gatt_svr_chr_access(uint16_t conn_handle,
                                   uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt,
                                   void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t copied = 0;
        int rc = ble_hs_mbuf_to_flat(ctxt->om, s_rx_tmp, sizeof(s_rx_tmp), &copied);
        if (rc == 0 && copied > 0) {
            ble_serial_handle_rx_bytes(s_rx_tmp, copied);
        }
        return 0;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        return 0;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_uuid_service.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &s_uuid_rx.u,
                .access_cb = ble_gatt_svr_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid = &s_uuid_tx.u,
                .access_cb = ble_gatt_svr_chr_access,
                .val_handle = &s_tx_val_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            {0}
        },
    },
    {0},
};

static int ble_gap_event_cb(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "BLE connected (handle=%u)", s_conn_handle);
            static const char *banner = "\r\nGBS BLE Shell connected\r\ngbs> ";
            ble_serial_send(banner, strlen(banner));
        } else {
            ESP_LOGW(TAG, "BLE connect failed: %d", event->connect.status);
            ble_serial_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE disconnected (reason=%d)", event->disconnect.reason);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        ble_serial_advertise();
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ble_serial_advertise();
        return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "BLE MTU updated: %u", event->mtu.value);
        return 0;

    default:
        return 0;
    }
}

static void ble_serial_advertise(void)
{
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)s_dev_name;
    fields.name_len = strlen(s_dev_name);
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields failed: %d", rc);
        return;
    }

    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_gap_event_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start failed: %d", rc);
    } else {
        ESP_LOGI(TAG, "BLE advertising as '%s'", s_dev_name);
    }
}

static void ble_on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_util_ensure_addr failed: %d", rc);
        return;
    }

    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_id_infer_auto failed: %d", rc);
        return;
    }

    uint8_t addr_val[6] = {0};
    ble_hs_id_copy_addr(s_own_addr_type, addr_val, NULL);
    ESP_LOGI(TAG, "BLE address: %02x:%02x:%02x:%02x:%02x:%02x",
             addr_val[5], addr_val[4], addr_val[3],
             addr_val[2], addr_val[1], addr_val[0]);

    ble_serial_advertise();
}

static void ble_on_reset(int reason)
{
    ESP_LOGW(TAG, "BLE host reset: reason=%d", reason);
}

static void nimble_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

#endif /* CONFIG_BT_ENABLED && CONFIG_BT_NIMBLE_ENABLED */

void ble_serial_init(ble_serial_line_cb_t line_cb)
{
#if !CONFIG_BT_ENABLED || !CONFIG_BT_NIMBLE_ENABLED
    (void)line_cb;
    ESP_LOGW(TAG, "BLE disabled in sdkconfig (enable CONFIG_BT_ENABLED + CONFIG_BT_NIMBLE_ENABLED)");
    return;
#else
    s_line_cb = line_cb;

    esp_log_level_set("NimBLE", ESP_LOG_WARN);
    esp_log_level_set("BLE_INIT", ESP_LOG_WARN);

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %d (BLE disabled)", err);
        return;
    }

    ble_hs_cfg.reset_cb = ble_on_reset;
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_serial_build_device_name();

    int rc = ble_svc_gap_device_name_set(s_dev_name);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_svc_gap_device_name_set failed: %d", rc);
        return;
    }

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
        return;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
        return;
    }

    ble_store_config_init();
    nimble_port_freertos_init(nimble_host_task);
    s_ble_initialized = true;

    ESP_LOGI(TAG, "BLE serial initialized (name=%s)", s_dev_name);
#endif
}

void ble_serial_send(const char *data, size_t len)
{
#if !CONFIG_BT_ENABLED || !CONFIG_BT_NIMBLE_ENABLED
    (void)data;
    (void)len;
    return;
#else
    if (!s_ble_initialized) return;
    if (data == NULL || len == 0) return;
    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE || s_tx_val_handle == 0) return;

    size_t off = 0;
    while (off < len) {
        size_t chunk = len - off;
        if (chunk > BLE_SERIAL_TX_CHUNK_MAX) {
            chunk = BLE_SERIAL_TX_CHUNK_MAX;
        }

        bool sent = false;
        for (int retry = 0; retry < 200; retry++) {
            struct os_mbuf *om = ble_hs_mbuf_from_flat(data + off, (uint16_t)chunk);
            if (om == NULL) {
                vTaskDelay(pdMS_TO_TICKS(5));
                continue;
            }

            int rc = ble_gatts_notify_custom(s_conn_handle, s_tx_val_handle, om);
            if (rc == 0) {
                sent = true;
                vTaskDelay(pdMS_TO_TICKS(1));
                break;
            }
            if (rc == BLE_HS_ENOTCONN || rc == BLE_HS_EAPP) {
                return;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }

        if (!sent) {
            ESP_LOGW(TAG, "BLE TX dropped at offset %u", (unsigned)off);
            return;
        }

        off += chunk;
    }
#endif
}

int ble_serial_is_connected(void)
{
#if !CONFIG_BT_ENABLED || !CONFIG_BT_NIMBLE_ENABLED
    return 0;
#else
    if (!s_ble_initialized) return 0;
    return s_conn_handle != BLE_HS_CONN_HANDLE_NONE;
#endif
}

void ble_serial_set_line_buffer(const char *line)
{
#if CONFIG_BT_ENABLED && CONFIG_BT_NIMBLE_ENABLED
    if (line == NULL) {
        s_line_len = 0;
        s_line_buf[0] = '\0';
        return;
    }
    size_t len = strnlen(line, BLE_SERIAL_LINE_BUF_SIZE - 1);
    memcpy(s_line_buf, line, len);
    s_line_buf[len] = '\0';
    s_line_len = len;
#else
    (void)line;
#endif
}
