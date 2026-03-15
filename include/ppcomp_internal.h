#pragma once
// include/ppcomp_internal.h
// Tipos internos, constantes PP_*, macros de log e variáveis globais da libppcomp.
//
// SEGURANÇA: Este header NUNCA deve expor PAN, PIN, trilhas ou chaves em claro.
// As macros de log são definidas aqui mas proibidas de logar dados sensíveis.

#include <cstdint>
#include <mutex>

// Inclusão condicional: em build Android usa __android_log_print;
// em build de teste (x86_64) usa fprintf stderr.
#ifdef __ANDROID__
#include <android/log.h>
#define LOG_PP_TAG "libppcomp"
#define LOG_PP_DEBUG(fmt, ...) __android_log_print(ANDROID_LOG_DEBUG, LOG_PP_TAG, fmt, ##__VA_ARGS__)
#define LOG_PP_INFO(fmt, ...)  __android_log_print(ANDROID_LOG_INFO,  LOG_PP_TAG, fmt, ##__VA_ARGS__)
#define LOG_PP_ERROR(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_PP_TAG, fmt, ##__VA_ARGS__)
#else
#include <cstdio>
#define LOG_PP_DEBUG(fmt, ...) fprintf(stderr, "[DBG] libppcomp: " fmt "\n", ##__VA_ARGS__)
#define LOG_PP_INFO(fmt, ...)  fprintf(stderr, "[INF] libppcomp: " fmt "\n", ##__VA_ARGS__)
#define LOG_PP_ERROR(fmt, ...) fprintf(stderr, "[ERR] libppcomp: " fmt "\n", ##__VA_ARGS__)
#endif

// ─── Códigos de retorno ABECS 2.20 ───────────────────────────────────────────

constexpr int PP_OK                = 0;
constexpr int PP_GO_ONLINE         = 1;   ///< ARQC gerado — ir ao autorizador
constexpr int PP_OFFLINE_APPROVE   = 2;   ///< TC gerado offline
constexpr int PP_OFFLINE_DECLINE   = 3;   ///< AAC gerado offline
constexpr int PP_FALLBACK_CT       = 10;  ///< CTLS falhou — tentar chip CT
constexpr int PP_MULTIAPP          = 11;  ///< Múltiplos AIDs — aguarda seleção
constexpr int PP_CTLS_WAITING      = 12;  ///< Aguardando toque NFC

constexpr int PP_ERR_INIT          = -1;  ///< Não inicializado ou falha de init
constexpr int PP_ERR_PARAM         = -2;  ///< Parâmetro inválido
constexpr int PP_ERR_STATE         = -3;  ///< Estado interno incorreto
constexpr int PP_ERR_NOTCHIP       = -4;  ///< Tecnologia não é chip
constexpr int PP_ERR_BUFFER        = -5;  ///< Buffer de saída pequeno demais
constexpr int PP_ERR_NOEVENT       = -10; ///< Sem evento de cartão (poll)
constexpr int PP_ERR_PINPAD        = -31; ///< Erro crítico de pinpad (CliSiTef: erro 31)
constexpr int PP_ABORT             = -40; ///< Operação cancelada pelo usuário
constexpr int PP_ERR_TIMEOUT       = -41; ///< Timeout
constexpr int PP_ERR_WRONG_PIN     = -42; ///< PIN incorreto
constexpr int PP_ERR_PIN_BLOCKED   = -43; ///< PIN bloqueado (3 erros)
constexpr int PP_ERR_CARD_BLOCKED  = -44; ///< Cartão bloqueado
constexpr int PP_ERR_PIN           = -45; ///< Erro genérico de PIN
constexpr int PP_PIN_BYPASS        = -46; ///< PIN bypass (sem PIN)
constexpr int PP_ERR_GENERIC       = -99; ///< Erro genérico não mapeado

// ─── Constantes de moeda e país ──────────────────────────────────────────────

