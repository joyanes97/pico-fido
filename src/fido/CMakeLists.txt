option(PICO_FIDO_EMBEDDED
    "Build embeddable CTAP core without app_main or TinyUSB ownership" OFF)

if(PICO_FIDO_EMBEDDED)
    set(ENABLE_OATH_APP OFF CACHE BOOL "OATH disabled in embedded mode" FORCE)
    set(ENABLE_OTP_APP OFF CACHE BOOL "OTP disabled in embedded mode" FORCE)
    set(ENABLE_PQC OFF CACHE BOOL "PQC disabled in embedded mode" FORCE)
    add_compile_definitions(PICO_FIDO_EMBEDDED=1 USB_ITF_HID=1)
    include(${CMAKE_CURRENT_LIST_DIR}/sources.cmake)
    set(SOURCES ${PICO_FIDO_CORE_SOURCES} ${CMAKE_CURRENT_LIST_DIR}/embedded.c)
endif()

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS . ../../include
    REQUIRES mbedtls pico-keys-sdk tinycbor
)
idf_component_set_property(${COMPONENT_NAME} WHOLE_ARCHIVE ON)
