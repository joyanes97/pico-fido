#ifndef PICO_FIDO_EMBEDDED_H
#define PICO_FIDO_EMBEDDED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PICO_FIDO_EMBEDDED_PACKET_SIZE 64

typedef enum {
    PICO_FIDO_EMBEDDED_OK = 0,
    PICO_FIDO_EMBEDDED_INVALID_ARGUMENT = -1,
    PICO_FIDO_EMBEDDED_INVALID_STATE = -2,
    PICO_FIDO_EMBEDDED_TRANSPORT_ERROR = -3,
    PICO_FIDO_EMBEDDED_LOCKED = -4,
} pico_fido_embedded_result_t;

typedef bool (*pico_fido_embedded_send_cb_t)(
    void *ctx,
    const uint8_t packet[PICO_FIDO_EMBEDDED_PACKET_SIZE]);
typedef bool (*pico_fido_embedded_presence_cb_t)(void *ctx,
                                                 uint32_t timeout_ms);

typedef struct {
    void *ctx;
    bool (*read)(void *ctx, uint32_t key, size_t offset, void *data,
                 size_t size);
    bool (*write)(void *ctx, uint32_t key, size_t offset, const void *data,
                  size_t size);
    bool (*erase)(void *ctx, uint32_t key);
} pico_fido_embedded_storage_t;

typedef struct {
    void *transport_ctx;
    pico_fido_embedded_send_cb_t send;
    void *presence_ctx;
    pico_fido_embedded_presence_cb_t check_presence;
    pico_fido_embedded_storage_t storage;
    const uint8_t *device_id;
    size_t device_id_size;
} pico_fido_embedded_config_t;

pico_fido_embedded_result_t pico_fido_embedded_init(
    const pico_fido_embedded_config_t *config);
pico_fido_embedded_result_t pico_fido_embedded_receive(
    const uint8_t packet[PICO_FIDO_EMBEDDED_PACKET_SIZE]);
void pico_fido_embedded_poll(void);
void pico_fido_embedded_deinit(void);
bool pico_fido_embedded_is_ready(void);
pico_fido_embedded_result_t pico_fido_embedded_storage_status(void);

#ifdef __cplusplus
}
#endif

#endif