constexpr uint16_t CURRENCY_BRL_CODE   = 0x0986; ///< ISO 4217 — Real Brasileiro
constexpr uint8_t  CURRENCY_BRL_EXP    = 2;       ///< Casas decimais (centavos)
constexpr uint16_t COUNTRY_BRAZIL_CODE = 0x0076;  ///< ISO 3166

// ─── Tecnologias de cartão (bitmask ABECS) ───────────────────────────────────

constexpr uint8_t TEC_NONE     = 0x00;
constexpr uint8_t TEC_CHIP_CT  = 0x01; ///< EMV Contato
constexpr uint8_t TEC_CTLS     = 0x02; ///< EMV Contactless / NFC
constexpr uint8_t TEC_MSR      = 0x04; ///< Tarja magnética

// ─── Tipos de verificação de PIN ─────────────────────────────────────────────

constexpr int PIN_VERIFICATION_OFFLINE = 0; ///< Kernel EMV gerencia internamente
constexpr int PIN_VERIFICATION_ONLINE  = 1; ///< PED envia PIN block criptografado

// ─── Timeouts em milissegundos ────────────────────────────────────────────────

constexpr int TIMEOUT_DETECTION_MS    = 30000;
constexpr int TIMEOUT_PIN_MS          = 30000;
constexpr int TIMEOUT_CARD_REMOVAL_MS = 5000;

// ─── Limites de buffer ────────────────────────────────────────────────────────

constexpr size_t MAX_TLV_BUF          = 512;
constexpr size_t MAX_AID_LEN          = 16;
constexpr size_t MAX_CAPK_MODULUS_LEN = 248;
constexpr size_t MAX_CAPK_EXP_LEN     = 3;
constexpr size_t MAX_TAC_LEN          = 5;
constexpr size_t MAX_SCRIPT_RESULTS   = 8;
constexpr size_t KSN_LEN              = 10; ///< DUKPT KSN: 10 bytes
constexpr size_t PIN_BLOCK_LEN        = 8;  ///< ISO format 0: 8 bytes

// ─── Estado interno ──────────────────────────────────────────────────────────

/// Estados do autômato interno da libppcomp.
/// Acesso exclusivo pela thread da libclisitef; callbacks usam mutex.
enum class PPState : int {
    IDLE               = 0,
    DETECTING          = 1,
    CARD_CT            = 2,
    CARD_CTLS          = 3,
    CARD_MSR           = 4,
    EMV_CT_STARTED     = 5,
    EMV_CT_OFFLINE     = 6,
    EMV_CT_ONLINE      = 7,
    EMV_CTLS_SETUP     = 8,
    EMV_CTLS_POLLING   = 9,
    PIN                = 10,
    MANUAL_ENTRY       = 11,
};

// ─── Structs ABECS ────────────────────────────────────────────────────────────

/// Dados do terminal — mapeados de PP_SetTermData (T1 da ABECS 2.20)
struct ABECS_TERMDATA_STRUCT {
    uint8_t  terminal_type;            ///< Tag 9F35
    uint8_t  terminal_capabilities[3]; ///< Tag 9F33
    uint8_t  add_terminal_cap[5];      ///< Tag 9F40
    uint32_t ctls_floor_limit;         ///< Limite de piso CTLS (centavos)
    uint32_t ctls_txn_limit;           ///< Limite de transação CTLS (centavos)
    uint32_t ctls_nopin_limit;         ///< Limite sem PIN CTLS (centavos)
};

/// Registro de AID — campos CT e CTLS separados por exigência da spec
struct ABECS_AID_RECORD {
    uint8_t  aid[MAX_AID_LEN];  ///< Application Identifier
    uint8_t  aid_len;
    bool     ctls_supported;    ///< Carrega também nas tabelas CTLS
    uint8_t  kernel_id;         ///< 2=MK 3=VK 4=AK 6=DK 7=EK (apenas CTLS)
    uint8_t  tac_denial[MAX_TAC_LEN];
    uint8_t  tac_online[MAX_TAC_LEN];
    uint8_t  tac_default[MAX_TAC_LEN];
    uint32_t floor_limit_ct;    ///< Limite de piso CT (centavos)
    uint8_t  default_ddol[32];  ///< Dynamic Data Object List default
    uint8_t  default_ddol_len;
    uint8_t  default_tdol[32];  ///< Transaction Data Object List default
    uint8_t  default_tdol_len;
};

