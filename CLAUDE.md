# CLAUDE.md — libppcomp

Guia para o Claude Code trabalhar neste projeto. Leia este arquivo **completamente** antes de qualquer implementação.

---

## IDENTIDADE DO PROJETO

**libppcomp** é uma biblioteca C++17 que implementa a interface **ABECS 2.20**
(Biblioteca Compartilhada para Pinpad) sobre o SDK nativo da Verifone
(**ADK 5.0.3 / SDI — Secure Device Interface**) para o terminal **V660P-A (VAOS/VOS3)**.

Esta biblioteca é o elo entre `libclisitef.so` (Software Express) e o hardware do V660P-A.

### Cadeia completa de chamadas

```
App Android (Kotlin/Java)
  └─ m-SiTef.apk → clisitef-android.jar
       └─ libclisitef.so  [dlopen("libppcomp.so")]
            └─ libppcomp.so  ◄── ESTE PROJETO (C++17, arm64-v8a)
                 └─ libsdiclient.a + libEMV_CT_Client.a + libEMV_CTLS_Client.a
                      └─ SDI Server (socket 127.0.0.1:12000)
                           └─ V660P-A: EMV chip · NFC · MSR · PED · Display
```

---

## ESPECIFICAÇÕES TÉCNICAS OBRIGATÓRIAS

| Item | Valor |
|------|-------|
| Linguagem | C++17 |
| NDK | 26.2.11394342 (r26b) |
| ABI alvo | `arm64-v8a` SOMENTE |
| STL | `c++_shared` |
| API mínima Android | 28 |
| Exceções | PROIBIDAS (`-fno-exceptions`) |
| RTTI | PROIBIDO (`-fno-rtti`) |
| Strip | PROIBIDO — libclisitef valida assinatura criptográfica |

---

## DOMÍNIOS DE SEGURANÇA — REGRAS ABSOLUTAS

1. **PAN NUNCA em claro** — SDI Server opera em P2PE. Dados de PAN são ofuscados.
2. **PIN NUNCA em claro** — PIN block sai do SDI já criptografado DUKPT.
3. **Chaves criptográficas** — nunca hardcodar.
4. **Validar todos os inputs** — usar `memcpy` com tamanho explícito. Nunca `strcpy`.
5. **Não fazer strip** dos binários.

---

## ESTRUTURA DO PROJETO

```
libppcomp/
├── include/
│   ├── ppcomp_internal.h      ← constantes PP_*, enum PPState, structs ABECS  [CRIADO]
│   ├── pp_sdi_bridge.h        ← funções de mapeamento SDI ↔ ABECS              [CRIADO]
│   └── ppcomp.h               ← API pública PP_* (Software Express — NÃO criar)
├── src/
│   ├── ppcomp_globals.cpp     ← variáveis globais                               [CRIADO]
│   ├── ppcomp_lifecycle.cpp   ← PP_Open, PP_Close, PP_Reset, PP_GetInfo, PP_Abort
│   ├── ppcomp_callbacks.cpp   ← callbacks SDI (cb_emv_ct, cb_sdi_status, etc.)
│   ├── ppcomp_mapping.cpp     ← mapEmvRetToAbecs, pp_serialize_*, pp_tlv_*
│   ├── ppcomp_errors.cpp      ← PP_GetErrorDescription()
│   ├── ppcomp_tables.cpp      ← PP_SetTermData, PP_LoadAIDTable, PP_LoadCAPKTable
│   ├── ppcomp_detection.cpp   ← PP_StartCheckEvent, PP_CheckEvent, PP_AbortCheckEvent
│   ├── ppcomp_emv_ct.cpp      ← PP_GoOnChip, PP_GoOnChipContinue, PP_FinishChip
│   ├── ppcomp_emv_ctls.cpp    ← PP_GoOnChipCTLS, PP_PollCTLS, PP_FinishChipCTLS
│   └── ppcomp_pin.cpp         ← PP_GetPIN, PP_StartGetPIN, PP_GetPINBlock
├── tests/
│   ├── CMakeLists.txt                         [CRIADO]
│   ├── fixtures/
│   ├── mocks/sdi_mock.h + sdi_mock.cpp
│   ├── integration/test_homologacao_se.cpp    ← 18 TCs Software Express
│   └── test_*.cpp                             ← um arquivo por módulo
├── tables/table_loader.h + table_loader.cpp
├── libs/arm64-v8a/          ← libs Verifone (acesso restrito — não comitar)
├── libs/include/sdiclient/  ← headers ADK Verifone (acesso restrito)
├── docs/                    ← PDFs confidenciais — não comitar
├── scripts/build.sh                           [CRIADO]
├── CMakeLists.txt                             [CRIADO]
├── .clang-format                              [CRIADO]
├── VERSIONS.TXT                               [CRIADO]
└── .github/
    ├── copilot-instructions.md
    ├── copilot/tasks.yml
    └── workflows/build.yml                    [CRIADO]
```

