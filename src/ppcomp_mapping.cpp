// src/ppcomp_mapping.cpp
// Descrição: Mapeamento bidirecional entre códigos/structs SDI (Verifone ADK)
//            e o formato ABECS 2.20 (Software Express).
//
// Mapeamento ABECS 2.20 → SDI:
//   mapEmvRetToAbecs()       → converte EMV_ADK_INFO → PP_*
//   mapTxnTypeAbecsToEmv()   → converte tipo de transação ABECS → tag 9C EMV
//   mapModalidadeToTec()     → converte bitmask ABECS → bitmask SDI
//   pp_serialize_*()         → converte structs SDI → buffer ABECS
//   pp_parse_tlv_to_msr()    → extrai ABECS_MSR_DATA de TLV SDI
//   pp_tlv_*()               → utilitários TLV

#include <cstring>
#include <cstdint>
#include <cstdio>
#include <mutex>

#include <sdiclient/sdi_emv.h>
#include <sdiclient/sdi_if.h>

#include "ppcomp.h"
#include "ppcomp_internal.h"
#include "pp_sdi_bridge.h"

// Buffer global para script results (protegido por g_sdi_callback_mutex)
static uint8_t s_script_results[MAX_SCRIPT_RESULTS][64];
static uint8_t s_script_result_lens[MAX_SCRIPT_RESULTS];
static int     s_script_count = 0;

// ─── Mapeamento de códigos de retorno ────────────────────────────────────────

int mapEmvRetToAbecs(EMV_ADK_INFO emvRet) {
    switch (emvRet) {
        case EMV_ADK_OK:             return PP_OK;
        case EMV_ADK_GO_ONLINE:      return PP_GO_ONLINE;
        // EMV_ADK_ONLINE é alias de EMV_ADK_GO_ONLINE (mesmo valor 0x01)
        case EMV_ADK_OFFLINE_APPROVE: return PP_OFFLINE_APPROVE;
        case EMV_ADK_DECLINE:        return PP_OFFLINE_DECLINE;
        case EMV_ADK_NO_CARD:        return PP_ERR_NOEVENT;
        case EMV_ADK_FALLBACK:       return PP_FALLBACK_CT;
        case EMV_ADK_MULTI_APP:      return PP_MULTIAPP;
        case EMV_ADK_TXN_ABORT:      return PP_ABORT;
        case EMV_ADK_TXN_CTLS_MOBILE: return PP_CTLS_WAITING;
        case EMV_ADK_WRONG_PIN:      return PP_ERR_WRONG_PIN;
        case EMV_ADK_PIN_BLOCKED:    return PP_ERR_PIN_BLOCKED;
        case EMV_ADK_CARD_BLOCKED:   return PP_ERR_CARD_BLOCKED;
        case EMV_ADK_COMM_ERROR:     return PP_ERR_PINPAD;   // crítico: erro 31
        case EMV_ADK_TIMEOUT:        return PP_ERR_TIMEOUT;
        case EMV_ADK_USER_CANCEL:    return PP_ABORT;
        case EMV_ADK_INTERNAL_ERROR: return PP_ERR_PINPAD;   // crítico: erro 31
        case EMV_ADK_PARAM_ERROR:    return PP_ERR_PARAM;
        default:
            LOG_PP_ERROR("mapEmvRetToAbecs: unmapped SDI code 0x%X", emvRet);
            return PP_ERR_GENERIC;
    }
}

uint8_t mapTxnTypeAbecsToEmv(int abecsTxnType) {
    // Mapeamento conforme ABECS 2.20 tabela T2_TIPOTR → tag 9C EMV
    switch (abecsTxnType) {
        case 0x01: return EMV_TXN_PURCHASE;   // Compra
        case 0x02: return EMV_TXN_CASH;        // Saque/Cash
        case 0x03: return EMV_TXN_INQUIRY;     // Consulta saldo
        case 0x04: return EMV_TXN_REFUND;      // Estorno
        case 0x09: return EMV_TXN_CASHBACK;    // Compra + Cashback
        case 0x0A: return EMV_TXN_PREAUTH;     // Pré-autorização
        default:
            LOG_PP_ERROR("mapTxnTypeAbecsToEmv: unknown type 0x%02X, using PURCHASE", abecsTxnType);
            return EMV_TXN_PURCHASE;
    }
}

