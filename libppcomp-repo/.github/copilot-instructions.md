# GitHub Copilot Instructions — libppcomp

## IDENTIDADE DO PROJETO

Você está desenvolvendo **libppcomp**, uma biblioteca C++17 que implementa a
interface **ABECS 2.20** (Biblioteca Compartilhada para Pinpad) sobre o SDK nativo
da Verifone (**ADK 5.0.3 / SDI — Secure Device Interface**) para o terminal
**V660P-A (VAOS/VOS3)**.

Esta biblioteca é o elo entre:
- `libclisitef.so` (Software Express — núcleo C do protocolo SiTef)
- O hardware de pagamento do terminal Verifone V660P-A

## CADEIA COMPLETA DE CHAMADAS

```
App Android (Kotlin/Java)
  │ Intent("br.com.softwareexpress.sitef.msitef.ACTIVITY_CLISITEF")
m-SiTef.apk  [Software Express]
  │ Java API interna
clisitef-android.jar  [Software Express]
  │ JNI → dlopen("libclisitef.so")
libclisitef.so  [Software Express — C — protocolo SiTef]
  │ dlopen("libppcomp.so") + dlsym(PP_Open, PP_Close, PP_GetCard...)
══► libppcomp.so  ◄══ ESTE PROJETO (C++17, NDK arm64-v8a)
  │ link estático
libsdiclient.a + libEMV_CT_Client.a + libEMV_CTLS_Client.a  [Verifone ADK]
  │ socket 127.0.0.1:12000
SDI Server  [processo VAOS do V660P-A]
  │ hardware drivers
V660P-A: EMV chip · NFC/CTLS · MSR · PED (PIN) · Display
```

## ESPECIFICAÇÕES TÉCNICAS OBRIGATÓRIAS

### Linguagem e compilação
- **C++17** — usar structured bindings, if constexpr, std::optional, std::variant
- **NDK 26.2.11394342** (Android NDK r26b)
- **ABI alvo**: `arm64-v8a` SOMENTE — o V660P-A é ARM64
- **STL**: `c++_shared` (libc++_shared.so)
- **Nível de API Android mínimo**: 28 (Android 9 — VAOS base)
- **NO EXCEPTIONS** em código de produção — retorno por código inteiro ABECS
- **NO RTTI** — `-fno-rtti` no CMake
- **Assinatura criptográfica**: os `.so` finais NÃO podem ser stripped
  (`-Wl,--no-strip-debug`, opção `doNotStrip` no build.gradle)

### Headers e includes disponíveis
```cpp
// ADK Verifone — disponíveis em libs/include/
#include <sdiclient/sdi_emv.h>    // SDI_CT_*, SDI_CTLS_*, SDI_fetchTxnTags
#include <sdiclient/sdi_if.h>     // libsdi::SDI, libsdi::CardDetection,
                                   // libsdi::PED, libsdi::Dialog,
                                   // libsdi::SdiCrypt, libsdi::ManualEntry
#include <sdiclient/sdi_nfc.h>    // NFC_PT_*, NFC_Mifare_*

// ABECS 2.20 — especificação da Software Express
#include "ppcomp.h"               // API pública: PP_Open, PP_Close, PP_GetCard...
#include "ppcomp_internal.h"      // tipos internos, constantes PP_*
#include "pp_sdi_bridge.h"        // mapeamento structs SDI ↔ ABECS
```

### Tipos e convenções ABECS
```cpp
// Retornos de função — SEMPRE int
PP_OK             = 0
PP_ERR_INIT       = -1
PP_ERR_PARAM      = -2
PP_ERR_PINPAD     = -31   // CRÍTICO: Erro 31 do CliSiTef = falha de pinpad
PP_ABORT          = -40   // cancelado pelo usuário
PP_ERR_TIMEOUT    = -41
PP_GO_ONLINE      = 1     // ARQC gerado, ir ao autorizador
PP_OFFLINE_APPROVE= 2     // TC gerado offline
PP_OFFLINE_DECLINE= 3     // AAC gerado offline
PP_FALLBACK_CT    = 10    // CTLS falhou, tentar chip CT
PP_MULTIAPP       = 11    // múltiplos AIDs, aguarda seleção
PP_CTLS_WAITING   = 12    // aguardando toque NFC

// Código de moeda Brasil
#define CURRENCY_BRL_CODE   0x0986   // ISO 4217
#define CURRENCY_BRL_EXP    2        // centavos
#define COUNTRY_BRAZIL_CODE 0x0076   // ISO 3166
```

