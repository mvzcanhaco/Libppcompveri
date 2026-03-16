// src/ppcomp_lifecycle.cpp
// Descrição: Funções de ciclo de vida do pinpad ABECS 2.20.
//
// Mapeamento ABECS 2.20 → SDI:
//   PP_Open()   → SDI_ProtocolInit + SDI_CT_Init_Framework + SDI_CTLS_Init_Framework
//   PP_Close()  → SDI_CT_Exit_Framework + SDI_CTLS_Exit_Framework + SDI_ProtocolExit
//   PP_Reset()  → PP_Abort + reset de estado
//   PP_GetInfo()→ retorna versão/modelo hardcoded (informações do firmware via SDI)
//   PP_Abort()  → trata todos os estados possíveis de g_pp_state

#include <cstring>
#include <cstdio>
#include <mutex>

#include <sdiclient/sdi_emv.h>
#include <sdiclient/sdi_if.h>

#include "ppcomp.h"
#include "ppcomp_internal.h"
#include "pp_sdi_bridge.h"

// Forward declarations dos callbacks (definidos em ppcomp_callbacks.cpp)
EMV_ADK_INFO cb_emv_ct(uint8_t* data, uint32_t len);
EMV_ADK_INFO cb_emv_ctls(uint8_t* data, uint32_t len);
void         cb_sdi_status(uint8_t* data, uint32_t len);
void         cb_card_removal(uint8_t* data, uint32_t len);
void         cb_ctls_notify(uint8_t* data, uint32_t len);

// SDI Server connection config
static constexpr const char* SDI_CONFIG_JSON =
    R"({"server":{"host":"127.0.0.1","port":12000}})";

// ─── Reset de estado interno ──────────────────────────────────────────────────

static void reset_state() {
    std::lock_guard<std::mutex> lock(g_sdi_callback_mutex);
    g_pp_state              = PPState::IDLE;
    g_current_tec           = TEC_NONE;
    g_detection_active      = false;
    g_pin_entered           = false;
    g_pin_requested         = false;
    g_pin_blocked           = false;
    g_pin_digit_count       = 0;
    g_pin_retry_count       = 0;
    g_card_removed_mid_txn  = false;
    g_candidate_count       = 0;
}

// ─── API ABECS ────────────────────────────────────────────────────────────────