uint8_t mapModalidadeToTec(int modalidades) {
    uint8_t tec = TEC_NONE;
    if (modalidades & 0x01) tec |= TEC_CHIP_CT;
    if (modalidades & 0x02) tec |= TEC_CTLS;
    if (modalidades & 0x04) tec |= TEC_MSR;
    return tec;
}

// ─── Utilitários TLV ─────────────────────────────────────────────────────────

uint8_t pp_tlv_get_byte(const uint8_t* tlv, uint32_t tlvLen, uint32_t tag) {
    uint8_t out = 0;
    uint32_t outLen = 1;
    pp_tlv_get_bytes(tlv, tlvLen, tag, &out, &outLen);
    return out;
}

bool pp_tlv_get_bytes(const uint8_t* tlv, uint32_t tlvLen, uint32_t tag,
                       uint8_t* out, uint32_t* outLen) {
    if (!tlv || tlvLen == 0 || !out || !outLen) return false;

    uint32_t pos = 0;
    while (pos < tlvLen) {
        // Decode tag
        uint32_t currentTag = 0;
        uint8_t tagByte = tlv[pos++];
        currentTag = tagByte;

        if ((tagByte & 0x1F) == 0x1F) {
            // Multi-byte tag
            do {
                if (pos >= tlvLen) return false;
                tagByte = tlv[pos++];
                currentTag = (currentTag << 8) | tagByte;
            } while (tagByte & 0x80);
        }

        if (pos >= tlvLen) break;

        // Decode length
        uint32_t len = tlv[pos++];
        if (len == 0x81) {
            if (pos >= tlvLen) return false;
            len = tlv[pos++];
        } else if (len == 0x82) {
            if (pos + 1 >= tlvLen) return false;
            len = ((uint32_t)tlv[pos] << 8) | tlv[pos + 1];
            pos += 2;
        }

        if (pos + len > tlvLen) break;

        if (currentTag == tag) {
            uint32_t copyLen = (len < *outLen) ? len : *outLen;
            memcpy(out, tlv + pos, copyLen);
            *outLen = copyLen;
            return true;
        }

        pos += len;
    }
    return false;
}

// ─── Serialização: startResult → buffer ABECS ────────────────────────────────

void pp_serialize_start_result(const EMV_CT_STARTRESULT_STRUCT* startResult,
                                char* outBuf, int* outLen) {
    if (!startResult || !outBuf || !outLen || *outLen < 4) {
        if (outLen) *outLen = 0;
        return;
    }

    uint8_t* buf = reinterpret_cast<uint8_t*>(outBuf);
    int pos = 0;

    // Formato ABECS: [numCandidates(1)] [AID_len(1) AID(N) label(16)]...
    buf[pos++] = startResult->NumCandidates;

    for (int i = 0; i < startResult->NumCandidates && pos + 33 < *outLen; i++) {
        const EMV_CT_CANDIDATE_STRUCT& c = startResult->Candidates[i];
        buf[pos++] = c.AIDLen;
        memcpy(&buf[pos], c.AID, c.AIDLen);
        pos += c.AIDLen;
        // App label: 16 bytes, zero-padded
        memset(&buf[pos], 0, MAX_APP_LABEL);
        strncpy(reinterpret_cast<char*>(&buf[pos]), c.AppLabel, MAX_APP_LABEL);
        pos += MAX_APP_LABEL;
        buf[pos++] = c.Priority;
    }

    *outLen = pos;
}

// ─── Serialização: transResult CT → buffer ABECS ─────────────────────────────

