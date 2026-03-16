// src/ppcomp_detection.cpp
// Descrição: Technology Selection — detecção de tecnologia de cartão.
//
// Mapeamento ABECS 2.20 → SDI:
//   PP_StartCheckEvent()  → CardDetection::startSelection (DETECTION_MODE_BLOCKING)
//   PP_CheckEvent()       → CardDetection::pollTechnology
//   PP_AbortCheckEvent()  → CardDetection::stopSelection
//
// Nota: DETECTION_MODE_BLOCKING é usado pois a libclisitef usa modelo poll
// (PP_StartCheckEvent + PP_CheckEvent em loop). O SDI gerencia RF internamente.

#include <cstring>
#include <mutex>

#include <sdiclient/sdi_emv.h>
#include <sdiclient/sdi_if.h>

#include "ppcomp.h"
#include "ppcomp_internal.h"
#include "pp_sdi_bridge.h"

extern "C" {

int PP_StartCheckEvent(int idPP, int modalidades, int timeout, int podeCancelar) {
    (void)idPP;
    (void)podeCancelar;

    LOG_PP_INFO("PP_StartCheckEvent: modalidades=0x%02X timeout=%ds",
                modalidades, timeout);

    if (!g_initialized) return PP_ERR_INIT;
    if (!g_tables_loaded) {
        LOG_PP_ERROR("PP_StartCheckEvent: tabelas não carregadas");
        return PP_ERR_STATE;
    }
    if (g_pp_state != PPState::IDLE) {
        LOG_PP_ERROR("PP_StartCheckEvent: estado incorreto: %d",
                     static_cast<int>(g_pp_state));
        return PP_ERR_STATE;
    }

    // Mapear bitmask ABECS → bitmask SDI
    uint8_t tecMask = mapModalidadeToTec(modalidades);
    if (tecMask == TEC_NONE) {
        LOG_PP_ERROR("PP_StartCheckEvent: nenhuma modalidade válida");
        return PP_ERR_PARAM;
    }

    // Flags CT: tentar PPS e detectar ATR errado
    uint32_t ctFlags = static_cast<uint32_t>(EMV_CT_TRY_PPS | EMV_CT_DETECT_WRONG_ATR);

    EMV_ADK_INFO ret = g_cardDetection.startSelection(
        tecMask,
        libsdi::DetectionMode::BLOCKING,
        static_cast<uint32_t>(timeout) * 1000,  // seconds → ms
        ctFlags);

    if (ret != EMV_ADK_OK) {
        LOG_PP_ERROR("PP_StartCheckEvent: startSelection falhou: 0x%X", ret);
        return mapEmvRetToAbecs(ret);
    }

    {
        std::lock_guard<std::mutex> lock(g_sdi_callback_mutex);
        g_detection_active = true;
        g_card_removed_mid_txn = false;
    }
    g_pp_state = PPState::DETECTING;

    LOG_PP_INFO("PP_StartCheckEvent: detecção iniciada, tecMask=0x%02X", tecMask);
    return PP_OK;
}

int PP_CheckEvent(int idPP, int* tecnologia, char* dadosCartao, int* tamDados) {
    (void)idPP;

    if (!g_initialized) return PP_ERR_INIT;
    if (!tecnologia) return PP_ERR_PARAM;

    if (g_pp_state != PPState::DETECTING) {
        LOG_PP_ERROR("PP_CheckEvent: não está em detecção, estado=%d",
                     static_cast<int>(g_pp_state));
        return PP_ERR_STATE;
    }

    libsdi::Technology tech = g_cardDetection.pollTechnology();

    switch (tech) {
        case libsdi::Technology::NONE:
            return PP_ERR_NOEVENT;  // Sem cartão ainda

        case libsdi::Technology::CHIP_CT:
            LOG_PP_INFO("PP_CheckEvent: cartão CT detectado");
            *tecnologia = 0x01;  // ABECS: chip contato
            g_current_tec = TEC_CHIP_CT;
            g_pp_state = PPState::CARD_CT;
            {
                std::lock_guard<std::mutex> lock(g_sdi_callback_mutex);
                g_detection_active = false;
            }
            return PP_OK;

        case libsdi::Technology::CTLS:
            LOG_PP_INFO("PP_CheckEvent: cartão CTLS detectado");
            *tecnologia = 0x02;  // ABECS: contactless
            g_current_tec = TEC_CTLS;
            g_pp_state = PPState::CARD_CTLS;
            {
                std::lock_guard<std::mutex> lock(g_sdi_callback_mutex);
                g_detection_active = false;
            }
            return PP_OK;

        case libsdi::Technology::MSR: {
            LOG_PP_INFO("PP_CheckEvent: cartão MSR detectado");

            // Obter tags MSR via SDI_fetchTxnTags
            static const uint32_t msrTags[] = {
                TAG_TRACK2_DATA, TAG_PAN, TAG_CARDHOLDER_NAME, TAG_EXPIRY_DATE,
                TAG_TRACK2_EQUIV, TAG_TRACK1_ENC, TAG_TRACK2_ENC, TAG_MSR_KSN
            };
            constexpr uint32_t msrTagCount = sizeof(msrTags) / sizeof(msrTags[0]);

            uint8_t  tlvBuf[MAX_TLV_BUF] = {};
            uint32_t tlvLen = sizeof(tlvBuf);

            EMV_ADK_INFO ret = SDI_fetchTxnTags(msrTags, msrTagCount, tlvBuf, &tlvLen);
            if (ret != EMV_ADK_OK) {
                LOG_PP_ERROR("PP_CheckEvent: SDI_fetchTxnTags MSR falhou: 0x%X", ret);
                return mapEmvRetToAbecs(ret);
            }

            // Parse TLV → ABECS_MSR_DATA
            ABECS_MSR_DATA msrData = {};
            pp_parse_tlv_to_msr(&msrData, tlvBuf, tlvLen);

            // Serializar para buffer ABECS
            if (dadosCartao && tamDados) {
                pp_serialize_msr_abecs(&msrData, dadosCartao, tamDados);
            }

            *tecnologia = 0x04;  // ABECS: MSR
            g_current_tec = TEC_MSR;
            g_pp_state = PPState::CARD_MSR;
            {
                std::lock_guard<std::mutex> lock(g_sdi_callback_mutex);
                g_detection_active = false;
            }
            return PP_OK;
        }

        default:
            LOG_PP_ERROR("PP_CheckEvent: tecnologia desconhecida: %d",
                         static_cast<int>(tech));
            return PP_ERR_PINPAD;
    }
}

int PP_AbortCheckEvent(int idPP) {
    (void)idPP;

    LOG_PP_INFO("PP_AbortCheckEvent");

    if (!g_initialized) return PP_OK;

    if (g_pp_state == PPState::DETECTING) {
        g_cardDetection.stopSelection();
        g_pp_state = PPState::IDLE;
        {
            std::lock_guard<std::mutex> lock(g_sdi_callback_mutex);
            g_detection_active = false;
        }
    }

    return PP_OK;
}

}  // extern "C"
