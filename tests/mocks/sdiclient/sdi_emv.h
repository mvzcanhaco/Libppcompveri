#pragma once
// tests/mocks/sdiclient/sdi_emv.h
// Stub dos tipos e funções EMV do Verifone ADK para builds de teste (USE_SDI_MOCK=ON).
// Em build Android real, este arquivo é substituído por libs/include/sdiclient/sdi_emv.h

#include <cstdint>
#include <cstring>

// ─── Tipo de retorno SDI ──────────────────────────────────────────────────────

typedef int EMV_ADK_INFO;

// ─── Códigos de retorno ───────────────────────────────────────────────────────

#define EMV_ADK_OK               0x00
#define EMV_ADK_GO_ONLINE        0x01
#define EMV_ADK_ONLINE           0x01
#define EMV_ADK_SEL_APP_OK       0x01
#define EMV_ADK_OFFLINE_APPROVE  0x02
#define EMV_ADK_DECLINE          0x03
#define EMV_ADK_NO_CARD          0x10
#define EMV_ADK_FALLBACK         0x11
#define EMV_ADK_MULTI_APP        0x12
#define EMV_ADK_TXN_ABORT        0x20
#define EMV_ADK_TXN_CTLS_MOBILE  0x21
#define EMV_ADK_WRONG_PIN        0x40
#define EMV_ADK_PIN_BLOCKED      0x41
#define EMV_ADK_CARD_BLOCKED     0x42
#define EMV_ADK_COMM_ERROR       0x50
#define EMV_ADK_TIMEOUT          0x51
#define EMV_ADK_USER_CANCEL      0x52
#define EMV_ADK_INTERNAL_ERROR   0x60
#define EMV_ADK_PARAM_ERROR      0x61

// ─── Flags de operação ────────────────────────────────────────────────────────

#define EMV_ADK_CLEAR_ALL_RECORDS  0x01
#define EMV_CT_TRY_PPS             0x01
#define EMV_CT_DETECT_WRONG_ATR    0x02

// ─── Tipos de transação EMV (tag 9C) ─────────────────────────────────────────

#define EMV_TXN_PURCHASE           0x00
#define EMV_TXN_CASH               0x01
#define EMV_TXN_CASHBACK           0x09
#define EMV_TXN_REFUND             0x20
#define EMV_TXN_INQUIRY            0x31
#define EMV_TXN_PREAUTH            0x30

// ─── Tipos de criptograma ─────────────────────────────────────────────────────

#define EMV_CRYPTOGRAM_AAC         0x00  // Offline decline
#define EMV_CRYPTOGRAM_TC          0x40  // Offline approve
#define EMV_CRYPTOGRAM_ARQC        0x80  // Go online

// ─── Callbacks EMV ────────────────────────────────────────────────────────────

#define EMV_CB_SELECT_APP          0x01
#define EMV_CB_ONLINE_PIN          0x02
#define EMV_CB_SCRIPT_RESULT       0x03
#define EMV_CB_WRONG_PIN           0x04
#define EMV_CB_PIN_BLOCKED         0x05

#define EMV_CTLS_CB_SELECT_KERNEL  0x10
#define EMV_CTLS_CB_ONLINE_PIN     0x11

// ─── Structs de dados terminais ───────────────────────────────────────────────

struct EMV_CT_TERMDATA_STRUCT {
    uint8_t  TerminalType;                   // tag 9F35
    uint8_t  TerminalCapabilities[3];        // tag 9F33
    uint8_t  AdditionalTerminalCapabilities[5]; // tag 9F40
    uint16_t TransactionCurrencyCode;        // tag 5F2A (BCD)
    uint8_t  TransactionCurrencyExponent;    // tag 5F36
    uint16_t MerchantCategoryCode;           // tag 9F15
    uint16_t TerminalCountryCode;            // tag 9F1A (BCD)
    uint32_t FloorLimit;                     // tag 9F1B (centavos)
};