/// Registro de CAPK (Certification Authority Public Key)
struct ABECS_CAPK_RECORD {
    uint8_t  rid[5];                        ///< Registered Application Provider ID
    uint8_t  index;                         ///< Índice da chave pública
    uint8_t  algorithm;                     ///< 0x01 = RSA
    uint8_t  modulus[MAX_CAPK_MODULUS_LEN];
    uint8_t  modulus_len;
    uint8_t  exponent[MAX_CAPK_EXP_LEN];
    uint8_t  exponent_len;
    uint8_t  checksum[20];                  ///< SHA-1 do módulo + expoente
    bool     ctls_supported;
};

/// Resultado de transação para comprovante
struct ABECS_TRANSRESULT {
    uint8_t  cryptogram[8];           ///< Application Cryptogram (AC)
    uint8_t  cryptogram_type;         ///< 0x80=ARQC 0x40=TC 0x00=AAC
    uint8_t  atc[2];                  ///< Application Transaction Counter
    uint8_t  tvr[5];                  ///< Terminal Verification Results
    uint8_t  tsi[2];                  ///< Transaction Status Information
    uint8_t  iad[32];                 ///< Issuer Application Data
    uint8_t  iad_len;
    uint8_t  pan_masked[19];          ///< PAN ofuscado (6 primeiros + 4 últimos)
    uint8_t  pan_masked_len;
    uint8_t  pan_seq;                 ///< PAN Sequence Number
    uint8_t  aid_selected[MAX_AID_LEN];
    uint8_t  aid_selected_len;
    uint8_t  app_label[16];
    uint8_t  app_label_len;
    uint8_t  unpred_number[4];        ///< Unpredictable Number (tag 9F37)
    uint32_t amount_auth;             ///< Valor autorizado em centavos
    uint8_t  txn_date[3];             ///< Formato YYMMDD
    uint8_t  txn_type;
    bool     has_script_result;
    uint8_t  script_result[64];
    uint8_t  script_result_len;
};

/// Dados de tarja magnética (MSR) — sempre cifrados (P2PE)
struct ABECS_MSR_DATA {
    uint8_t  track2_enc[40];   ///< Trilha 2 cifrada DUKPT
    uint8_t  track2_enc_len;
    uint8_t  track1_enc[80];   ///< Trilha 1 cifrada DUKPT
    uint8_t  track1_enc_len;
    uint8_t  ksn[KSN_LEN];     ///< Key Serial Number DUKPT
    bool     track1_present;
    bool     track2_present;
};

/// Parâmetros CTLS por transação
struct ABECS_CTLS_PARAMS {
    uint32_t txn_limit;        ///< Limite de transação (centavos)
    uint32_t floor_limit;      ///< Piso (centavos)
    uint32_t no_pin_limit;     ///< Limite sem PIN (centavos)
    uint8_t  supported_kernels; ///< Bitmask: bit1=MK bit2=VK bit3=AK bit5=DK bit6=EK
};

// ─── Variáveis globais — definidas em src/ppcomp_globals.cpp ─────────────────
// Acessadas pela thread da libclisitef.
// Callbacks SDI devem adquirir g_sdi_callback_mutex antes de modificar.

extern PPState         g_pp_state;
extern uint8_t         g_current_tec;          ///< Tecnologia detectada (TEC_*)
extern bool            g_initialized;
extern bool            g_tables_loaded;
extern bool            g_pin_entered;
extern bool            g_pin_requested;
extern bool            g_pin_blocked;
extern bool            g_card_removed_mid_txn;
extern int             g_pin_digit_count;
extern int             g_pin_retry_count;
extern int             g_candidate_count;
extern bool            g_detection_active;
extern std::mutex      g_sdi_callback_mutex;