---

## HEADERS DISPONÍVEIS

```cpp
// ADK Verifone (em libs/include/ — acesso restrito)
#include <sdiclient/sdi_emv.h>    // SDI_CT_*, SDI_CTLS_*, SDI_fetchTxnTags
#include <sdiclient/sdi_if.h>     // libsdi::SDI, CardDetection, PED, SdiCrypt
#include <sdiclient/sdi_nfc.h>    // NFC_PT_*

// libppcomp (criados no bootstrap)
#include "ppcomp_internal.h"      // constantes PP_*, PPState, structs ABECS
#include "pp_sdi_bridge.h"        // funções de mapeamento

// Software Express (NÃO criar — fornecido externamente)
#include "ppcomp.h"               // PP_Open, PP_Close, PP_GetCard...
```

---

## MAPEAMENTO SDI ↔ ABECS

### Inicialização
| ABECS | SDI |
|-------|-----|
| PP_Open | SDI_ProtocolInit + CT/CTLS_Init_Framework |
| PP_Close | CT/CTLS_Exit + SDI_Disconnect |
| PP_SetTermData | SDI_CT/CTLS_SetTermData |
| PP_LoadAIDs | SDI_CT_SetAppliData × N |
| PP_LoadCAPKs | SDI_CT/CTLS_StoreCAPKey × N |
| [após tabelas] | **CT/CTLS_ApplyConfiguration()** — OBRIGATÓRIO |

### Por transação
| ABECS | SDI |
|-------|-----|
| PP_StartCheckEvent | CardDetection::startSelection |
| PP_CheckEvent | CardDetection::pollTechnology |
| PP_GoOnChip | SDI_CT_StartTransaction |
| PP_GoOnChipContinue | SDI_CT_ContinueOffline |
| PP_FinishChip | SDI_CT_ContinueOnline |
| PP_EndChip | SDI_CT_EndTransaction |
| PP_GoOnChipCTLS | SDI_CTLS_SetupTransaction |
| PP_PollCTLS | SDI_CTLS_ContinueOffline |
| PP_GetPIN | PED::startPinInput (bloqueante) |
| PP_Abort | stopSelection + CT/CTLS_Break |

---

## CÓDIGOS DE RETORNO

```cpp
PP_OK               = 0;    PP_GO_ONLINE        = 1;
PP_OFFLINE_APPROVE  = 2;    PP_OFFLINE_DECLINE  = 3;
PP_FALLBACK_CT      = 10;   PP_MULTIAPP         = 11;   PP_CTLS_WAITING = 12;
PP_ERR_INIT         = -1;   PP_ERR_PARAM        = -2;   PP_ERR_STATE    = -3;
PP_ERR_PINPAD       = -31;  // CRÍTICO: erro 31 do CliSiTef
PP_ABORT            = -40;  PP_ERR_TIMEOUT      = -41;
PP_ERR_WRONG_PIN    = -42;  PP_ERR_PIN_BLOCKED  = -43;  PP_ERR_CARD_BLOCKED = -44;
```

---

## 9 REGRAS INEGOCIÁVEIS

```cpp
// 1. Zero dados sensíveis em logs
LOG_PP_INFO("technology: %d", tech);        // OK
LOG_PP_INFO("PAN: %s", pan);                // PROIBIDO

// 2. Inicializar structs SDI com zero
EMV_CT_TERMDATA_STRUCT termData = {};       // sempre

// 3. Verificar todos os retornos SDI
EMV_ADK_INFO ret = SDI_CT_SetTermData(&td);
if (ret != EMV_ADK_OK) { ...; return PP_ERR_PARAM; }

// 4. NUNCA retornar código SDI diretamente
return mapEmvRetToAbecs(ret);              // sempre

// 5. Verificar pré-condições
if (!g_initialized) return PP_ERR_INIT;
if (g_pp_state != PPState::CARD_CT) return PP_ERR_STATE;

// 6. Bounds checking
if (*outLen < REQUIRED) return PP_ERR_BUFFER;

// 7. Mutex em callbacks SDI
static EMV_ADK_INFO cb_emv_ct(...) {
    std::lock_guard<std::mutex> lock(g_sdi_callback_mutex);
    // ...
}

// 8. extern "C" nos símbolos exportados
extern "C" { int PP_Open(...); }

// 9. ApplyConfiguration após tabelas
SDI_CT_ApplyConfiguration(); SDI_CTLS_ApplyConfiguration();
g_tables_loaded = true;
```