void pp_serialize_trans_result(const EMV_CT_TRANSRES_STRUCT* ct,
                                const EMV_SDI_CT_TRANSRES_STRUCT* sdi,
                                char* outBuf, int* outLen) {
    if (!ct || !outBuf || !outLen || *outLen < 128) {
        if (outLen) *outLen = 0;
        return;
    }

    uint8_t* buf = reinterpret_cast<uint8_t*>(outBuf);
    int pos = 0;

    // Cryptogram (8 bytes)
    memcpy(&buf[pos], ct->Cryptogram, 8);
    pos += 8;

    // Cryptogram type (1 byte)
    buf[pos++] = ct->CryptogramType;

    // ATC (2 bytes)
    memcpy(&buf[pos], ct->ATC, 2);
    pos += 2;

    // TVR (5 bytes)
    memcpy(&buf[pos], ct->TVR, 5);
    pos += 5;

    // TSI (2 bytes)
    memcpy(&buf[pos], ct->TSI, 2);
    pos += 2;

    // IAD: [len(1) data(N)]
    buf[pos++] = ct->IADLen;
    if (ct->IADLen > 0) {
        memcpy(&buf[pos], ct->IAD, ct->IADLen);
        pos += ct->IADLen;
    }

    // AID selecionado: [len(1) data(N)]
    buf[pos++] = ct->AIDSelectedLen;
    memcpy(&buf[pos], ct->AIDSelected, ct->AIDSelectedLen);
    pos += ct->AIDSelectedLen;

    // App label: 16 bytes zero-padded
    memset(&buf[pos], 0, MAX_APP_LABEL);
    strncpy(reinterpret_cast<char*>(&buf[pos]), ct->AppLabel, MAX_APP_LABEL);
    pos += MAX_APP_LABEL;

    // Unpredictable number (4 bytes)
    memcpy(&buf[pos], ct->UnpredictableNumber, 4);
    pos += 4;

    // Amount authorized (4 bytes big-endian)
    buf[pos++] = (ct->AuthorizedAmount >> 24) & 0xFF;
    buf[pos++] = (ct->AuthorizedAmount >> 16) & 0xFF;
    buf[pos++] = (ct->AuthorizedAmount >>  8) & 0xFF;
    buf[pos++] = (ct->AuthorizedAmount >>  0) & 0xFF;

    // Transaction date (3 bytes YYMMDD)
    memcpy(&buf[pos], ct->TransactionDate, 3);
    pos += 3;

    // Transaction type (1 byte)
    buf[pos++] = ct->TransactionType;

    // PAN masked (from SDI) — pode ser NULL em mocks
    if (sdi) {
        buf[pos++] = sdi->MaskedPANLen;
        memcpy(&buf[pos], sdi->MaskedPAN, sdi->MaskedPANLen);
        pos += sdi->MaskedPANLen;
        buf[pos++] = sdi->PINWasBypassed ? 0x01 : 0x00;
    } else {
        buf[pos++] = 0;  // no PAN
        buf[pos++] = 0;  // no bypass flag
    }

    // Script result
    {
        std::lock_guard<std::mutex> lock(g_sdi_callback_mutex);
        buf[pos++] = (s_script_count > 0) ? 0x01 : 0x00;
        if (s_script_count > 0) {
            buf[pos++] = s_script_result_lens[0];
            memcpy(&buf[pos], s_script_results[0], s_script_result_lens[0]);
            pos += s_script_result_lens[0];
        }
    }

    *outLen = pos;
}

// ─── Serialização: transResult CTLS → buffer ABECS ───────────────────────────