struct EMV_CTLS_TERMDATA_STRUCT {
    uint8_t  TerminalType;
    uint8_t  TerminalCapabilities[3];
    uint8_t  AdditionalTerminalCapabilities[5];
    uint16_t TransactionCurrencyCode;
    uint8_t  TransactionCurrencyExponent;
    uint16_t TerminalCountryCode;
    uint32_t CTLSTransactionLimit;           // Limite de transação CTLS
    uint32_t CTLSFloorLimit;
    uint32_t CTLSNoPINLimit;
};

// ─── Structs de AID/CAPK ──────────────────────────────────────────────────────

struct EMV_CT_APPLI_DATA_STRUCT {
    uint8_t  AID[16];
    uint8_t  AIDLen;
    uint8_t  TACDenial[5];
    uint8_t  TACOnline[5];
    uint8_t  TACDefault[5];
    uint32_t FloorLimit;
    uint8_t  DefaultDDOL[32];
    uint8_t  DefaultDDOLLen;
    uint8_t  DefaultTDOL[32];
    uint8_t  DefaultTDOLLen;
    uint32_t Flags;
};

struct EMV_CT_CAPKEY_STRUCT {
    uint8_t  RID[5];
    uint8_t  Index;
    uint8_t  Algorithm;
    uint8_t  Modulus[248];
    uint8_t  ModulusLen;
    uint8_t  Exponent[3];
    uint8_t  ExponentLen;
    uint8_t  Checksum[20];
};

struct EMV_CTLS_APPLI_DATA_STRUCT {
    uint8_t  AID[16];
    uint8_t  AIDLen;
    uint8_t  KernelID;
    uint8_t  TACDenial[5];
    uint8_t  TACOnline[5];
    uint8_t  TACDefault[5];
    uint32_t CTLSTransactionLimit;
    uint32_t CTLSFloorLimit;
    uint32_t CTLSNoPINLimit;
    uint32_t Flags;
};

struct EMV_CTLS_CAPKEY_STRUCT {
    uint8_t  RID[5];
    uint8_t  Index;
    uint8_t  Algorithm;
    uint8_t  Modulus[248];
    uint8_t  ModulusLen;
    uint8_t  Exponent[3];
    uint8_t  ExponentLen;
    uint8_t  Checksum[20];
};

// ─── Structs de resultado de transação ───────────────────────────────────────

#define MAX_CANDIDATES  10
#define MAX_APP_LABEL   16
#define MAX_APP_NAME    32

struct EMV_CT_CANDIDATE_STRUCT {
    uint8_t  AID[16];
    uint8_t  AIDLen;
    char     AppLabel[MAX_APP_LABEL + 1];
    char     AppPreferredName[MAX_APP_NAME + 1];
    uint8_t  Priority;
};

struct EMV_CT_STARTRESULT_STRUCT {
    uint8_t                   NumCandidates;
    EMV_CT_CANDIDATE_STRUCT   Candidates[MAX_CANDIDATES];
    uint8_t                   SelectedAID[16];
    uint8_t                   SelectedAIDLen;
};

struct EMV_CT_TRANSRES_STRUCT {
    uint8_t  Cryptogram[8];              // tag 9F26
    uint8_t  CryptogramType;             // tag 9F27 (0=AAC, 0x40=TC, 0x80=ARQC)
    uint8_t  ATC[2];                     // tag 9F36
    uint8_t  TVR[5];                     // tag 95
    uint8_t  TSI[2];                     // tag 9B
    uint8_t  IAD[32];                    // tag 9F10
    uint8_t  IADLen;
    uint8_t  AIDSelected[16];            // tag 84
    uint8_t  AIDSelectedLen;
    char     AppLabel[MAX_APP_LABEL + 1]; // tag 50
    char     AppPreferredName[MAX_APP_NAME + 1]; // tag 9F12
    uint8_t  UnpredictableNumber[4];     // tag 9F37
    uint32_t AuthorizedAmount;           // tag 9F02
    uint8_t  TransactionDate[3];         // tag 9A (YYMMDD)
    uint8_t  TransactionType;            // tag 9C
    uint8_t  PANSeqNumber;               // tag 5F34
    bool     HasScriptResult;
    uint8_t  ScriptResult[64];
    uint8_t  ScriptResultLen;
};