---

## ESTADO GLOBAL

Definido em `src/ppcomp_globals.cpp`, declarado em `ppcomp_internal.h`:

| Variável | Tipo | Significado |
|----------|------|-------------|
| `g_pp_state` | `PPState` | Estado do autômato |
| `g_current_tec` | `uint8_t` | Tecnologia detectada (TEC_CHIP_CT/TEC_CTLS/TEC_MSR) |
| `g_initialized` | `bool` | PP_Open chamado com sucesso |
| `g_tables_loaded` | `bool` | Tabelas carregadas + ApplyConfig feito |
| `g_pin_entered` | `bool` | PIN capturado |
| `g_card_removed_mid_txn` | `bool` | Cartão removido durante transação |
| `g_sdi_callback_mutex` | `std::mutex` | Protege estado em callbacks |

---

## CONVENÇÕES DE NOMENCLATURA

| Tipo | Convenção | Exemplo |
|------|-----------|---------|
| Funções exportadas (ABI ABECS) | `PP_NomeCamelCase()` | `PP_GoOnChip` |
| Funções internas C++ | `pp_nome_snake_case()` | `pp_serialize_trans_result` |
| Constantes/macros | `PP_CONSTANTE_MAIUSCULO` | `PP_ERR_TIMEOUT` |
| Structs ABECS | `ABECS_NOME_STRUCT` | `ABECS_TRANSRESULT` |
| Variáveis globais | `g_nome_snake_case` | `g_pp_state` |

---

## ORDEM DE IMPLEMENTAÇÃO

Ver `.copilot/tasks/dependency-map.md` para o DAG completo.

```
00-bootstrap         CONCLUIDO (headers, globals, CMakeLists, CI)
  ↓
01-lifecycle         src/ppcomp_lifecycle.cpp + tests/test_lifecycle.cpp
  ↓
02-callbacks         src/ppcomp_callbacks.cpp + tests/test_callbacks.cpp
  ↓
03-mapping           src/ppcomp_mapping.cpp + src/ppcomp_errors.cpp
  ↓
04-tables            src/ppcomp_tables.cpp + tables/table_loader.cpp
  ↓
05-detection         src/ppcomp_detection.cpp
  ↓
06-emv-ct ─────┐
               ↓
07-emv-ctls ───┤
               ↓
            08-pin
               ↓
            09-build (CMakeLists.txt completo + build.gradle)
               ↓
            10-integration-tests (18 TCs de homologação SE)
```

Especificação detalhada de cada task: `.github/copilot/tasks.yml`

---

## COMANDOS DE BUILD

```bash
./scripts/build.sh --test      # testes x86_64, sem hardware
./scripts/build.sh --android   # build arm64-v8a (requer NDK + libs Verifone)
./scripts/build.sh --release   # release (requer assinatura Verifone)
```

---

## O QUE NUNCA FAZER

- NÃO usar `EMV_CT_SmartISO()` / `EMV_CTLS_SmartISO()` — bloqueados pelo SDI
- NÃO chamar `SDI_CT_fetchTxnTags()` — usar sempre `SDI_fetchTxnTags()` (unified)
- NÃO fazer strip dos binários
- NÃO criar threads próprias — libclisitef é single-thread
- NÃO fazer dlopen de outras libs dentro da libppcomp
- NÃO usar Java/JNI

---

## REFERÊNCIAS SECUNDÁRIAS NESTE REPO

- `.github/copilot-instructions.md` — especificação técnica completa (incluindo tags EMV, tipos ABECS)
- `.github/copilot/tasks.yml` — especificação detalhada de cada um dos 11 tasks
- `.copilot/context/architecture.md` — decisões de design (por que C++, por que socket, por que DETECTION_MODE_BLOCKING)
- `.copilot/context/coding-standards.md` — 9 regras com exemplos e checklist de code review
- `.copilot/tasks/dependency-map.md` — DAG de dependências e critérios de conclusão
- `.copilot/prompts/reusable-prompts.md` — prompts para código, testes, debug
- `.copilot/prompts/emv-flow-cot.md` — chain-of-thought para fluxo EMV CT e CTLS
