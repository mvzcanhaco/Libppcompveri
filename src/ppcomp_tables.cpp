// src/ppcomp_tables.cpp
// Descrição: Carga de tabelas EMV no SDI Server (T1, T2, T3 ABECS 2.20).
//
// Mapeamento ABECS 2.20 → SDI:
//   PP_SetTermData()    → SDI_CT_SetTermData + SDI_CTLS_SetTermData
//   PP_LoadAIDTable()   → SDI_CT_SetAppliData × N + SDI_CTLS_SetAppliDataSchemeSpecific × N
//   PP_LoadCAPKTable()  → SDI_CT_StoreCAPKey × N + SDI_CTLS_StoreCAPKey × N
//                         + SDI_CT/CTLS_ApplyConfiguration() [OBRIGATÓRIO]

#include <cstring>
#include <mutex>

#include <sdiclient/sdi_emv.h>
#include <sdiclient/sdi_if.h>

#include "ppcomp.h"
#include "ppcomp_internal.h"
#include "pp_sdi_bridge.h"
#include "tables/table_loader.h"

// Buffers estáticos para tabelas parseadas (evita alocação dinâmica)
static ABECS_TERMDATA_STRUCT  s_termdata;
static ABECS_AID_RECORD       s_aids[MAX_AID_RECORDS];
static ABECS_CAPK_RECORD      s_capks[MAX_CAPK_RECORDS];
static int                    s_aid_count  = 0;
static int                    s_capk_count = 0;

// ─── Conversão ABECS → SDI structs ───────────────────────────────────────────

static void abecs_to_sdi_termdata_ct(const ABECS_TERMDATA_STRUCT* abecs,
                                      EMV_CT_TERMDATA_STRUCT* sdi) {
    memset(sdi, 0, sizeof(EMV_CT_TERMDATA_STRUCT));
    sdi->TerminalType = abecs->terminal_type;
    memcpy(sdi->TerminalCapabilities, abecs->terminal_capabilities, 3);
    memcpy(sdi->AdditionalTerminalCapabilities, abecs->add_terminal_cap, 5);
    sdi->TransactionCurrencyCode     = CURRENCY_BRL_CODE;  // hardcoded BRL
    sdi->TransactionCurrencyExponent = CURRENCY_BRL_EXP;
    sdi->TerminalCountryCode         = COUNTRY_BRAZIL_CODE;
    sdi->FloorLimit                  = 0;  // floor limit é por AID
}

static void abecs_to_sdi_termdata_ctls(const ABECS_TERMDATA_STRUCT* abecs,
                                        EMV_CTLS_TERMDATA_STRUCT* sdi) {
    memset(sdi, 0, sizeof(EMV_CTLS_TERMDATA_STRUCT));
    sdi->TerminalType = abecs->terminal_type;
    memcpy(sdi->TerminalCapabilities, abecs->terminal_capabilities, 3);
    memcpy(sdi->AdditionalTerminalCapabilities, abecs->add_terminal_cap, 5);
    sdi->TransactionCurrencyCode     = CURRENCY_BRL_CODE;
    sdi->TransactionCurrencyExponent = CURRENCY_BRL_EXP;
    sdi->TerminalCountryCode         = COUNTRY_BRAZIL_CODE;
    sdi->CTLSTransactionLimit        = abecs->ctls_txn_limit;
    sdi->CTLSFloorLimit              = abecs->ctls_floor_limit;
    sdi->CTLSNoPINLimit              = abecs->ctls_nopin_limit;
}

static void abecs_to_sdi_aid_ct(const ABECS_AID_RECORD* rec, EMV_CT_APPLI_DATA_STRUCT* sdi,
                                  uint32_t* flags, bool isFirst) {
    memset(sdi, 0, sizeof(EMV_CT_APPLI_DATA_STRUCT));
    memcpy(sdi->AID, rec->aid, rec->aid_len);
    sdi->AIDLen = rec->aid_len;
    memcpy(sdi->TACDenial,  rec->tac_denial,  5);
    memcpy(sdi->TACOnline,  rec->tac_online,  5);
    memcpy(sdi->TACDefault, rec->tac_default, 5);
    sdi->FloorLimit = rec->floor_limit_ct;
    if (rec->default_ddol_len > 0) {
        memcpy(sdi->DefaultDDOL, rec->default_ddol, rec->default_ddol_len);
        sdi->DefaultDDOLLen = rec->default_ddol_len;
    }
    if (rec->default_tdol_len > 0) {
        memcpy(sdi->DefaultTDOL, rec->default_tdol, rec->default_tdol_len);
        sdi->DefaultTDOLLen = rec->default_tdol_len;
    }
    // Primeiro AID deve limpar configuração anterior do SDI
    *flags = isFirst ? EMV_ADK_CLEAR_ALL_RECORDS : 0;
}