extern "C" {

int PP_Open(int portaFisica, char* parametros) {
    (void)portaFisica;
    (void)parametros;

    LOG_PP_INFO("PP_Open: iniciando conexão com SDI Server");

    if (g_initialized) {
        LOG_PP_INFO("PP_Open: já inicializado, reiniciando");
        PP_Close(0);
    }

    // 1. Conectar ao SDI Server via socket local 127.0.0.1:12000
    EMV_ADK_INFO ret = SDI_ProtocolInit(SDI_CONFIG_JSON);
    if (ret != EMV_ADK_OK) {
        LOG_PP_ERROR("PP_Open: SDI_ProtocolInit falhou: 0x%X", ret);
        return PP_ERR_INIT;
    }

    // 2. Inicializar framework EMV Contact com callback
    ret = SDI_CT_Init_Framework(cb_emv_ct);
    if (ret != EMV_ADK_OK) {
        LOG_PP_ERROR("PP_Open: SDI_CT_Init_Framework falhou: 0x%X", ret);
        SDI_ProtocolExit();
        return PP_ERR_INIT;
    }

    // 3. Inicializar framework EMV Contactless com callback
    ret = SDI_CTLS_Init_Framework(cb_emv_ctls);
    if (ret != EMV_ADK_OK) {
        LOG_PP_ERROR("PP_Open: SDI_CTLS_Init_Framework falhou: 0x%X", ret);
        SDI_CT_Exit_Framework();
        SDI_ProtocolExit();
        return PP_ERR_INIT;
    }

    // 4. Registrar callbacks de status SDI
    // SW1=9F: status (PIN digits, etc.)
    SDI_SetSdiCallback(reinterpret_cast<void*>(cb_sdi_status), 0x9F, 0x00);
    // SW1=9E SW2=04: remoção de cartão
    SDI_SetSdiCallback(reinterpret_cast<void*>(cb_card_removal), 0x9E, 0x04);
    // SW1=9E SW2=01: notificações CTLS/LED
    SDI_SetSdiCallback(reinterpret_cast<void*>(cb_ctls_notify), 0x9E, 0x01);

    // 5. Estado inicial
    reset_state();
    g_initialized = true;

    LOG_PP_INFO("PP_Open: SDI Server conectado, pinpad pronto");
    return PP_OK;
}

int PP_Close(int idPP) {
    (void)idPP;

    LOG_PP_INFO("PP_Close: encerrando conexão com SDI Server");

    if (!g_initialized) {
        LOG_PP_ERROR("PP_Close: não inicializado");
        return PP_ERR_INIT;
    }

    // Abortar operação em andamento
    if (g_pp_state != PPState::IDLE) {
        PP_Abort(0);
    }

    SDI_CT_Exit_Framework();
    SDI_CTLS_Exit_Framework();
    SDI_ProtocolExit();

    reset_state();
    g_initialized = false;
    g_tables_loaded = false;

    LOG_PP_INFO("PP_Close: encerrado");
    return PP_OK;
}

int PP_Reset(int idPP) {
    (void)idPP;

    LOG_PP_INFO("PP_Reset: reiniciando estado do pinpad");

    if (!g_initialized) {
        LOG_PP_ERROR("PP_Reset: não inicializado");
        return PP_ERR_INIT;
    }

    // Abortar qualquer operação em andamento
    if (g_pp_state != PPState::IDLE) {
        PP_Abort(0);
    }

    reset_state();

    LOG_PP_INFO("PP_Reset: estado resetado para IDLE");
    return PP_OK;
}

int PP_GetInfo(int idPP, int tipo, char* buffer, int tamBuffer) {
    (void)idPP;

    if (!buffer || tamBuffer <= 0) return PP_ERR_PARAM;
    if (!g_initialized) return PP_ERR_INIT;

    memset(buffer, 0, tamBuffer);

    switch (tipo) {
        case 0:  // Versão da biblioteca
            snprintf(buffer, tamBuffer, "libppcomp 1.0.0 / ADK 5.0.3 / ABECS 2.20");
            break;
        case 1:  // Modelo do terminal
            snprintf(buffer, tamBuffer, "Verifone V660P-A (VAOS/VOS3)");
            break;
        case 2:  // Número de série — seria obtido do SDI em produção
            snprintf(buffer, tamBuffer, "V660P-SERIAL-UNKNOWN");
            break;
        default:
            LOG_PP_ERROR("PP_GetInfo: tipo desconhecido %d", tipo);
            return PP_ERR_PARAM;
    }

    LOG_PP_DEBUG("PP_GetInfo: tipo=%d info='%s'", tipo, buffer);
    return PP_OK;
}

int PP_Abort(int idPP) {
    (void)idPP;

    LOG_PP_INFO("PP_Abort: abortando operação, estado=%d",
                static_cast<int>(g_pp_state));

    if (!g_initialized) return PP_OK;  // Se não inicializado, nada a abortar

    // Tratar cada estado possível
    switch (g_pp_state) {
        case PPState::DETECTING:
            g_cardDetection.stopSelection();
            break;

        case PPState::EMV_CT_STARTED:
        case PPState::EMV_CT_OFFLINE:
        case PPState::EMV_CT_ONLINE:
            SDI_CT_EndTransaction(EMV_ADK_TXN_ABORT);
            break;

        case PPState::EMV_CTLS_SETUP:
        case PPState::EMV_CTLS_POLLING:
            SDI_CTLS_Break();
            SDI_CTLS_EndTransaction(EMV_ADK_TXN_ABORT);
            break;

        case PPState::PIN:
            g_ped.stopPinEntry();
            break;

        case PPState::IDLE:
        case PPState::CARD_CT:
        case PPState::CARD_CTLS:
        case PPState::CARD_MSR:
        case PPState::MANUAL_ENTRY:
            // Sem operação bloqueante em andamento
            break;
    }

    // Aguardar remoção do cartão (5s timeout) para não travar próxima transação
    if (g_pp_state != PPState::IDLE) {
        libsdi::SDI::waitCardRemoval(5);
    }

    reset_state();
    LOG_PP_INFO("PP_Abort: concluído, estado=IDLE");
    return PP_OK;
}

}  // extern "C"
