/*
 * Embedding boundary for applications that own app_main and TinyUSB.
 * Credential storage remains deliberately locked until file operations can be
 * mapped transactionally onto the host callbacks.
 */
#include "pico_fido_embedded.h"

#include "apdu.h"
#include "esp_compat.h"
#include "mbedtls/sha256.h"
#include "pico_keys.h"
#include "usb.h"

#include <stdio.h>
#include <string.h>

static pico_fido_embedded_config_t embedded_config;
static bool embedded_initialized;
static bool embedded_active;

app_t apps[16];
uint8_t num_apps;
app_t *current_app;
const uint8_t *ccid_atr;
int (*button_pressed_cb)(uint8_t);
bool cancel_button;
struct apdu apdu;
char pico_serial_str[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
uint8_t pico_serial_hash[32];
pico_unique_board_id_t pico_serial;
TaskHandle_t hcore0 = NULL;
TaskHandle_t hcore1 = NULL;

static bool rtc_was_set;

extern int pico_fido_hid_receive_packet(const uint8_t packet[64]);
extern bool pico_fido_hid_ready(void);
extern void pico_fido_hid_reset(void);
bool pico_fido_embedded_send_packet(const uint8_t packet[64]);
bool pico_fido_embedded_check_presence(uint32_t timeout_ms);

bool app_exists(const uint8_t *aid, size_t aid_len) {
    for (uint8_t i = 0; i < num_apps; i++) {
        if (aid_len >= apps[i].aid[0] &&
            memcmp(apps[i].aid + 1, aid, apps[i].aid[0]) == 0) {
            return true;
        }
    }
    return false;
}

int register_app(int (*select_aid)(app_t *, uint8_t), const uint8_t *aid) {
    if (app_exists(aid + 1, aid[0])) {
        return 1;
    }
    if (num_apps >= sizeof(apps) / sizeof(apps[0])) {
        return 0;
    }
    apps[num_apps].select_aid = select_aid;
    apps[num_apps].aid = aid;
    num_apps++;
    return 1;
}

int select_app(const uint8_t *aid, size_t aid_len) {
    for (uint8_t i = 0; i < num_apps; i++) {
        if (aid_len >= apps[i].aid[0] &&
            memcmp(apps[i].aid + 1, aid, apps[i].aid[0]) == 0) {
            current_app = &apps[i];
            return current_app->select_aid(current_app, 1);
        }
    }
    return PICOKEY_ERR_FILE_NOT_FOUND;
}

bool is_req_button_pending(void) {
    return false;
}

bool wait_button(void) {
    return !pico_fido_embedded_check_presence(15000);
}

int picokey_init(void) {
    return PICOKEY_OK;
}

bool has_set_rtc(void) {
    return rtc_was_set;
}

void set_rtc_time(time_t value) {
    struct timeval tv = {.tv_sec = value, .tv_usec = 0};
    settimeofday(&tv, NULL);
    rtc_was_set = true;
}

time_t get_rtc_time(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec;
}

bool pico_fido_embedded_send_packet(const uint8_t packet[64]) {
    return embedded_active && embedded_config.send != NULL &&
           embedded_config.send(embedded_config.transport_ctx, packet);
}

bool pico_fido_embedded_check_presence(uint32_t timeout_ms) {
    return embedded_active && embedded_config.check_presence != NULL &&
           embedded_config.check_presence(embedded_config.presence_ctx,
                                          timeout_ms);
}

pico_fido_embedded_result_t pico_fido_embedded_init(
    const pico_fido_embedded_config_t *config) {
    bool any_storage_callback = config != NULL &&
        (config->storage.read != NULL || config->storage.write != NULL ||
         config->storage.erase != NULL);
    bool all_storage_callbacks = config != NULL &&
        config->storage.read != NULL && config->storage.write != NULL &&
        config->storage.erase != NULL;
    if (config == NULL || config->send == NULL || config->check_presence == NULL ||
        (any_storage_callback && !all_storage_callbacks) ||
        config->device_id_size > sizeof(pico_serial.id) ||
        (config->device_id_size > 0 && config->device_id == NULL)) {
        return PICO_FIDO_EMBEDDED_INVALID_ARGUMENT;
    }
    if (embedded_active) {
        return PICO_FIDO_EMBEDDED_INVALID_STATE;
    }

    embedded_config = *config;
    memset(&pico_serial, 0, sizeof(pico_serial));
    if (config->device_id_size > 0) {
        memcpy(pico_serial.id, config->device_id, config->device_id_size);
    }
    memset(pico_serial_str, 0, sizeof(pico_serial_str));
    for (size_t i = 0; i < sizeof(pico_serial.id); i++) {
        snprintf(&pico_serial_str[2 * i], 3, "%02X", pico_serial.id[i]);
    }
    mbedtls_sha256(pico_serial.id, sizeof(pico_serial.id), pico_serial_hash,
                   false);

    if (!embedded_initialized) {
        usb_init();
        if (!pico_fido_hid_ready()) {
            memset(&embedded_config, 0, sizeof(embedded_config));
            return PICO_FIDO_EMBEDDED_INVALID_STATE;
        }
        embedded_initialized = true;
    }
    pico_fido_hid_reset();
    embedded_active = true;
    return PICO_FIDO_EMBEDDED_OK;
}

pico_fido_embedded_result_t pico_fido_embedded_receive(
    const uint8_t packet[PICO_FIDO_EMBEDDED_PACKET_SIZE]) {
    if (!embedded_active) {
        return PICO_FIDO_EMBEDDED_INVALID_STATE;
    }
    if (packet == NULL) {
        return PICO_FIDO_EMBEDDED_INVALID_ARGUMENT;
    }
    return pico_fido_hid_receive_packet(packet) == PICOKEY_OK
               ? PICO_FIDO_EMBEDDED_OK
               : PICO_FIDO_EMBEDDED_TRANSPORT_ERROR;
}

void pico_fido_embedded_poll(void) {
    if (embedded_active) {
        usb_task();
    }
}

void pico_fido_embedded_deinit(void) {
    if (!embedded_active) {
        return;
    }
    card_exit();
    pico_fido_hid_reset();
    embedded_active = false;
    memset(&embedded_config, 0, sizeof(embedded_config));
}

bool pico_fido_embedded_is_ready(void) {
    return embedded_active;
}

pico_fido_embedded_result_t pico_fido_embedded_storage_status(void) {
    return PICO_FIDO_EMBEDDED_LOCKED;
}
