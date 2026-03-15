#pragma once
// tables/table_loader.h
// Parser do formato binário ABECS 2.20 para tabelas T1, T2 e T3.
//
// Sem dependências externas — pode ser compilado em qualquer plataforma.

#include <cstdint>
#include "ppcomp_internal.h"

// ─── Constantes do formato ABECS ─────────────────────────────────────────────

// Identificadores de tabela no stream binário ABECS
#define ABECS_TABLE_T1   0x01   // TermData
#define ABECS_TABLE_T2   0x02   // AID Table
#define ABECS_TABLE_T3   0x03   // CAPK Table

// Tamanho máximo de registros
#define MAX_AID_RECORDS   50
#define MAX_CAPK_RECORDS  100

// ─── Funções de parsing ───────────────────────────────────────────────────────

/**
 * table_parse_termdata — Lê estrutura T1 (TermData) do buffer ABECS.
 * @param buf     Buffer de entrada (formato ABECS binário)
 * @param bufLen  Tamanho do buffer
 * @param out     Struct de saída preenchida
 * @return Número de bytes consumidos, ou -1 em caso de erro
 */
int table_parse_termdata(const uint8_t* buf, int bufLen, ABECS_TERMDATA_STRUCT* out);

/**
 * table_parse_aids — Lê tabela de AIDs (T2) do buffer ABECS.
 * @param buf     Buffer de entrada
 * @param bufLen  Tamanho do buffer
 * @param out     Array de saída (MAX_AID_RECORDS)
 * @param count   [out] Número de AIDs lidos
 * @return Número de bytes consumidos, ou -1 em erro
 */
int table_parse_aids(const uint8_t* buf, int bufLen,
                     ABECS_AID_RECORD* out, int* count);

/**
 * table_parse_capks — Lê tabela de CAPKs (T3) do buffer ABECS.
 * @param buf     Buffer de entrada
 * @param bufLen  Tamanho do buffer
 * @param out     Array de saída (MAX_CAPK_RECORDS)
 * @param count   [out] Número de CAPKs lidos
 * @return Número de bytes consumidos, ou -1 em erro
 */
int table_parse_capks(const uint8_t* buf, int bufLen,
                      ABECS_CAPK_RECORD* out, int* count);

/**
 * table_get_kernel_id — Determina kernel CTLS pelo RID do AID.
 * @param aid    Bytes do AID
 * @param aidLen Comprimento
 * @return Kernel ID: 2=MK, 3=VK, 4=AK, 6=DK, 7=EK, 0=desconhecido
 */
uint8_t table_get_kernel_id(const uint8_t* aid, uint8_t aidLen);