static void abecs_to_sdi_aid_ctls(const ABECS_AID_RECORD* rec,
                                    EMV_CTLS_APPLI_DATA_STRUCT* sdi,
                                    uint32_t* flags, bool isFirst) {
    memset(sdi, 0, sizeof(EMV_CTLS_APPLI_DATA_STRUCT));
    memcpy(sdi->AID, rec->aid, rec->aid_len);
    sdi->AIDLen   = rec->aid_len;
    sdi->KernelID = rec->kernel_id;
    memcpy(sdi->TACDenial,  rec->tac_denial,  5);
    memcpy(sdi->TACOnline,  rec->tac_online,  5);
    memcpy(sdi->TACDefault, rec->tac_default, 5);
    *flags = isFirst ? EMV_ADK_CLEAR_ALL_RECORDS : 0;
}

static void abecs_to_sdi_capk_ct(const ABECS_CAPK_RECORD* rec, EMV_CT_CAPKEY_STRUCT* sdi) {
    memset(sdi, 0, sizeof(EMV_CT_CAPKEY_STRUCT));
    memcpy(sdi->RID, rec->rid, 5);
    sdi->Index     = rec->index;
    sdi->Algorithm = rec->algorithm;
    memcpy(sdi->Modulus, rec->modulus, rec->modulus_len);
    sdi->ModulusLen = rec->modulus_len;
    memcpy(sdi->Exponent, rec->exponent, rec->exponent_len);
    sdi->ExponentLen = rec->exponent_len;
    memcpy(sdi->Checksum, rec->checksum, 20);
}

static void abecs_to_sdi_capk_ctls(const ABECS_CAPK_RECORD* rec, EMV_CTLS_CAPKEY_STRUCT* sdi) {
    memset(sdi, 0, sizeof(EMV_CTLS_CAPKEY_STRUCT));
    memcpy(sdi->RID, rec->rid, 5);
    sdi->Index     = rec->index;
    sdi->Algorithm = rec->algorithm;
    memcpy(sdi->Modulus, rec->modulus, rec->modulus_len);
    sdi->ModulusLen = rec->modulus_len;
    memcpy(sdi->Exponent, rec->exponent, rec->exponent_len);
    sdi->ExponentLen = rec->exponent_len;
    memcpy(sdi->Checksum, rec->checksum, 20);
}

// ─── API ABECS ────────────────────────────────────────────────────────────────

