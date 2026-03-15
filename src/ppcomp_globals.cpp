// src/ppcomp_globals.cpp
// Definição e inicialização de todas as variáveis globais da libppcomp.
//
// REGRA: estas variáveis são acessadas exclusivamente pela thread da libclisitef.
// Callbacks SDI (outras threads) devem adquirir g_sdi_callback_mutex antes
// de modificar qualquer variável aqui definida.

#include <mutex>
#include "ppcomp_internal.h"

// ─── Estado do autômato ──────────────────────────────────────────────────────

PPState  g_pp_state              = PPState::IDLE;
uint8_t  g_current_tec           = TEC_NONE;
bool     g_initialized           = false;
bool     g_tables_loaded         = false;
bool     g_detection_active      = false;

// ─── Estado de PIN ────────────────────────────────────────────────────────────

bool g_pin_entered               = false;
bool g_pin_requested             = false;
bool g_pin_blocked               = false;
int  g_pin_digit_count           = 0;
int  g_pin_retry_count           = 0;

// ─── Estado de transação ─────────────────────────────────────────────────────

bool g_card_removed_mid_txn      = false;
int  g_candidate_count           = 0;

// ─── Sincronização ────────────────────────────────────────────────────────────

std::mutex g_sdi_callback_mutex;
