// src/ppcomp_callbacks.cpp
// Descrição: Handlers de callback do SDI Server.
//
// Mapeamento ABECS 2.20 → SDI:
//   cb_emv_ct()       ← SDI_CT_Init_Framework callback (SW1=91, EMV Contact events)
//   cb_emv_ctls()     ← SDI_CTLS_Init_Framework callback (SW1=91, CTLS events)
//   cb_sdi_status()   ← SDI_SetSdiCallback(SW1=9F, PIN digit count)
//   cb_card_removal() ← SDI_SetSdiCallback(SW1=9E, SW2=04)
//   cb_ctls_notify()  ← SDI_SetSdiCallback(SW1=9E, SW2=01)
//
// REGRA CRÍTICA: callbacks são chamados de threads SDI diferentes.
// Todos devem adquirir g_sdi_callback_mutex antes de modificar estado global.
// Callbacks devem ser O(microseconds) — sem I/O ou alocação dinâmica.

#include <cstring>
#include <mutex>

#include <sdiclient/sdi_emv.h>
#include <sdiclient/sdi_if.h>

#include "ppcomp.h"
#include "ppcomp_internal.h"
#include "pp_sdi_bridge.h"

// ─── Callback EMV Contact ────────────────────────────────────────────────────

EMV_ADK_INFO cb_emv_ct(uint8_t* data, uint32_t len) {
    std::lock_guard<std::mutex> lock(g_sdi_callback_mutex);

    if (!data || len == 0) {
        LOG_PP_ERROR("cb_emv_ct: dados nulos");
        return EMV_ADK_OK;
    }

    uint8_t eventType = data[0];

    switch (eventType) {
        case EMV_CB_SELECT_APP:
            // AID selection: sinaliza para thread principal via g_candidate_count
            // O conteúdo completo dos candidatos vem via EMV_CT_STARTRESULT_STRUCT
            LOG_PP_DEBUG("cb_emv_ct: SELECT_APP event, numCandidates=%d",
                         (len > 1) ? data[1] : 0);
            if (len > 1) {
                g_candidate_count = data[1];
            }
            break;

        case EMV_CB_ONLINE_PIN:
            // Terminal deve capturar PIN online
            g_pin_requested = true;
            LOG_PP_INFO("cb_emv_ct: ONLINE_PIN requested");
            break;

        case EMV_CB_SCRIPT_RESULT:
            // Salvar resultado de script issuer
            if (len > 1) {
                pp_save_script_result(data + 1, len - 1);
            }
            break;

        case EMV_CB_WRONG_PIN:
            g_pin_retry_count++;
            LOG_PP_INFO("cb_emv_ct: WRONG_PIN, tentativa %d", g_pin_retry_count);
            break;

        case EMV_CB_PIN_BLOCKED:
            g_pin_blocked = true;
            LOG_PP_INFO("cb_emv_ct: PIN_BLOCKED");
            break;

        default:
            LOG_PP_DEBUG("cb_emv_ct: evento desconhecido 0x%02X", eventType);
            break;
    }

    // SEMPRE retornar EMV_ADK_OK — nunca EMV_ADK_WAIT (bloqueia o SDI)
    return EMV_ADK_OK;
}

// ─── Callback EMV Contactless ─────────────────────────────────────────────────

EMV_ADK_INFO cb_emv_ctls(uint8_t* data, uint32_t len) {
    std::lock_guard<std::mutex> lock(g_sdi_callback_mutex);

    if (!data || len == 0) return EMV_ADK_OK;

    uint8_t eventType = data[0];

    switch (eventType) {
        case EMV_CTLS_CB_SELECT_KERNEL:
            // Aceitar o kernel proposto pelo SDI
            LOG_PP_DEBUG("cb_emv_ctls: SELECT_KERNEL, kernel=%d",
                         (len > 1) ? data[1] : 0);
            break;

        case EMV_CTLS_CB_ONLINE_PIN:
            g_pin_requested = true;
            LOG_PP_INFO("cb_emv_ctls: ONLINE_PIN requested");
            break;

        default:
            LOG_PP_DEBUG("cb_emv_ctls: evento desconhecido 0x%02X", eventType);
            break;
    }

    return EMV_ADK_OK;
}

// ─── Callback SDI Status (SW1=9F) ────────────────────────────────────────────
// Recebe eventos de status: dígitos de PIN digitados, entrada manual, etc.

void cb_sdi_status(uint8_t* data, uint32_t len) {
    std::lock_guard<std::mutex> lock(g_sdi_callback_mutex);

    if (!data || len < 2) return;

    uint8_t statusType = data[0];
    uint8_t statusValue = data[1];

    switch (statusType) {
        case 0x01:  // PIN digit count update
            g_pin_digit_count = statusValue;
            // Não logar — poderia revelar comprimento do PIN
            break;

        case 0x02:  // Manual entry progress
            LOG_PP_DEBUG("cb_sdi_status: manual entry, digits=%d", statusValue);
            break;

        default:
            break;
    }
}

// ─── Callback remoção de cartão (SW1=9E, SW2=04) ─────────────────────────────

void cb_card_removal(uint8_t* data, uint32_t len) {
    std::lock_guard<std::mutex> lock(g_sdi_callback_mutex);
    (void)data;
    (void)len;

    g_card_removed_mid_txn = true;
    LOG_PP_INFO("cb_card_removal: cartao removido durante transacao");
    // Sem dados sensíveis — apenas sinaliza o evento
}

// ─── Callback notificação CTLS (SW1=9E, SW2=01) ──────────────────────────────
// Eventos de LED e status RF — gerenciados pelo VAOS, geralmente ignorar.

void cb_ctls_notify(uint8_t* data, uint32_t len) {
    (void)data;
    (void)len;
    // LEDs e animações NFC são gerenciados pelo VAOS automaticamente.
    // Nenhuma ação necessária nesta implementação.
}
