// src/ppcomp_pin.cpp
// Descrição: Captura segura de PIN via PED (PIN Entry Device).
//
// DOMÍNIO DE SEGURANÇA:
//   A libppcomp NUNCA recebe o PIN em claro.
//   O PED do V660P-A é um enclave seguro.
//   O PIN block sai do SDI já criptografado DUKPT (ISO format 0, 8 bytes).
//   O KSN (10 bytes) identifica a chave de trabalho para o autorizador.
//   NUNCA logar conteúdo de pinBlock ou KSN.
//
// Mapeamento ABECS 2.20 → SDI:
//   PP_GetPIN()      → PED::sendPinInputParameters + PED::startPinInput (bloqueante)
//   PP_StartGetPIN() → PED::startPinEntry (assíncrono)
//   PP_PollGetPIN()  → PED::pollPinEntry
//   PP_AbortGetPIN() → PED::stopPinEntry
//   PP_GetPINBlock() → SdiCrypt::getEncryptedPin + SdiCrypt::getKeyInventory

#include <cstring>
#include <mutex>
#include <vector>

#include <sdiclient/sdi_emv.h>
#include <sdiclient/sdi_if.h>

#include "ppcomp.h"
#include "ppcomp_internal.h"
#include "pp_sdi_bridge.h"