extern "C" {

int PP_SetTermData(int idPP, char* buffer, int tamBuf) {
    (void)idPP;

    LOG_PP_INFO("PP_SetTermData: configurando terminal");

    if (!g_initialized) return PP_ERR_INIT;
    if (!buffer || tamBuf <= 0) return PP_ERR_PARAM;

    // Parse do formato ABECS
    int consumed = table_parse_termdata(
        reinterpret_cast<const uint8_t*>(buffer), tamBuf, &s_termdata);
    if (consumed < 0) {
        LOG_PP_ERROR("PP_SetTermData: parse falhou");
        return PP_ERR_PARAM;
    }

    // Enviar ao SDI
    EMV_CT_TERMDATA_STRUCT   ctTermData   = {};
    EMV_CTLS_TERMDATA_STRUCT ctlsTermData = {};
    abecs_to_sdi_termdata_ct(&s_termdata, &ctTermData);
    abecs_to_sdi_termdata_ctls(&s_termdata, &ctlsTermData);

    EMV_ADK_INFO ret = SDI_CT_SetTermData(&ctTermData);
    if (ret != EMV_ADK_OK) {
        LOG_PP_ERROR("PP_SetTermData: SDI_CT_SetTermData falhou: 0x%X", ret);
        return PP_ERR_PARAM;
    }

    ret = SDI_CTLS_SetTermData(&ctlsTermData);
    if (ret != EMV_ADK_OK) {
        LOG_PP_ERROR("PP_SetTermData: SDI_CTLS_SetTermData falhou: 0x%X", ret);
        return PP_ERR_PARAM;
    }

    LOG_PP_INFO("PP_SetTermData: terminal configurado, tipo=0x%02X", s_termdata.terminal_type);
    return PP_OK;
}

int PP_LoadAIDTable(int idPP, char* buffer, int tamBuf) {
    (void)idPP;

    LOG_PP_INFO("PP_LoadAIDTable: carregando tabela de AIDs");

    if (!g_initialized) return PP_ERR_INIT;
    if (!buffer || tamBuf <= 0) return PP_ERR_PARAM;

    // Parse do formato binário ABECS T2
    s_aid_count = 0;
    int consumed = table_parse_aids(
        reinterpret_cast<const uint8_t*>(buffer), tamBuf, s_aids, &s_aid_count);
    if (consumed < 0 || s_aid_count == 0) {
        LOG_PP_ERROR("PP_LoadAIDTable: parse falhou ou tabela vazia");
        return PP_ERR_PARAM;
    }

    bool firstCT   = true;
    bool firstCTLS = true;

    for (int i = 0; i < s_aid_count; i++) {
        // Carregar no SDI CT
        EMV_CT_APPLI_DATA_STRUCT ctAid = {};
        uint32_t ctFlags = 0;
        abecs_to_sdi_aid_ct(&s_aids[i], &ctAid, &ctFlags, firstCT);

        EMV_ADK_INFO ret = SDI_CT_SetAppliData(&ctAid, ctFlags);
        if (ret != EMV_ADK_OK) {
            LOG_PP_ERROR("PP_LoadAIDTable: SDI_CT_SetAppliData[%d] falhou: 0x%X", i, ret);
            return PP_ERR_PARAM;
        }
        firstCT = false;

        // Se CTLS suportado, carregar também no SDI CTLS
        if (s_aids[i].ctls_supported && s_aids[i].kernel_id != 0) {
            EMV_CTLS_APPLI_DATA_STRUCT ctlsAid = {};
            uint32_t ctlsFlags = 0;
            abecs_to_sdi_aid_ctls(&s_aids[i], &ctlsAid, &ctlsFlags, firstCTLS);

            ret = SDI_CTLS_SetAppliDataSchemeSpecific(&ctlsAid, s_aids[i].kernel_id, ctlsFlags);
            if (ret != EMV_ADK_OK) {
                LOG_PP_ERROR("PP_LoadAIDTable: SDI_CTLS_SetAppliDataSchemeSpecific[%d] falhou: 0x%X",
                             i, ret);
                // Não fatal — AID pode não suportar CTLS neste kernel
            }
            firstCTLS = false;
        }
    }

    LOG_PP_INFO("PP_LoadAIDTable: %d AIDs carregados", s_aid_count);
    return PP_OK;
}

int PP_LoadCAPKTable(int idPP, char* buffer, int tamBuf) {
    (void)idPP;

    LOG_PP_INFO("PP_LoadCAPKTable: carregando tabela de CAPKs");

    if (!g_initialized) return PP_ERR_INIT;
    if (!buffer || tamBuf <= 0) return PP_ERR_PARAM;

    // Parse do formato binário ABECS T3
    s_capk_count = 0;
    int consumed = table_parse_capks(
        reinterpret_cast<const uint8_t*>(buffer), tamBuf, s_capks, &s_capk_count);
    if (consumed < 0 || s_capk_count == 0) {
        LOG_PP_ERROR("PP_LoadCAPKTable: parse falhou ou tabela vazia");
        return PP_ERR_PARAM;
    }

    bool firstCT   = true;
    bool firstCTLS = true;

    for (int i = 0; i < s_capk_count; i++) {
        EMV_CT_CAPKEY_STRUCT ctKey = {};
        abecs_to_sdi_capk_ct(&s_capks[i], &ctKey);

        uint32_t flags = firstCT ? EMV_ADK_CLEAR_ALL_RECORDS : 0;
        EMV_ADK_INFO ret = SDI_CT_StoreCAPKey(&ctKey, flags);
        if (ret != EMV_ADK_OK) {
            LOG_PP_ERROR("PP_LoadCAPKTable: SDI_CT_StoreCAPKey[%d] falhou: 0x%X", i, ret);
            return PP_ERR_PARAM;
        }
        firstCT = false;

        if (s_capks[i].ctls_supported) {
            EMV_CTLS_CAPKEY_STRUCT ctlsKey = {};
            abecs_to_sdi_capk_ctls(&s_capks[i], &ctlsKey);

            uint32_t ctlsFlags = firstCTLS ? EMV_ADK_CLEAR_ALL_RECORDS : 0;
            ret = SDI_CTLS_StoreCAPKey(&ctlsKey, ctlsFlags);
            if (ret != EMV_ADK_OK) {
                LOG_PP_ERROR("PP_LoadCAPKTable: SDI_CTLS_StoreCAPKey[%d] falhou: 0x%X", i, ret);
                // Não fatal individualmente
            }
            firstCTLS = false;
        }
    }

    // OBRIGATÓRIO: persistir configuração nos kernels EMV do SDI Server
    // Sem isso, a configuração é perdida no próximo restart do SDI
    EMV_ADK_INFO ret = SDI_CT_ApplyConfiguration();
    if (ret != EMV_ADK_OK) {
        LOG_PP_ERROR("PP_LoadCAPKTable: SDI_CT_ApplyConfiguration falhou: 0x%X", ret);
        return PP_ERR_PINPAD;
    }

    ret = SDI_CTLS_ApplyConfiguration();
    if (ret != EMV_ADK_OK) {
        LOG_PP_ERROR("PP_LoadCAPKTable: SDI_CTLS_ApplyConfiguration falhou: 0x%X", ret);
        return PP_ERR_PINPAD;
    }

    g_tables_loaded = true;
    LOG_PP_INFO("PP_LoadCAPKTable: %d CAPKs carregados, ApplyConfiguration OK", s_capk_count);
    return PP_OK;
}

}  // extern "C"