## DOMÍNIOS DE SEGURANÇA — REGRAS ABSOLUTAS

1. **PAN NUNCA em claro** na `libppcomp`. O SDI Server opera em domínio P2PE.
   Qualquer dado de PAN retornado pelo SDI é ofuscado. Não tentar desofuscar.

2. **PIN NUNCA em claro**. O PIN block sai do SDI já criptografado DUKPT via
   `libsdi::SdiCrypt::getEncryptedPin()`. Nunca logar, nunca serializar em claro.

3. **Chaves criptográficas** — nunca hardcodar. Chaves vêm das tabelas ABECS T3
   (CAPKs) e são carregadas via `SDI_CT_StoreCAPKey()` / `SDI_CTLS_StoreCAPKey()`.

4. **Validar todos os inputs** antes de passar ao SDI. Buffers com tamanho
   explícito. Nunca `strcpy`, sempre `memcpy` com verificação de tamanho.

5. **Não fazer strip** dos binários — a `libclisitef.so` valida assinatura
   criptográfica dos `.so` em tempo de execução.

## ESTADO GLOBAL E THREAD SAFETY

O SDI Server aceita apenas uma conexão por processo. O estado global é:

```cpp
// Estado interno — ppcomp_internal.h
enum PPState {
    PP_STATE_IDLE,
    PP_STATE_DETECTING,
    PP_STATE_CARD_CT,
    PP_STATE_CARD_CTLS,
    PP_STATE_CARD_MSR,
    PP_STATE_EMV_CT_STARTED,
    PP_STATE_EMV_CT_OFFLINE,
    PP_STATE_EMV_CT_ONLINE,
    PP_STATE_EMV_CTLS_SETUP,
    PP_STATE_EMV_CTLS_POLLING,
    PP_STATE_PIN,
    PP_STATE_MANUAL_ENTRY,
};

// Variáveis globais — acesso SOMENTE pela thread da libclisitef
extern PPState         g_pp_state;
extern unsigned char   g_current_tec;   // TEC_CHIP_CT | TEC_CTLS | TEC_MSR
extern bool            g_initialized;
extern bool            g_pin_entered;
extern bool            g_card_removed_mid_txn;
extern libsdi::CardDetection g_cardDetection;
extern libsdi::PED           g_ped;
```

A `libclisitef.so` chama as funções PP_ de uma **única thread**. O SDI pode
disparar callbacks de outras threads — usar `std::mutex g_sdi_callback_mutex`
para proteger estado compartilhado entre callbacks e thread principal.

## MAPEAMENTO SDI ↔ ABECS — REFERÊNCIA RÁPIDA

### Inicialização
| ABECS          | SDI                               |
|----------------|-----------------------------------|
| PP_Open        | SDI_ProtocolInit + CT/CTLS_Init   |
| PP_Close       | CT/CTLS_Exit + SDI_Disconnect     |
| PP_SetTermData | SDI_CT/CTLS_SetTermData           |
| PP_LoadAIDs    | SDI_CT_SetAppliData × N           |
| PP_LoadCAPKs   | SDI_CT/CTLS_StoreCAPKey × N       |
| [após tabelas] | CT/CTLS_ApplyConfiguration()      |

### Por transação
| ABECS                | SDI                                    |
|----------------------|----------------------------------------|
| PP_StartCheckEvent   | CardDetection::startSelection          |
| PP_CheckEvent        | CardDetection::pollTechnology          |
| PP_AbortCheckEvent   | CardDetection::stopSelection           |
| PP_GetCard (MSR)     | CardDetection::startMsrRead            |
| PP_GoOnChip          | SDI_CT_StartTransaction                |
| PP_GoOnChipContinue  | SDI_CT_ContinueOffline                 |
| PP_FinishChip        | SDI_CT_ContinueOnline                  |
| PP_EndChip           | SDI_CT_EndTransaction                  |
| PP_GoOnChipCTLS      | SDI_CTLS_SetupTransaction              |
| PP_PollCTLS          | SDI_CTLS_ContinueOffline               |
| PP_FinishChipCTLS    | SDI_CTLS_ContinueOnline                |
| PP_GetPIN            | PED::startPinInput (bloqueante)        |
| PP_StartGetPIN       | PED::startPinEntry (async)             |
| PP_PollGetPIN        | PED::pollPinEntry                      |
| PP_AbortGetPIN       | PED::stopPinEntry                      |
| PP_Abort             | stopSelection + CT/CTLS_Break + stop   |