struct EMV_SDI_CT_TRANSRES_STRUCT {
    uint8_t  MaskedPAN[20];              // ofuscado pelo SDI
    uint8_t  MaskedPANLen;
    uint8_t  ExpiryDate[3];              // YYMMDD ofuscado
    bool     PINWasBypassed;
    uint8_t  CVMResults[3];              // tag 9F34
};

struct EMV_CTLS_TRANSRES_STRUCT {
    uint8_t  Cryptogram[8];
    uint8_t  CryptogramType;
    uint8_t  ATC[2];
    uint8_t  TVR[5];
    uint8_t  AIDSelected[16];
    uint8_t  AIDSelectedLen;
    char     AppLabel[MAX_APP_LABEL + 1];
    uint32_t AuthorizedAmount;
    uint8_t  TransactionDate[3];
    uint8_t  TransactionType;
    bool     IsContactlessMobile;
    uint8_t  KernelID;
};

struct EMV_SDI_CTLS_TRANSRES_STRUCT {
    uint8_t  MaskedPAN[20];
    uint8_t  MaskedPANLen;
    uint8_t  CVMResults[3];
};

// ─── Constantes de moeda e status ────────────────────────────────────────────

#define CURRENCY_BRL_CODE   0x0986  // ISO 4217 BRL
#define CURRENCY_BRL_EXP    0x02

#define SDI_STATUS_PIN_DIGIT  0x01  // Evento: dígito PIN digitado (value = contagem)

// ─── Callback types ───────────────────────────────────────────────────────────

typedef EMV_ADK_INFO (*EMV_CT_CALLBACK_FN)(uint8_t* data, uint32_t len);
typedef EMV_ADK_INFO (*EMV_CTLS_CALLBACK_FN)(uint8_t* data, uint32_t len);

// ─── Tags para SDI_fetchTxnTags ───────────────────────────────────────────────

static const uint32_t COMPROVANTE_TAGS[] = {
    0x9F26,  // Application Cryptogram
    0x9F36,  // ATC
    0x9F10,  // IAD
    0x9F37,  // Unpredictable Number
    0x84,    // DF Name (AID selecionado)
    0x9F1A,  // Terminal Country Code
    0x5F2A,  // Transaction Currency Code
    0x9A,    // Transaction Date
    0x9C,    // Transaction Type
    0x9F02,  // Amount Authorized
    0x5F34,  // PAN Sequence Number
    0x9F27,  // Cryptogram Information Data
    0x9F06,  // AID
    0x9F12,  // Application Preferred Name
    0x50,    // Application Label
};
#define COMPROVANTE_TAGS_COUNT  (sizeof(COMPROVANTE_TAGS) / sizeof(COMPROVANTE_TAGS[0]))

// Tags MSR
#define TAG_TRACK2_DATA      0x57
#define TAG_PAN              0x5A
#define TAG_CARDHOLDER_NAME  0x5F20
#define TAG_EXPIRY_DATE      0x5F24
#define TAG_TRACK2_EQUIV     0x9F6B
#define TAG_TRACK1_ENC       0xDFAB20
#define TAG_TRACK2_ENC       0xDFAB21
#define TAG_MSR_KSN          0xDFAB22

// ─── Funções SDI EMV (declarações) ───────────────────────────────────────────

