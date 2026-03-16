// src/ppcomp_emv_ctls.cpp
// Descrição: Fluxo EMV Contactless (NFC/CTLS).
//
// DIFERENÇAS CRÍTICAS do fluxo CT:
//   - NÃO há seleção de AID explícita — kernel seleciona internamente
//   - SetupTransaction NÃO bloqueia — ativa RF e retorna imediatamente
//   - ContinueOffline retorna EMV_ADK_NO_CARD enquanto aguarda toque
//   - Maioria das transações CTLS é offline puro (sem ContinueOnline)
//   - Fallback para chip CT é caminho esperado e comum
//
// Mapeamento ABECS 2.20 → SDI:
//   PP_GoOnChipCTLS()   → SDI_CTLS_SetupTransaction + Dialog::requestCard
//   PP_PollCTLS()       → SDI_CTLS_ContinueOffline (retorna NO_CARD até toque)
//   PP_FinishChipCTLS() → SDI_CTLS_ContinueOnline
//   PP_AbortCTLS()      → SDI_CTLS_Break + SDI_CTLS_EndTransaction

#include <cstring>
#include <mutex>

#include <sdiclient/sdi_emv.h>
#include <sdiclient/sdi_if.h>

#include "ppcomp.h"
#include "ppcomp_internal.h"
#include "pp_sdi_bridge.h"

