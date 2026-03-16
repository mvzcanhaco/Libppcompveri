#pragma once
// include/pp_sdi_bridge.h
// Declarações de todas as funções de mapeamento/serialização SDI ↔ ABECS.
//
// Este header não depende de nenhuma biblioteca Verifone — pode ser incluído
// em builds de teste (x86_64) sem o ADK instalado, usando forward declarations.

#include <cstdint>
#include <vector>
#include "ppcomp_internal.h"

// Forward declarations para evitar incluir headers do ADK neste header.
// As definições reais de EMV_ADK_INFO, EMV_CT_TRANSRES_STRUCT, etc.
// estão em sdiclient/sdi_emv.h (incluído apenas nos .cpp).
#ifndef EMV_ADK_INFO_DEFINED
using EMV_ADK_INFO = int;
#define EMV_ADK_INFO_DEFINED
#endif

struct EMV_CT_STARTRESULT_STRUCT;
struct EMV_CT_TRANSRES_STRUCT;
struct EMV_SDI_CT_TRANSRES_STRUCT;
struct EMV_CTLS_TRANSRES_STRUCT;
struct EMV_SDI_CTLS_TRANSRES_STRUCT;

// ─── Mapeamento de códigos de erro ───────────────────────────────────────────

/// Converte EMV_ADK_INFO (código Verifone SDI) para código ABECS PP_*.
/// NUNCA retornar um código SDI diretamente — sempre passar por esta função.
int mapEmvRetToAbecs(EMV_ADK_INFO emvRet);

/// Converte tipo de transação ABECS (T2_TIPOTR) para tipo EMV (tag 9C).
/// Exemplo: 0x01=compra→0x00, 0x02=saque→0x01, 0x09=cashback→0x09
uint8_t mapTxnTypeAbecsToEmv(int abecsTxnType);

/// Converte bitmask de modalidades ABECS para bitmask de tecnologias SDI.
uint8_t mapModalidadeToTec(int modalidades);

// ─── Serialização de resultados SDI → buffer ABECS ───────────────────────────

/// Serializa EMV_CT_TRANSRES_STRUCT + extra SDI para buffer de saída ABECS.
/// outBuf deve ter pelo menos 128 bytes. outLen é atualizado com o tamanho real.
void pp_serialize_trans_result(
    const EMV_CT_TRANSRES_STRUCT*     ct,
    const EMV_SDI_CT_TRANSRES_STRUCT* sdi,
    char*  outBuf,
    int*   outLen);

/// Serializa resultado CTLS para buffer ABECS.
void pp_serialize_ctls_result(
    const EMV_CTLS_TRANSRES_STRUCT*     ctls,
    const EMV_SDI_CTLS_TRANSRES_STRUCT* sdi,
    char*  outBuf,
    int*   outLen);

/// Serializa EMV_CT_STARTRESULT_STRUCT (lista de candidatos) para buffer ABECS.
void pp_serialize_start_result(
    const EMV_CT_STARTRESULT_STRUCT* startResult,
    char*  outBuf,
    int*   outLen);

/// Serializa ABECS_MSR_DATA para buffer de saída ABECS (formato T2 MSR).
void pp_serialize_msr_abecs(
    const ABECS_MSR_DATA* msr,
    char*  outBuf,
    int*   outLen);

// ─── Parse de TLV → structs ABECS ────────────────────────────────────────────

/// Popula ABECS_MSR_DATA a partir de buffer TLV retornado pelo SDI_fetchTxnTags.
/// Tags esperadas: 0x57, 0x5A, 0xDFAB20, 0xDFAB21 (enc), 0xDFAB22 (KSN).
void pp_parse_tlv_to_msr(
    ABECS_MSR_DATA*       msr,
    const uint8_t*        tlv,
    uint32_t              tlvLen);

/// Serializa ABECS_TRANSRESULT para buffer comprovante.
void pp_abecs_serialize(
    const ABECS_TRANSRESULT* result,
    char*  outBuf,
    int*   outLen);

// ─── Utilitários TLV ─────────────────────────────────────────────────────────

/// Busca tag no buffer TLV e retorna o primeiro byte do value, ou 0 se não encontrado.
uint8_t pp_tlv_get_byte(
    const uint8_t* tlv,
    uint32_t       tlvLen,
    uint32_t       tag);

/// Busca tag no buffer TLV e copia value para out. Retorna true se encontrado.
bool pp_tlv_get_bytes(
    const uint8_t* tlv,
    uint32_t       tlvLen,
    uint32_t       tag,
    uint8_t*       out,
    uint32_t*      outLen);

// ─── Utilitários DUKPT ───────────────────────────────────────────────────────

/// Extrai KSN (10 bytes) do inventário retornado por SdiCrypt::getKeyInventory().
/// NUNCA logar o conteúdo do KSN.
void pp_extract_ksn_from_inventory(
    const std::vector<uint8_t>& inventory,
    uint8_t* ksn,
    int*     ksnLen);

// ─── Script results ───────────────────────────────────────────────────────────

/// Salva resultado de script issuer em g_script_results[].
/// Chamado pelo callback cb_emv_ct na tag EMV_CB_SCRIPT_RESULT.
void pp_save_script_result(const uint8_t* data, uint32_t len);