#ifdef __cplusplus
extern "C" {
#endif

EMV_ADK_INFO SDI_ProtocolInit(const char* jsonConfig);
EMV_ADK_INFO SDI_ProtocolExit(void);
EMV_ADK_INFO SDI_CT_Init_Framework(EMV_CT_CALLBACK_FN callback);
EMV_ADK_INFO SDI_CT_Exit_Framework(void);
EMV_ADK_INFO SDI_CTLS_Init_Framework(EMV_CTLS_CALLBACK_FN callback);
EMV_ADK_INFO SDI_CTLS_Exit_Framework(void);
EMV_ADK_INFO SDI_SetSdiCallback(void* callback, uint8_t sw1, uint8_t sw2);

EMV_ADK_INFO SDI_CT_SetTermData(const EMV_CT_TERMDATA_STRUCT* termData);
EMV_ADK_INFO SDI_CTLS_SetTermData(const EMV_CTLS_TERMDATA_STRUCT* termData);
EMV_ADK_INFO SDI_CT_SetAppliData(const EMV_CT_APPLI_DATA_STRUCT* appData, uint32_t flags);
EMV_ADK_INFO SDI_CTLS_SetAppliDataSchemeSpecific(const EMV_CTLS_APPLI_DATA_STRUCT* appData,
                                                  uint8_t kernelID, uint32_t flags);
EMV_ADK_INFO SDI_CT_StoreCAPKey(const EMV_CT_CAPKEY_STRUCT* key, uint32_t flags);
EMV_ADK_INFO SDI_CTLS_StoreCAPKey(const EMV_CTLS_CAPKEY_STRUCT* key, uint32_t flags);
EMV_ADK_INFO SDI_CT_ApplyConfiguration(void);
EMV_ADK_INFO SDI_CTLS_ApplyConfiguration(void);

EMV_ADK_INFO SDI_CT_StartTransaction(uint32_t amount, uint32_t cashback,
                                      uint16_t currencyCode, uint8_t txnType,
                                      EMV_CT_STARTRESULT_STRUCT* startResult);
EMV_ADK_INFO SDI_CT_SetSelectedApp(const uint8_t* aid, uint8_t aidLen);
EMV_ADK_INFO SDI_CT_ContinueOffline(uint32_t options,
                                     EMV_CT_TRANSRES_STRUCT* transResult,
                                     EMV_SDI_CT_TRANSRES_STRUCT* sdiExtra);
EMV_ADK_INFO SDI_CT_ContinueOnline(const uint8_t* arpc, uint32_t arpcLen,
                                    const uint8_t* scripts, uint32_t scriptsLen,
                                    EMV_CT_TRANSRES_STRUCT* transResult,
                                    EMV_SDI_CT_TRANSRES_STRUCT* sdiExtra);
EMV_ADK_INFO SDI_CT_EndTransaction(uint32_t options);
EMV_ADK_INFO SDI_CT_Break(void);

EMV_ADK_INFO SDI_CTLS_SetupTransaction(uint32_t amount, uint32_t cashback,
                                        uint16_t currencyCode, uint8_t txnType,
                                        void* setupResult);
EMV_ADK_INFO SDI_CTLS_ContinueOffline(uint32_t options,
                                       EMV_CTLS_TRANSRES_STRUCT* transResult,
                                       EMV_SDI_CTLS_TRANSRES_STRUCT* sdiExtra);
EMV_ADK_INFO SDI_CTLS_ContinueOnline(const uint8_t* arpc, uint32_t arpcLen,
                                      EMV_CTLS_TRANSRES_STRUCT* transResult,
                                      EMV_SDI_CTLS_TRANSRES_STRUCT* sdiExtra);
EMV_ADK_INFO SDI_CTLS_EndTransaction(uint32_t options);
EMV_ADK_INFO SDI_CTLS_Break(void);

EMV_ADK_INFO SDI_fetchTxnTags(const uint32_t* tags, uint32_t tagCount,
                               uint8_t* outBuf, uint32_t* outLen);

#ifdef __cplusplus
}
#endif
