cmake_minimum_required(VERSION 3.22)
project(libppcomp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── Opções de build ──────────────────────────────────────────────────────────

option(BUILD_TESTS    "Build unit tests (x86_64, requer mock SDI)" OFF)
option(USE_SDI_MOCK   "Usar mock SDI em vez das libs Verifone"     OFF)

# ─── Flags de compilação ──────────────────────────────────────────────────────

add_compile_options(
    -fno-rtti
    -fno-exceptions
    -Wall
    -Werror
    -Wno-unused-parameter
)

# ─── Verificação de ABI (Android) ────────────────────────────────────────────

if(ANDROID)
    if(NOT ANDROID_ABI STREQUAL "arm64-v8a")
        message(FATAL_ERROR "libppcomp suporta apenas arm64-v8a. ABI atual: ${ANDROID_ABI}")
    endif()
    message(STATUS "Build Android: ABI=${ANDROID_ABI} API=${ANDROID_PLATFORM}")
endif()

# ─── Fontes da biblioteca ─────────────────────────────────────────────────────

set(PPCOMP_SOURCES
    src/ppcomp_globals.cpp
    src/ppcomp_errors.cpp
    src/ppcomp_mapping.cpp
    src/ppcomp_callbacks.cpp
    src/ppcomp_lifecycle.cpp
    src/ppcomp_tables.cpp
    src/ppcomp_detection.cpp
    src/ppcomp_emv_ct.cpp
    src/ppcomp_emv_ctls.cpp
    src/ppcomp_pin.cpp
    tables/table_loader.cpp
)

add_library(ppcomp SHARED ${PPCOMP_SOURCES})

target_include_directories(ppcomp PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tables
    ${CMAKE_SOURCE_DIR}/libs/include
)

# Em builds mock (x86_64 para testes), usar headers stub em vez do ADK real
if(USE_SDI_MOCK)
    target_include_directories(ppcomp PRIVATE ${CMAKE_SOURCE_DIR}/tests/mocks)
    target_compile_definitions(ppcomp PRIVATE USE_SDI_MOCK=1)
endif()

# ─── Bibliotecas Verifone ADK ────────────────────────────────────────────────

if(ANDROID AND NOT USE_SDI_MOCK)
    set(ADK_LIB_DIR ${CMAKE_SOURCE_DIR}/libs/arm64-v8a)

    add_library(sdiclient        STATIC IMPORTED)
    add_library(emv_ct_client    STATIC IMPORTED)
    add_library(emv_ctls_client  STATIC IMPORTED)
    add_library(tlv_util         STATIC IMPORTED)
    add_library(sdiprotocol      SHARED IMPORTED)
    add_library(ipc_vfi          SHARED IMPORTED)
    add_library(vfilog           SHARED IMPORTED)

    set_target_properties(sdiclient       PROPERTIES IMPORTED_LOCATION ${ADK_LIB_DIR}/libsdiclient.a)
    set_target_properties(emv_ct_client   PROPERTIES IMPORTED_LOCATION ${ADK_LIB_DIR}/libEMV_CT_Client.a)
    set_target_properties(emv_ctls_client PROPERTIES IMPORTED_LOCATION ${ADK_LIB_DIR}/libEMV_CTLS_Client.a)
    set_target_properties(tlv_util        PROPERTIES IMPORTED_LOCATION ${ADK_LIB_DIR}/libTLV_Util.a)
    set_target_properties(sdiprotocol     PROPERTIES IMPORTED_LOCATION ${ADK_LIB_DIR}/libsdiprotocol.so)
    set_target_properties(ipc_vfi         PROPERTIES IMPORTED_LOCATION ${ADK_LIB_DIR}/libipc.so)
    set_target_properties(vfilog          PROPERTIES IMPORTED_LOCATION ${ADK_LIB_DIR}/libvfilog.so)

    target_link_libraries(ppcomp PRIVATE
        sdiclient
        emv_ct_client
        emv_ctls_client
        tlv_util
        sdiprotocol
        ipc_vfi
        vfilog
        android
        log
    )
endif()

# CRÍTICO: não fazer strip — a libclisitef.so valida assinatura criptográfica
set_target_properties(ppcomp PROPERTIES
    OUTPUT_NAME "ppcomp"
    LINK_FLAGS "-Wl,--no-strip-debug"
)

# ─── Testes unitários ─────────────────────────────────────────────────────────

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