extern "C" {

int PP_GoOnChipCTLS(int idPP, long valor, int tipoConta, int tipoTrans,
                    char* outBuf, int* outLen) {
    (void)idPP;
    (void)tipoConta;
    (void)outBuf;
    (void)outLen;

    LOG_PP_INFO("PP_GoOnChipCTLS: valor=%ld tipoTrans=0x%02X", valor, tipoTrans);

    if (!g_initialized) return PP_ERR_INIT;
    if (!g_tables_loaded) {
        LOG_PP_ERROR("PP_GoOnChipCTLS: tabelas não carregadas");
        return PP_ERR_STATE;
    }
    if (g_current_tec != TEC_CTLS) {
        LOG_PP_ERROR("PP_GoOnChipCTLS: tecnologia incorreta: %d", g_current_tec);
        return PP_ERR_NOTCHIP;
    }
    if (g_pp_state != PPState::CARD_CTLS) {
        LOG_PP_ERROR("PP_GoOnChipCTLS: estado incorreto: %d",
                     static_cast<int>(g_pp_state));
        return PP_ERR_STATE;
    }

    g_pp_state = PPState::EMV_CTLS_SETUP;

    // Reset de estado de transação
    {
        std::lock_guard<std::mutex> lock(g_sdi_callback_mutex);
        g_pin_entered         = false;
        g_pin_requested       = false;
        g_pin_blocked         = false;
        g_pin_retry_count     = 0;
        g_card_removed_mid_txn = false;
    }

    uint8_t emvTxnType = mapTxnTypeAbecsToEmv(tipoTrans);

    // Setup não bloqueia — ativa RF polling e retorna imediatamente
    EMV_ADK_INFO ret = SDI_CTLS_SetupTransaction(
        static_cast<uint32_t>(valor),
        0,  // cashback = 0 para CTLS
        CURRENCY_BRL_CODE,
        emvTxnType,
        nullptr);  // setupResult não usado nesta versão

    if (ret != EMV_ADK_OK) {
        LOG_PP_ERROR("PP_GoOnChipCTLS: SDI_CTLS_SetupTransaction falhou: 0x%X", ret);
        g_pp_state = PPState::CARD_CTLS;
        return mapEmvRetToAbecs(ret);
    }

    // Exibir mensagem para o usuário aproximar o cartão
    libsdi::Dialog::requestCard("Aproxime o cartão", TIMEOUT_DETECTION_MS);

    g_pp_state = PPState::EMV_CTLS_POLLING;

    LOG_PP_INFO("PP_GoOnChipCTLS: RF ativado, aguardando toque NFC");
    return PP_CTLS_WAITING;  // Sinaliza para libclisitef iniciar PP_PollCTLS
}

int PP_PollCTLS(int idPP, char* outBuf, int* outLen) {
    (void)idPP;

    if (!g_initialized) return PP_ERR_INIT;
    if (g_pp_state != PPState::EMV_CTLS_POLLING) {
        LOG_PP_ERROR("PP_PollCTLS: estado incorreto: %d", static_cast<int>(g_pp_state));
        return PP_ERR_STATE;
    }
    if (!outBuf || !outLen || *outLen < 80) return PP_ERR_BUFFER;

    EMV_CTLS_TRANSRES_STRUCT     transResult = {};
    EMV_SDI_CTLS_TRANSRES_STRUCT sdiExtra    = {};

    EMV_ADK_INFO ret = SDI_CTLS_ContinueOffline(0, &transResult, &sdiExtra);

    switch (ret) {
        case EMV_ADK_NO_CARD:
            // Ainda aguardando toque — sem cartão detectado
            return PP_ERR_NOEVENT;

        case EMV_ADK_OK:
        case EMV_ADK_OFFLINE_APPROVE:
            LOG_PP_INFO("PP_PollCTLS: aprovação offline CTLS");
            pp_serialize_ctls_result(&transResult, &sdiExtra, outBuf, outLen);
            g_pp_state = PPState::IDLE;
            g_current_tec = TEC_NONE;
            return PP_OFFLINE_APPROVE;

        case EMV_ADK_GO_ONLINE:
            LOG_PP_INFO("PP_PollCTLS: ir ao autorizador (ARQC CTLS)");
            pp_serialize_ctls_result(&transResult, &sdiExtra, outBuf, outLen);
            // Estado permanece CTLS_POLLING até PP_FinishChipCTLS ou PP_AbortCTLS
            return PP_GO_ONLINE;

        case EMV_ADK_DECLINE:
            LOG_PP_INFO("PP_PollCTLS: recusa offline CTLS");
            pp_serialize_ctls_result(&transResult, &sdiExtra, outBuf, outLen);
            g_pp_state = PPState::IDLE;
            g_current_tec = TEC_NONE;
            return PP_OFFLINE_DECLINE;

        case EMV_ADK_TXN_CTLS_MOBILE:
            // Segundo toque necessário (wallet mobile: Apple Pay, Google Pay)
            LOG_PP_INFO("PP_PollCTLS: wallet mobile — segundo toque necessário");
            libsdi::Dialog::requestCard("Aproxime novamente", TIMEOUT_DETECTION_MS);
            return PP_CTLS_WAITING;

        case EMV_ADK_FALLBACK:
            // Kernel sinalizou fallback — tentar chip CT
            LOG_PP_INFO("PP_PollCTLS: fallback para chip CT sinalizado");
            SDI_CTLS_Break();
            g_pp_state    = PPState::IDLE;
            g_current_tec = TEC_NONE;
            return PP_FALLBACK_CT;

        default:
            LOG_PP_ERROR("PP_PollCTLS: SDI_CTLS_ContinueOffline falhou: 0x%X", ret);
            g_pp_state    = PPState::IDLE;
            g_current_tec = TEC_NONE;
            return mapEmvRetToAbecs(ret);
    }
}

int PP_FinishChipCTLS(int idPP, char* arpc, int arpcLen,
                      char* scripts, int scriptsLen,
                      char* outBuf, int* outLen) {
    (void)idPP;
    (void)scripts;     // SDI_CTLS_ContinueOnline não aceita scripts nesta versão do ADK
    (void)scriptsLen;

    LOG_PP_INFO("PP_FinishChipCTLS: resposta online arpcLen=%d scriptsLen=%d",
                arpcLen, scriptsLen);

    if (!g_initialized) return PP_ERR_INIT;
    if (g_pp_state != PPState::EMV_CTLS_POLLING) {
        LOG_PP_ERROR("PP_FinishChipCTLS: estado incorreto: %d",
                     static_cast<int>(g_pp_state));
        return PP_ERR_STATE;
    }
    if (!arpc || arpcLen <= 0) return PP_ERR_PARAM;
    if (!outBuf || !outLen || *outLen < 80) return PP_ERR_BUFFER;

    EMV_CTLS_TRANSRES_STRUCT     transResult = {};
    EMV_SDI_CTLS_TRANSRES_STRUCT sdiExtra    = {};

    EMV_ADK_INFO ret = SDI_CTLS_ContinueOnline(
        reinterpret_cast<const uint8_t*>(arpc),
        static_cast<uint32_t>(arpcLen),
        &transResult,
        &sdiExtra);

    if (ret != EMV_ADK_OK && ret != EMV_ADK_OFFLINE_APPROVE && ret != EMV_ADK_DECLINE) {
        LOG_PP_ERROR("PP_FinishChipCTLS: ContinueOnline falhou: 0x%X", ret);
        return mapEmvRetToAbecs(ret);
    }

    pp_serialize_ctls_result(&transResult, &sdiExtra, outBuf, outLen);

    g_pp_state    = PPState::IDLE;
    g_current_tec = TEC_NONE;

    int abecs_ret = (ret == EMV_ADK_DECLINE) ? PP_OFFLINE_DECLINE : PP_OFFLINE_APPROVE;
    LOG_PP_INFO("PP_FinishChipCTLS: resultado=%d", abecs_ret);
    return abecs_ret;
}

int PP_AbortCTLS(int idPP) {
    (void)idPP;

    LOG_PP_INFO("PP_AbortCTLS");

    if (!g_initialized) return PP_OK;

    if (g_pp_state == PPState::EMV_CTLS_SETUP ||
        g_pp_state == PPState::EMV_CTLS_POLLING) {
        SDI_CTLS_Break();
        SDI_CTLS_EndTransaction(EMV_ADK_TXN_ABORT);
    }

    g_pp_state    = PPState::IDLE;
    g_current_tec = TEC_NONE;

    return PP_OK;
}

}  // extern "C"
