# ESP-IDF embedded mode

`PICO_FIDO_EMBEDDED=ON` builds Pico-FIDO as a host-owned CTAPHID core. The host
owns `app_main`, TinyUSB initialization, descriptors, and all TinyUSB callbacks.
Standalone behavior is unchanged when the option is `OFF`.

## Build contract

Set these cache variables before including Pico-FIDO components:

```cmake
set(PICO_FIDO_EMBEDDED ON CACHE BOOL "" FORCE)
set(ENABLE_OATH_APP OFF CACHE BOOL "" FORCE)
set(ENABLE_OTP_APP OFF CACHE BOOL "" FORCE)
set(ENABLE_PQC OFF CACHE BOOL "" FORCE)
```

Add `src/fido`, `pico-keys-sdk/config/esp32/components/pico-keys-sdk`, and
`pico-keys-sdk/config/esp32/components/tinycbor` to the parent project's
`EXTRA_COMPONENT_DIRS`. The component CMake files are self-contained when
`PICO_FIDO_EMBEDDED` is enabled.

Embedded mode omits `pico-keys-sdk/src/main.c` and
`pico-keys-sdk/src/usb/usb_descriptors.c`. It does not call `tusb_init`,
`tinyusb_driver_install`, provisioning, secure-boot, JTAG, or eFuse-write
operations. CCID, OATH, OTP, keyboard HID, and PQC are disabled.

## Host API

Include `pico_fido_embedded.h`. Call `pico_fido_embedded_init()` once USB is
ready, pass each complete 64-byte OUT report to
`pico_fido_embedded_receive()`, and call `pico_fido_embedded_poll()` regularly.
The `send` callback receives complete 64-byte IN reports and may synchronously
enqueue them into the host USB transport. Presence checks are delegated to
`check_presence`.

`device_id` is optional and limited to eight bytes. It identifies the device
without reading eFuse state.

## Storage blocker

Storage callbacks are part of the configuration contract but are not consumed
in this iteration. Supply either all three callbacks or none; partial storage
configuration is rejected. `pico_fido_embedded_storage_status()` always returns
`PICO_FIDO_EMBEDDED_LOCKED`. CTAP2 credential, assertion, PIN, reset,
management, configuration, large-blob, vendor, and `GetInfo` operations return
operation denied; U2F/OTP messages return a CTAPHID error. Only CTAPHID
initialization, ping, and locking remain available for transport integration
tests.

Do not advertise this build as a functional authenticator. Required follow-up:
map Pico-FIDO file records onto host storage with atomic write/erase semantics,
integrity/version handling, key lifecycle, and power-loss tests. Only then may
credential and assertion paths be unlocked.