extern "C" {

int PP_GetPIN(int idPP, int minDig, int maxDig, int timeout,
              int podeCancelar, int tipoVerif) {
    (void)idPP;

    LOG_PP_INFO("PP_GetPIN: minDig=%d maxDig=%d timeout=%d tipoVerif=%d",
                minDig, maxDig, timeout, tipoVerif);

    if (!g_initialized) return PP_ERR_INIT;

    // tipoVerif == PIN_VERIFICATION_OFFLINE:
    //   PIN offline é gerenciado internamente pelo kernel EMV durante
    //   SDI_CT_ContinueOffline. Não capturar via PED aqui.
    if (tipoVerif == PIN_VERIFICATION_OFFLINE) {
        LOG_PP_DEBUG("PP_GetPIN: PIN offline — kernel EMV gerencia internamente");
        g_pp_state = PPState::PIN;
        return PP_OK;
    }

    // tipoVerif == PIN_VERIFICATION_ONLINE: capturar via PED
    g_pp_state = PPState::PIN;

    // Configurar parâmetros de captura
    EMV_ADK_INFO ret = g_ped.setDefaultTimeout(
        static_cast<uint32_t>(timeout) * 1000);
    if (ret != EMV_ADK_OK) {
        LOG_PP_ERROR("PP_GetPIN: setDefaultTimeout falhou: 0x%X", ret);
    }

    ret = g_ped.sendPinInputParameters(
        static_cast<uint8_t>(minDig),
        static_cast<uint8_t>(maxDig),
        0);
    if (ret != EMV_ADK_OK) {
        LOG_PP_ERROR("PP_GetPIN: sendPinInputParameters falhou: 0x%X", ret);
        g_pp_state = PPState::EMV_CT_OFFLINE;
        return PP_ERR_PINPAD;
    }

    // Captura bloqueante — a thread aguarda até PIN ser digitado ou evento
    uint8_t cancelMode = (podeCancelar != 0) ? 0x01 : 0x00;
    int pedRet = g_ped.startPinInput(cancelMode);

    // Mapear resultado PED → ABECS
    switch (pedRet) {
        case EMV_PED_OK:
            {
                std::lock_guard<std::mutex> lock(g_sdi_callback_mutex);
                g_pin_entered = true;
            }
            LOG_PP_INFO("PP_GetPIN: PIN capturado com sucesso");
            return PP_OK;

        case EMV_PED_BYPASS:
            LOG_PP_INFO("PP_GetPIN: PIN bypass");
            g_pp_state = PPState::EMV_CT_OFFLINE;
            return PP_PIN_BYPASS;

        case EMV_PED_CANCEL:
            LOG_PP_INFO("PP_GetPIN: cancelado pelo usuário");
            g_pp_state = PPState::EMV_CT_OFFLINE;
            return PP_ABORT;

        case EMV_PED_TIMEOUT:
            LOG_PP_ERROR("PP_GetPIN: timeout");
            g_pp_state = PPState::EMV_CT_OFFLINE;
            return PP_ERR_TIMEOUT;

        default:
            // Verificar se pin está bloqueado (sinalizado pelo callback)
            {
                std::lock_guard<std::mutex> lock(g_sdi_callback_mutex);
                if (g_pin_blocked) {
                    LOG_PP_INFO("PP_GetPIN: PIN bloqueado");
                    g_pp_state = PPState::EMV_CT_OFFLINE;
                    return PP_ERR_PIN_BLOCKED;
                }
                if (g_pin_retry_count > 0) {
                    LOG_PP_INFO("PP_GetPIN: PIN incorreto (tentativa %d)", g_pin_retry_count);
                    g_pp_state = PPState::EMV_CT_OFFLINE;
                    return PP_ERR_WRONG_PIN;
                }
            }
            LOG_PP_ERROR("PP_GetPIN: erro PED desconhecido: %d", pedRet);
            g_pp_state = PPState::EMV_CT_OFFLINE;
            return PP_ERR_PIN;
    }
}

int PP_StartGetPIN(int idPP, int minDig, int maxDig, int timeout,
                   int podeCancelar, int tipoVerif) {
    (void)idPP;
    (void)tipoVerif;

    LOG_PP_INFO("PP_StartGetPIN: assíncrono, minDig=%d maxDig=%d", minDig, maxDig);

    if (!g_initialized) return PP_ERR_INIT;

    g_pp_state = PPState::PIN;

    g_ped.setDefaultTimeout(static_cast<uint32_t>(timeout) * 1000);
    g_ped.sendPinInputParameters(
        static_cast<uint8_t>(minDig),
        static_cast<uint8_t>(maxDig),
        0);

    uint8_t cancelMode = (podeCancelar != 0) ? 0x01 : 0x00;
    EMV_ADK_INFO ret = g_ped.startPinEntry(cancelMode);
    if (ret != EMV_ADK_OK) {
        LOG_PP_ERROR("PP_StartGetPIN: startPinEntry falhou: 0x%X", ret);
        g_pp_state = PPState::EMV_CT_OFFLINE;
        return PP_ERR_PINPAD;
    }

    return PP_OK;
}

int PP_PollGetPIN(int idPP) {
    (void)idPP;

    if (!g_initialized) return PP_ERR_INIT;
    if (g_pp_state != PPState::PIN) return PP_ERR_STATE;

    int pedRet = g_ped.pollPinEntry();

    switch (pedRet) {
        case EMV_PED_OK: {
            std::lock_guard<std::mutex> lock(g_sdi_callback_mutex);
            g_pin_entered = true;
            return PP_OK;
        }
        case EMV_PED_BYPASS:
            return PP_PIN_BYPASS;
        case EMV_PED_CANCEL:
            return PP_ABORT;
        case EMV_PED_TIMEOUT:
            return PP_ERR_TIMEOUT;
        default:
            // Ainda digitando — retornar "sem evento"
            if (pedRet < 0) return PP_ERR_NOEVENT;
            return PP_ERR_NOEVENT;
    }
}

int PP_AbortGetPIN(int idPP) {
    (void)idPP;

    LOG_PP_INFO("PP_AbortGetPIN");

    if (!g_initialized) return PP_OK;

    if (g_pp_state == PPState::PIN) {
        g_ped.stopPinEntry();
        g_pp_state = PPState::EMV_CT_OFFLINE;
    }

    return PP_OK;
}

int PP_GetPINBlock(int idPP, unsigned char* pinBlock, int* pinBlockLen,
                   unsigned char* ksn, int* ksnLen) {
    (void)idPP;

    // SEGURANÇA: NUNCA logar conteúdo de pinBlock ou KSN
    LOG_PP_INFO("PP_GetPINBlock: obtendo PIN block criptografado");

    if (!g_initialized) return PP_ERR_INIT;

    {
        std::lock_guard<std::mutex> lock(g_sdi_callback_mutex);
        if (!g_pin_entered) {
            LOG_PP_ERROR("PP_GetPINBlock: PIN não foi capturado");
            return PP_ERR_INIT;
        }
    }

    if (!pinBlock || !pinBlockLen || *pinBlockLen < static_cast<int>(PIN_BLOCK_LEN)) {
        return PP_ERR_BUFFER;
    }
    if (!ksn || !ksnLen || *ksnLen < static_cast<int>(KSN_LEN)) {
        return PP_ERR_BUFFER;
    }

    libsdi::SdiCrypt crypt;

    // Obter PIN block (8 bytes ISO format 0, DUKPT)
    std::vector<uint8_t> encPin;
    EMV_ADK_INFO ret = crypt.getEncryptedPin(encPin);
    if (ret != EMV_ADK_OK) {
        LOG_PP_ERROR("PP_GetPINBlock: getEncryptedPin falhou: 0x%X", ret);
        return PP_ERR_PINPAD;
    }
    if (encPin.size() < PIN_BLOCK_LEN) {
        LOG_PP_ERROR("PP_GetPINBlock: PIN block tamanho inválido: %zu", encPin.size());
        return PP_ERR_PINPAD;
    }

    memcpy(pinBlock, encPin.data(), PIN_BLOCK_LEN);
    *pinBlockLen = PIN_BLOCK_LEN;
    // NUNCA logar os bytes do pinBlock

    // Obter KSN (10 bytes DUKPT)
    std::vector<uint8_t> inventory;
    ret = crypt.getKeyInventory(inventory);
    if (ret != EMV_ADK_OK) {
        LOG_PP_ERROR("PP_GetPINBlock: getKeyInventory falhou: 0x%X", ret);
        return PP_ERR_PINPAD;
    }

    pp_extract_ksn_from_inventory(inventory, ksn, ksnLen);
    // NUNCA logar o KSN

    LOG_PP_INFO("PP_GetPINBlock: PIN block obtido (pinBlockLen=%d ksnLen=%d)",
                *pinBlockLen, *ksnLen);
    return PP_OK;
}

}  // extern "C"