### Tags EMV usadas para comprovante (SDI_fetchTxnTags)
```cpp
static const unsigned long COMPROVANTE_TAGS[] = {
    0x9F26,   // Application Cryptogram (AC)
    0x9F36,   // Application Transaction Counter (ATC)
    0x9F10,   // Issuer Application Data (IAD)
    0x9F37,   // Unpredictable Number
    0x84,     // Dedicated File Name (AID selecionado)
    0x9F1A,   // Terminal Country Code
    0x5F2A,   // Transaction Currency Code
    0x9A,     // Transaction Date
    0x9C,     // Transaction Type
    0x9F02,   // Amount Authorized
    0x5F34,   // PAN Sequence Number
    0x9F27,   // Cryptogram Information Data
    0x9F06,   // Application Identifier (AID)
    0x9F12,   // Application Preferred Name
    0x50,     // Application Label
};
```

## PADRÕES DE CÓDIGO OBRIGATÓRIOS

### Tratamento de erros SDI
```cpp
// SEMPRE verificar retorno EMV_ADK_INFO
EMV_ADK_INFO ret = SDI_CT_StartTransaction(...);
if (ret != EMV_ADK_OK && ret != EMV_ADK_GO_ONLINE && ret != EMV_ADK_MULTI_APP) {
    LOG_PP_ERROR("SDI_CT_StartTransaction failed: 0x%X", ret);
    return mapEmvRetToAbecs(ret);  // NUNCA retornar código SDI diretamente
}
```

### Logging — sem dados sensíveis
```cpp
// LOG_PP_DEBUG, LOG_PP_INFO, LOG_PP_ERROR — definidos em ppcomp_internal.h
// Usar __android_log_print com tag "libppcomp"
// NUNCA logar: PAN, PIN, trilhas em claro, chaves

LOG_PP_INFO("StartTransaction: amount=%ld txnType=0x%02X", amount, txnType);
// OK: logar amount, txnType, AID selecionado (sem PAN)
```

### Buffers e tamanhos
```cpp
// SEMPRE usar tamanhos explícitos
// NUNCA: char buf[256]; strcpy(buf, src);
// SEMPRE:
constexpr size_t MAX_TLV_BUF = 512;
unsigned char tlvBuf[MAX_TLV_BUF];
unsigned long tlvLen = sizeof(tlvBuf);
```

### Inicialização de structs SDI
```cpp
// SEMPRE inicializar com zero antes de preencher
EMV_CT_TERMDATA_STRUCT termData = {};      // zero-init
EMV_CT_STARTRESULT_STRUCT startResult{};   // idem
```

## O QUE NUNCA FAZER

- NÃO usar `EMV_CT_SmartISO()` ou `EMV_CTLS_SmartISO()` — bloqueados pelo SDI
- NÃO chamar `SDI_CT_fetchTxnTags()` ou `EMV_CT_fetchTxnTags()` diretamente —
  usar SEMPRE `SDI_fetchTxnTags()` (unified)
- NÃO fazer strip dos binários
- NÃO expor PAN ou PIN fora do domínio SDI
- NÃO criar threads próprias — a libclisitef é single-thread
- NÃO fazer `dlopen` de outras bibliotecas dentro da libppcomp
- NÃO usar Java/JNI — esta lib é C++ puro, sem Android runtime

## REFERÊNCIAS EXTERNAS

- ABECS 2.20: `docs/ABECS_Pinpad_Protocolo_v2.20.pdf`
- ADK SDI Client Guide: `docs/ADK_SDI_Client_Programmers_Guide.pdf`
- ADK EMV CT Guide: `docs/ADK_EMV_Contact_Programmers_Guide.pdf`
- ADK EMV CTLS Guide: `docs/ADK_EMV_Contactless_Programmers_Guide.pdf`
- Plano técnico completo: `docs/libppcomp_plano_tecnico.md`