void pp_serialize_ctls_result(const EMV_CTLS_TRANSRES_STRUCT* ctls,
                               const EMV_SDI_CTLS_TRANSRES_STRUCT* sdi,
                               char* outBuf, int* outLen) {
    if (!ctls || !outBuf || !outLen || *outLen < 80) {
        if (outLen) *outLen = 0;
        return;
    }

    uint8_t* buf = reinterpret_cast<uint8_t*>(outBuf);
    int pos = 0;

    memcpy(&buf[pos], ctls->Cryptogram, 8);      pos += 8;
    buf[pos++] = ctls->CryptogramType;
    memcpy(&buf[pos], ctls->ATC, 2);              pos += 2;
    memcpy(&buf[pos], ctls->TVR, 5);              pos += 5;

    buf[pos++] = ctls->AIDSelectedLen;
    memcpy(&buf[pos], ctls->AIDSelected, ctls->AIDSelectedLen);
    pos += ctls->AIDSelectedLen;

    memset(&buf[pos], 0, MAX_APP_LABEL);
    strncpy(reinterpret_cast<char*>(&buf[pos]), ctls->AppLabel, MAX_APP_LABEL);
    pos += MAX_APP_LABEL;

    buf[pos++] = (ctls->AuthorizedAmount >> 24) & 0xFF;
    buf[pos++] = (ctls->AuthorizedAmount >> 16) & 0xFF;
    buf[pos++] = (ctls->AuthorizedAmount >>  8) & 0xFF;
    buf[pos++] = (ctls->AuthorizedAmount >>  0) & 0xFF;

    memcpy(&buf[pos], ctls->TransactionDate, 3);  pos += 3;
    buf[pos++] = ctls->TransactionType;
    buf[pos++] = ctls->KernelID;
    buf[pos++] = ctls->IsContactlessMobile ? 0x01 : 0x00;

    if (sdi) {
        buf[pos++] = sdi->MaskedPANLen;
        memcpy(&buf[pos], sdi->MaskedPAN, sdi->MaskedPANLen);
        pos += sdi->MaskedPANLen;
    } else {
        buf[pos++] = 0;
    }

    *outLen = pos;
}

// ─── Serialização ABECS_TRANSRESULT ──────────────────────────────────────────

void pp_abecs_serialize(const ABECS_TRANSRESULT* result, char* outBuf, int* outLen) {
    if (!result || !outBuf || !outLen || *outLen < 128) {
        if (outLen) *outLen = 0;
        return;
    }

    uint8_t* buf = reinterpret_cast<uint8_t*>(outBuf);
    int pos = 0;

    memcpy(&buf[pos], result->cryptogram, 8);           pos += 8;
    buf[pos++] = result->cryptogram_type;
    memcpy(&buf[pos], result->atc, 2);                  pos += 2;
    memcpy(&buf[pos], result->tvr, 5);                  pos += 5;
    memcpy(&buf[pos], result->tsi, 2);                  pos += 2;
    buf[pos++] = result->iad_len;
    memcpy(&buf[pos], result->iad, result->iad_len);    pos += result->iad_len;
    buf[pos++] = result->pan_masked_len;
    memcpy(&buf[pos], result->pan_masked, result->pan_masked_len);
    pos += result->pan_masked_len;
    buf[pos++] = result->pan_seq;
    buf[pos++] = result->aid_selected_len;
    memcpy(&buf[pos], result->aid_selected, result->aid_selected_len);
    pos += result->aid_selected_len;
    buf[pos++] = result->app_label_len;
    memcpy(&buf[pos], result->app_label, result->app_label_len);
    pos += result->app_label_len;
    memcpy(&buf[pos], result->unpred_number, 4);        pos += 4;
    buf[pos++] = (result->amount_auth >> 24) & 0xFF;
    buf[pos++] = (result->amount_auth >> 16) & 0xFF;
    buf[pos++] = (result->amount_auth >>  8) & 0xFF;
    buf[pos++] = (result->amount_auth >>  0) & 0xFF;
    memcpy(&buf[pos], result->txn_date, 3);             pos += 3;
    buf[pos++] = result->txn_type;

    if (result->has_script_result) {
        buf[pos++] = 0x01;
        buf[pos++] = result->script_result_len;
        memcpy(&buf[pos], result->script_result, result->script_result_len);
        pos += result->script_result_len;
    } else {
        buf[pos++] = 0x00;
    }

    *outLen = pos;
}

// ─── Parse de TLV MSR ────────────────────────────────────────────────────────

