from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_embedded_public_api_exposes_lifecycle_and_64_byte_transport():
    header = (ROOT / "include" / "pico_fido_embedded.h").read_text()

    assert "PICO_FIDO_EMBEDDED_PACKET_SIZE 64" in header
    assert "pico_fido_embedded_init" in header
    assert "pico_fido_embedded_receive" in header
    assert "pico_fido_embedded_poll" in header
    assert "pico_fido_embedded_deinit" in header
    assert "pico_fido_embedded_storage_t" in header
    assert "pico_fido_embedded_presence_cb_t" in header
    assert "PICO_FIDO_EMBEDDED_LOCKED" in header


def test_embedded_build_excludes_owned_entrypoint_and_usb_descriptors():
    cmake = (ROOT / "pico-keys-sdk" / "pico_keys_sdk_import.cmake").read_text()
    hid = (ROOT / "pico-keys-sdk" / "src" / "usb" / "hid" / "hid.c").read_text()

    assert "if(NOT PICO_FIDO_EMBEDDED)\n    list(APPEND PICO_KEYS_SOURCES\n        ${CMAKE_CURRENT_LIST_DIR}/src/main.c" in cmake
    assert "elseif(NOT PICO_FIDO_EMBEDDED)" in cmake
    assert "PICO_FIDO_EMBEDDED" in (ROOT / "CMakeLists.txt").read_text()
    assert "#ifndef PICO_FIDO_EMBEDDED\nvoid tud_hid_report_complete_cb" in hid


def test_embedded_mode_forces_optional_protocols_off_and_is_explicitly_locked():
    cmake = (ROOT / "CMakeLists.txt").read_text()
    runtime = (ROOT / "src" / "fido" / "embedded.c").read_text()

    for option in ("ENABLE_OATH_APP", "ENABLE_OTP_APP", "ENABLE_PQC"):
        assert f"set({option} OFF CACHE BOOL" in cmake
    assert "PICO_FIDO_EMBEDDED_LOCKED" in runtime
    assert "init_otp_files" not in runtime
    assert "otp_enable_secure_boot" not in runtime
    assert "tinyusb_driver_install" not in runtime
    assert "tusb_init" not in runtime


def test_locked_mode_never_dispatches_credential_or_u2f_handlers():
    cbor = (ROOT / "src" / "fido" / "cbor.c").read_text()
    hid = (ROOT / "pico-keys-sdk" / "src" / "usb" / "hid" / "hid.c").read_text()
    usb = (ROOT / "pico-keys-sdk" / "src" / "usb" / "usb.c").read_text()

    assert "#ifdef PICO_FIDO_EMBEDDED\n    /* Storage adapter is intentionally pending." in cbor
    assert "return CTAP2_ERR_OPERATION_DENIED;" in cbor
    assert "U2F depends on persistent key material" in hid
    assert "return ctap_error(CTAP1_ERR_OTHER);" in hid
    assert "#ifndef PICO_FIDO_EMBEDDED\n    low_flash_init_core1();" in usb


def test_embedded_hid_rejects_oversized_ping_and_resets_session_state():
    hid = (ROOT / "pico-keys-sdk" / "src" / "usb" / "hid" / "hid.c").read_text()
    runtime = (ROOT / "src" / "fido" / "embedded.c").read_text()

    assert "MSG_LEN(ctap_req) > sizeof(ctap_req->init.data)" in hid
    assert "MSG_LEN(ctap_req) != sizeof(((CTAPHID_INIT_REQ *)0)->nonce)" in hid
    assert "resp->capFlags = CAPFLAG_WINK;" in hid
    assert "pico_fido_hid_reset();" in runtime
    assert "if (!pico_fido_hid_ready())" in runtime


def test_embedded_sdk_source_list_excludes_irreversible_provisioning():
    cmake = (ROOT / "pico-keys-sdk" / "config" / "esp32" / "components" /
             "pico-keys-sdk" / "CMakeLists.txt").read_text()

    assert "set(PICO_KEYS_EMBEDDED_SOURCES" in cmake
    embedded_sources = cmake.split("set(PICO_KEYS_EMBEDDED_SOURCES", 1)[1].split(")", 1)[0]
    for forbidden in ("otp.c", "rescue.c", "main.c", "eac.c", "led.c",
                      "usb_descriptors.c"):
        assert forbidden not in embedded_sources
    assert "if(PICO_FIDO_EMBEDDED)\n    set(PICO_KEYS_SOURCES ${PICO_KEYS_EMBEDDED_SOURCES})" in cmake
