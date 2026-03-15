// src/ppcomp_emv_ct.cpp
// Descrição: Fluxo completo EMV Chip Contact (CT).
//
// Mapeamento ABECS 2.20 → SDI:
//   PP_GoOnChip()         → SDI_CT_StartTransaction
//   PP_SetAID()           → SDI_CT_SetSelectedApp (após PP_MULTIAPP)
//   PP_GoOnChipContinue() → SDI_CT_ContinueOffline
//   PP_FinishChip()       → SDI_CT_ContinueOnline
//   PP_UpdateTags()       → atualiza TLV antes de ContinueOffline
//   PP_EndChip()          → SDI_CT_EndTransaction + waitCardRemoval

#include <cstring>
#include <mutex>

#include <sdiclient/sdi_emv.h>
#include <sdiclient/sdi_if.h>

#include "ppcomp.h"
#include "ppcomp_internal.h"
#include "pp_sdi_bridge.h"

extern "C" {

int PP_GoOnChip(int idPP, long valor, int cashback, int tipoConta,
                int tipoTrans, char* outBuf, int* outLen) {
    (void)idPP;
    (void)tipoConta;

    LOG_PP_INFO("PP_GoOnChip: valor=%ld cashback=%d tipoTrans=0x%02X",
                valor, cashback, tipoTrans);

    if (!g_initialized) return PP_ERR_INIT;
    if (!g_tables_loaded) {
        LOG_PP_ERROR("PP_GoOnChip: tabelas não carregadas");
        return PP_ERR_STATE;
    }
    if (g_current_tec != TEC_CHIP_CT) {
        LOG_PP_ERROR("PP_GoOnChip: tecnologia incorreta: %d", g_current_tec);
        return PP_ERR_NOTCHIP;
    }
    if (g_pp_state != PPState::CARD_CT) {
        LOG_PP_ERROR("PP_GoOnChip: estado incorreto: %d", static_cast<int>(g_pp_state));
        return PP_ERR_STATE;
    }
    if (!outBuf || !outLen || *outLen < 64) return PP_ERR_BUFFER;

    g_pp_state = PPState::EMV_CT_STARTED;

    // Mapear tipo de transação ABECS → EMV (tag 9C)
    uint8_t emvTxnType = mapTxnTypeAbecsToEmv(tipoTrans);

    // Limpar script results da transação anterior
    {
        std::lock_guard<std::mutex> lock(g_sdi_callback_mutex);
        g_pin_entered   = false;
        g_pin_requested = false;
        g_pin_blocked   = false;
        g_pin_retry_count = 0;
        g_candidate_count = 0;
        g_card_removed_mid_txn = false;
    }

    EMV_CT_STARTRESULT_STRUCT startResult = {};

    EMV_ADK_INFO ret = SDI_CT_StartTransaction(
        static_cast<uint32_t>(valor),
        static_cast<uint32_t>(cashback),
        CURRENCY_BRL_CODE,
        emvTxnType,
        &startResult);

    if (ret == EMV_ADK_MULTI_APP) {
        LOG_PP_INFO("PP_GoOnChip: múltiplos AIDs (%d candidatos)", startResult.NumCandidates);
        pp_serialize_start_result(&startResult, outBuf, outLen);
        return PP_MULTIAPP;
    }

    if (ret != EMV_ADK_OK && ret != EMV_ADK_SEL_APP_OK) {
        LOG_PP_ERROR("PP_GoOnChip: SDI_CT_StartTransaction falhou: 0x%X", ret);
        g_pp_state = PPState::CARD_CT;
        return mapEmvRetToAbecs(ret);
    }

    // AID único selecionado — serializar resultado
    pp_serialize_start_result(&startResult, outBuf, outLen);

    LOG_PP_INFO("PP_GoOnChip: transação iniciada, AID selecionado len=%d",
                startResult.SelectedAIDLen);
    return PP_OK;
}

int PP_SetAID(int idPP, char* aid, int aidLen) {
    (void)idPP;

    LOG_PP_INFO("PP_SetAID: selecionando AID aidLen=%d", aidLen);

    if (!g_initialized) return PP_ERR_INIT;
    if (!aid || aidLen <= 0 || aidLen > static_cast<int>(MAX_AID_LEN)) return PP_ERR_PARAM;
    if (g_pp_state != PPState::EMV_CT_STARTED) return PP_ERR_STATE;

    EMV_ADK_INFO ret = SDI_CT_SetSelectedApp(
        reinterpret_cast<const uint8_t*>(aid), static_cast<uint8_t>(aidLen));
    if (ret != EMV_ADK_OK) {
        LOG_PP_ERROR("PP_SetAID: SDI_CT_SetSelectedApp falhou: 0x%X", ret);
        return mapEmvRetToAbecs(ret);
    }

    return PP_OK;
}

int PP_GoOnChipContinue(int idPP, char* outBuf, int* outLen) {
    (void)idPP;

    LOG_PP_INFO("PP_GoOnChipContinue: decisão offline");

    if (!g_initialized) return PP_ERR_INIT;
    if (g_pp_state != PPState::EMV_CT_STARTED) {
        LOG_PP_ERROR("PP_GoOnChipContinue: estado incorreto: %d",
                     static_cast<int>(g_pp_state));
        return PP_ERR_STATE;
    }
    if (!outBuf || !outLen || *outLen < 128) return PP_ERR_BUFFER;

    g_pp_state = PPState::EMV_CT_OFFLINE;

    EMV_CT_TRANSRES_STRUCT     transResult = {};
    EMV_SDI_CT_TRANSRES_STRUCT sdiExtra    = {};

    EMV_ADK_INFO ret = SDI_CT_ContinueOffline(0, &transResult, &sdiExtra);

    int abecs_ret;
    switch (ret) {
        case EMV_ADK_OK:
        case EMV_ADK_OFFLINE_APPROVE:
            abecs_ret = PP_OFFLINE_APPROVE;
            break;
        case EMV_ADK_GO_ONLINE:
        case EMV_ADK_ONLINE:
            abecs_ret = PP_GO_ONLINE;
            break;
        case EMV_ADK_DECLINE:
            abecs_ret = PP_OFFLINE_DECLINE;
            break;
        default:
            LOG_PP_ERROR("PP_GoOnChipContinue: ContinueOffline falhou: 0x%X", ret);
            g_pp_state = PPState::CARD_CT;
            return mapEmvRetToAbecs(ret);
    }

    // Verificar se cartão foi removido durante processamento
    {
        std::lock_guard<std::mutex> lock(g_sdi_callback_mutex);
        if (g_card_removed_mid_txn) {
            LOG_PP_ERROR("PP_GoOnChipContinue: cartão removido durante ContinueOffline");
            g_pp_state = PPState::IDLE;
            return PP_ERR_PINPAD;
        }
    }

    // Serializar resultado para buffer ABECS
    pp_serialize_trans_result(&transResult, &sdiExtra, outBuf, outLen);

    LOG_PP_INFO("PP_GoOnChipContinue: resultado=%d cryptogramType=0x%02X",
                abecs_ret, transResult.CryptogramType);
    return abecs_ret;
}

int PP_UpdateTags(int idPP, char* tlvData, int tlvLen) {
    (void)idPP;
    (void)tlvData;
    (void)tlvLen;
    // O SDI ADK 5.x não expõe SDI_CT_UpdateTags diretamente nesta versão.
    // Tags extras são passadas opcionalmente no StartTransaction.
    // Implementação futura quando ADK expuser esta função.
    LOG_PP_DEBUG("PP_UpdateTags: não implementado nesta versão do ADK");
    return PP_OK;
}

int PP_FinishChip(int idPP, char* arpc, int arpcLen,
                  char* scripts, int scriptsLen,
                  char* outBuf, int* outLen) {
    (void)idPP;

    LOG_PP_INFO("PP_FinishChip: resposta online arpcLen=%d scriptsLen=%d",
                arpcLen, scriptsLen);

    if (!g_initialized) return PP_ERR_INIT;
    if (g_pp_state != PPState::EMV_CT_OFFLINE) {
        LOG_PP_ERROR("PP_FinishChip: estado incorreto: %d", static_cast<int>(g_pp_state));
        return PP_ERR_STATE;
    }
    if (!arpc || arpcLen <= 0) return PP_ERR_PARAM;
    if (!outBuf || !outLen || *outLen < 128) return PP_ERR_BUFFER;

    g_pp_state = PPState::EMV_CT_ONLINE;

    EMV_CT_TRANSRES_STRUCT     transResult = {};
    EMV_SDI_CT_TRANSRES_STRUCT sdiExtra    = {};

    EMV_ADK_INFO ret = SDI_CT_ContinueOnline(
        reinterpret_cast<const uint8_t*>(arpc),
        static_cast<uint32_t>(arpcLen),
        scripts ? reinterpret_cast<const uint8_t*>(scripts) : nullptr,
        static_cast<uint32_t>(scriptsLen > 0 ? scriptsLen : 0),
        &transResult,
        &sdiExtra);

    if (ret != EMV_ADK_OK && ret != EMV_ADK_OFFLINE_APPROVE && ret != EMV_ADK_DECLINE) {
        LOG_PP_ERROR("PP_FinishChip: SDI_CT_ContinueOnline falhou: 0x%X", ret);
        g_pp_state = PPState::EMV_CT_OFFLINE;
        return mapEmvRetToAbecs(ret);
    }

    // Buscar tags para comprovante
    uint8_t  tlvBuf[MAX_TLV_BUF] = {};
    uint32_t tlvLen = sizeof(tlvBuf);
    SDI_fetchTxnTags(COMPROVANTE_TAGS, COMPROVANTE_TAGS_COUNT, tlvBuf, &tlvLen);

    // Serializar resultado completo
    pp_serialize_trans_result(&transResult, &sdiExtra, outBuf, outLen);

    int abecs_ret = (ret == EMV_ADK_DECLINE) ? PP_OFFLINE_DECLINE : PP_OFFLINE_APPROVE;

    LOG_PP_INFO("PP_FinishChip: resultado=%d cryptogramType=0x%02X",
                abecs_ret, transResult.CryptogramType);
    return abecs_ret;
}

int PP_EndChip(int idPP, int resultado) {
    (void)idPP;

    LOG_PP_INFO("PP_EndChip: encerrando transação CT, resultado=%d", resultado);

    if (!g_initialized) return PP_OK;

    uint32_t opts = (resultado != 0) ? EMV_ADK_TXN_ABORT : 0;
    SDI_CT_EndTransaction(opts);

    // Aguardar remoção do cartão (30s timeout conforme spec)
    libsdi::SDI::waitCardRemoval(30);

    // Reset de estado
    {
        std::lock_guard<std::mutex> lock(g_sdi_callback_mutex);
        g_pp_state    = PPState::IDLE;
        g_current_tec = TEC_NONE;
        g_pin_entered = false;
    }

    LOG_PP_INFO("PP_EndChip: transação encerrada");
    return PP_OK;
}

int PP_GetCard(int idPP, char* dadosCartao, int* tamDados) {
    (void)idPP;

    LOG_PP_INFO("PP_GetCard: lendo dados MSR");

    if (!g_initialized) return PP_ERR_INIT;
    if (g_pp_state != PPState::CARD_MSR) {
        LOG_PP_ERROR("PP_GetCard: estado incorreto: %d", static_cast<int>(g_pp_state));
        return PP_ERR_STATE;
    }
    if (!dadosCartao || !tamDados || *tamDados < 30) return PP_ERR_BUFFER;

    // Buscar tags MSR (já deveriam ter sido capturados no PP_CheckEvent)
    static const uint32_t msrTags[] = {
        TAG_TRACK2_ENC, TAG_TRACK1_ENC, TAG_MSR_KSN
    };
    constexpr uint32_t msrTagCount = sizeof(msrTags) / sizeof(msrTags[0]);

    uint8_t  tlvBuf[MAX_TLV_BUF] = {};
    uint32_t tlvLen = sizeof(tlvBuf);

    EMV_ADK_INFO ret = SDI_fetchTxnTags(msrTags, msrTagCount, tlvBuf, &tlvLen);
    if (ret != EMV_ADK_OK) {
        LOG_PP_ERROR("PP_GetCard: SDI_fetchTxnTags falhou: 0x%X", ret);
        return mapEmvRetToAbecs(ret);
    }

    ABECS_MSR_DATA msrData = {};
    pp_parse_tlv_to_msr(&msrData, tlvBuf, tlvLen);
    pp_serialize_msr_abecs(&msrData, dadosCartao, tamDados);

    LOG_PP_INFO("PP_GetCard: MSR lido, track1=%s track2=%s",
                msrData.track1_present ? "sim" : "não",
                msrData.track2_present ? "sim" : "não");
    return PP_OK;
}

}  // extern "C"