void pp_parse_tlv_to_msr(ABECS_MSR_DATA* msr, const uint8_t* tlv, uint32_t tlvLen) {
    if (!msr || !tlv) return;
    memset(msr, 0, sizeof(ABECS_MSR_DATA));

    // Track 2 enc (tag 0xDFAB21)
    uint32_t len = sizeof(msr->track2_enc);
    if (pp_tlv_get_bytes(tlv, tlvLen, TAG_TRACK2_ENC, msr->track2_enc, &len)) {
        msr->track2_enc_len = static_cast<uint8_t>(len);
        msr->track2_present = (len > 0);
    }

    // Track 1 enc (tag 0xDFAB20)
    len = sizeof(msr->track1_enc);
    if (pp_tlv_get_bytes(tlv, tlvLen, TAG_TRACK1_ENC, msr->track1_enc, &len)) {
        msr->track1_enc_len = static_cast<uint8_t>(len);
        msr->track1_present = (len > 0);
    }

    // KSN (tag 0xDFAB22)
    len = KSN_LEN;
    pp_tlv_get_bytes(tlv, tlvLen, TAG_MSR_KSN, msr->ksn, &len);
}

// ─── Serialização MSR → buffer ABECS ────────────────────────────────────────

void pp_serialize_msr_abecs(const ABECS_MSR_DATA* msr, char* outBuf, int* outLen) {
    if (!msr || !outBuf || !outLen || *outLen < 20) {
        if (outLen) *outLen = 0;
        return;
    }

    uint8_t* buf = reinterpret_cast<uint8_t*>(outBuf);
    int pos = 0;

    // [flags(1)] [track1_len(1) track1_enc(N)] [track2_len(1) track2_enc(N)] [ksn(10)]
    uint8_t flags = 0;
    if (msr->track1_present) flags |= 0x01;
    if (msr->track2_present) flags |= 0x02;
    buf[pos++] = flags;

    buf[pos++] = msr->track1_enc_len;
    if (msr->track1_enc_len > 0 && pos + msr->track1_enc_len < *outLen) {
        memcpy(&buf[pos], msr->track1_enc, msr->track1_enc_len);
        pos += msr->track1_enc_len;
    }

    buf[pos++] = msr->track2_enc_len;
    if (msr->track2_enc_len > 0 && pos + msr->track2_enc_len < *outLen) {
        memcpy(&buf[pos], msr->track2_enc, msr->track2_enc_len);
        pos += msr->track2_enc_len;
    }

    memcpy(&buf[pos], msr->ksn, KSN_LEN);
    pos += KSN_LEN;

    *outLen = pos;
}

// ─── KSN extraction ─────────────────────────────────────────────────────────

void pp_extract_ksn_from_inventory(const std::vector<uint8_t>& inventory,
                                    uint8_t* ksn, int* ksnLen) {
    if (!ksn || !ksnLen) return;
    // O inventário de chaves SDI contém o KSN nos últimos 10 bytes.
    // Formato exato depende do firmware — aqui usamos a convenção Verifone ADK 5.x.
    if (inventory.size() >= KSN_LEN) {
        size_t offset = inventory.size() - KSN_LEN;
        memcpy(ksn, inventory.data() + offset, KSN_LEN);
        *ksnLen = KSN_LEN;
    } else {
        memset(ksn, 0, KSN_LEN);
        *ksnLen = 0;
    }
}

// ─── Script results ──────────────────────────────────────────────────────────

void pp_save_script_result(const uint8_t* data, uint32_t len) {
    std::lock_guard<std::mutex> lock(g_sdi_callback_mutex);
    if (s_script_count >= static_cast<int>(MAX_SCRIPT_RESULTS)) {
        LOG_PP_ERROR("pp_save_script_result: buffer cheio, descartando script result");
        return;
    }
    uint32_t copyLen = (len < 64) ? len : 64;
    memcpy(s_script_results[s_script_count], data, copyLen);
    s_script_result_lens[s_script_count] = static_cast<uint8_t>(copyLen);
    s_script_count++;
}
