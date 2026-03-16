# Padrões de Código — libppcomp

## REGRAS INEGOCIÁVEIS

Estas regras são verificadas em code review e CI. Qualquer violação
bloqueia o merge.

---

### REGRA 1: Zero dados sensíveis em logs

```cpp
// ❌ PROIBIDO — nunca logar PAN, PIN, trilhas, chaves
LOG_PP_INFO("Card PAN: %s", pan_string);
LOG_PP_DEBUG("PIN block: %02X%02X...", pinBlock[0], pinBlock[1]);
LOG_PP_INFO("Track2: %s", track2_data);

// ✅ CORRETO — logar apenas metadados
LOG_PP_INFO("Card detected, technology: %d", tech);
LOG_PP_INFO("PIN entry complete, pinDigits: %d", g_pin_digit_count);
LOG_PP_INFO("MSR read complete, track2_present: %s",
            track2_len > 0 ? "yes" : "no");
```

### REGRA 2: Inicialização de structs SDI

```cpp
// ❌ PROIBIDO — struct não inicializada
EMV_CT_TERMDATA_STRUCT termData;
termData.TerminalType = 0x22;
// Outros campos têm lixo de memória

// ✅ CORRETO — sempre zero-init
EMV_CT_TERMDATA_STRUCT termData = {};
termData.TerminalType = 0x22;
// Outros campos são zero por padrão
```

### REGRA 3: Verificação de retorno SDI

```cpp
// ❌ PROIBIDO — ignorar retorno
SDI_CT_SetTermData(&termData);
SDI_CT_SetAppliData(&appData, 0);

// ✅ CORRETO — verificar TODOS os retornos
EMV_ADK_INFO ret = SDI_CT_SetTermData(&termData);
if (ret != EMV_ADK_OK) {
    LOG_PP_ERROR("SDI_CT_SetTermData failed: 0x%X", static_cast<int>(ret));
    return PP_ERR_PARAM;
}
```

### REGRA 4: Retorno sempre via mapEmvRetToAbecs

```cpp
// ❌ PROIBIDO — retornar código SDI diretamente
return static_cast<int>(emv_ret);  // código SDI ≠ código ABECS

// ✅ CORRETO — sempre mapear
return mapEmvRetToAbecs(emv_ret);
```

### REGRA 5: Verificação de pré-condições

```cpp
// ❌ PROIBIDO — chamar SDI sem verificar estado
int PP_GoOnChip(int idPP, long amount, ...) {
    EMV_CT_STARTRESULT_STRUCT result = {};
    SDI_CT_StartTransaction(amount, ...);  // pode travar se não inicializado
}

// ✅ CORRETO — verificar estado antes
int PP_GoOnChip(int idPP, long amount, ...) {
    if (!g_initialized) {
        LOG_PP_ERROR("PP_GoOnChip: not initialized");
        return PP_ERR_INIT;
    }
    if (g_current_tec != TEC_CHIP_CT) {
        LOG_PP_ERROR("PP_GoOnChip: wrong technology: %d", g_current_tec);
        return PP_ERR_NOTCHIP;
    }
    if (g_pp_state != PP_STATE_CARD_CT) {
        LOG_PP_ERROR("PP_GoOnChip: wrong state: %d", g_pp_state);
        return PP_ERR_STATE;
    }
    // ... agora seguro chamar SDI
}
```

### REGRA 6: Bounds checking em buffers

```cpp
// ❌ PROIBIDO — tamanho não verificado
memcpy(outBuf, &transResult, sizeof(transResult));  // outBuf pode ser menor

// ✅ CORRETO — verificar antes de escrever
constexpr int TRANSRESULT_SIZE = 64;  // tamanho do buffer ABECS de resultado
if (*outLen < TRANSRESULT_SIZE) {
    LOG_PP_ERROR("Output buffer too small: %d < %d", *outLen, TRANSRESULT_SIZE);
    return PP_ERR_BUFFER;
}
// Agora seguro escrever
```

### REGRA 7: Mutex nos callbacks

```cpp
// ❌ PROIBIDO — modificar estado global sem proteção em callback
static EMV_ADK_INFO cb_emv_ct(unsigned char* data, unsigned long len) {
    g_candidate_count = parseCount(data);  // race condition!
    return EMV_ADK_OK;
}

// ✅ CORRETO — proteger com mutex
static EMV_ADK_INFO cb_emv_ct(unsigned char* data, unsigned long len) {
    std::lock_guard<std::mutex> lock(g_sdi_callback_mutex);
    g_candidate_count = parseCount(data);
    return EMV_ADK_OK;
}
```

### REGRA 8: extern "C" nos símbolos exportados

```cpp
// ❌ PROIBIDO — símbolo C++ com name mangling
int PP_Open(int portaFisica, char* parametros) { ... }
// dlsym("PP_Open") vai falhar — o símbolo real é algo como _Z7PP_OpeniPc

// ✅ CORRETO — extern "C" para compatibilidade com libclisitef.so
extern "C" {
    int PP_Open(int portaFisica, char* parametros);
    int PP_Close(int idPP);
    int PP_GetCard(int idPP, ...);
    // ... todos os PP_* que a libclisitef resolve via dlsym
}
```

### REGRA 9: ApplyConfiguration após tabelas

```cpp
// ❌ PROIBIDO — esquecer ApplyConfiguration
int PP_LoadCAPKTable(ABECS_CAPK_RECORD* keys, int count) {
    for (int i = 0; i < count; i++) {
        SDI_CT_StoreCAPKey(&key, opts);
        SDI_CTLS_StoreCAPKey(&ctlsKey, opts);
    }
    return PP_OK;  // ERRO: configuração não persistida no SDI Server!
}

// ✅ CORRETO — sempre chamar ApplyConfiguration no final
int PP_LoadCAPKTable(ABECS_CAPK_RECORD* keys, int count) {
    for (int i = 0; i < count; i++) {
        SDI_CT_StoreCAPKey(&key, opts);
        SDI_CTLS_StoreCAPKey(&ctlsKey, opts);
    }
    // OBRIGATÓRIO: persiste config nos kernels EMV
    SDI_CT_ApplyConfiguration();
    SDI_CTLS_ApplyConfiguration();
    g_tables_loaded = true;
    return PP_OK;
}
```

---

## CHECKLIST DE CODE REVIEW

Para cada PR na libppcomp, verificar:

### Segurança
- [ ] Nenhum dado sensível (PAN, PIN, trilhas, chaves) em logs ou strings
- [ ] Todos os buffers têm bounds checking antes de escrita
- [ ] Nenhum `strcpy`, `sprintf`, `gets` — usar `memcpy` com tamanho explícito
- [ ] Símbolos exportados têm `extern "C"` no header ppcomp.h

### Corretude SDI
- [ ] Todas as structs SDI inicializadas com `= {}`
- [ ] Todos os retornos `EMV_ADK_INFO` verificados
- [ ] Nenhum código SDI retornado diretamente (sempre `mapEmvRetToAbecs`)
- [ ] `ApplyConfiguration()` chamado após carga de tabelas
- [ ] Estado `g_pp_state` atualizado em todos os caminhos de código

### Thread Safety
- [ ] Callbacks SDI usam `std::lock_guard<std::mutex> g_sdi_callback_mutex`
- [ ] Nenhuma variável global modificada fora da thread principal sem mutex
- [ ] Callbacks são rápidos (< 1ms) — sem I/O ou alocação

### Compilação
- [ ] Zero warnings com `-Wall -Werror`
- [ ] `-fno-rtti -fno-exceptions` não causam problemas
- [ ] Compila para `arm64-v8a` com NDK r26b
- [ ] `doNotStrip` configurado no build.gradle

### Testes
- [ ] Novo código tem testes unitários correspondentes
- [ ] Testes cobrem pelo menos: sucesso, falha de pré-condição, erro SDI
- [ ] Testes rodam em < 500ms no total
- [ ] Nenhum teste usa hardware real ou SDI real

---

## CONVENÇÕES DE NOMENCLATURA

```
Funções exportadas (ABI ABECS):    PP_NomeCamelCase()
Funções internas C++:              pp_nome_snake_case()
Constantes/macros:                 PP_CONSTANTE_MAIUSCULO
Structs ABECS:                     ABECS_NOME_STRUCT
Structs SDI (Verifone):            EMV_CT_NOME_STRUCT / EMV_CTLS_NOME_STRUCT
Variáveis globais:                 g_nome_snake_case
Parâmetros:                        nomeCamelCase (sem prefixo)
Constantes de timeout (ms):        TIMEOUT_NOME_MS
Constantes de tamanho:             MAX_NOME_SIZE ou NOME_LEN
```

---

## ESTRUTURA PADRÃO DE ARQUIVO .cpp

```cpp
// src/ppcomp_[modulo].cpp
// Descrição: [uma linha descrevendo o módulo]
//
// Mapeamento ABECS 2.20 → SDI:
//   PP_FuncaoA() → SDI_FuncaoX()
//   PP_FuncaoB() → SDI_FuncaoY()

#include <cstring>
#include <cstdint>
#include <mutex>

// ADK Verifone
#include <sdiclient/sdi_emv.h>
#include <sdiclient/sdi_if.h>

// libppcomp
#include "ppcomp.h"
#include "ppcomp_internal.h"
#include "pp_sdi_bridge.h"

// ─── Estado do módulo ─────────────────────────────────────────────────────

// [variáveis de estado específicas deste módulo, se houver]

// ─── Funções internas ─────────────────────────────────────────────────────

static [retorno] pp_helper_interno([params]) {
    // implementação
}

// ─── API ABECS (extern "C") ───────────────────────────────────────────────

extern "C" {

int PP_FuncaoA(int idPP, ...) {
    LOG_PP_INFO("PP_FuncaoA: entry, state=%d", g_pp_state);

    // 1. Verificar pré-condições
    if (!g_initialized) return PP_ERR_INIT;
    if (g_pp_state != PP_STATE_ESPERADO) return PP_ERR_STATE;

    // 2. Preparar structs SDI
    EMV_[...] sdiStruct = {};

    // 3. Chamar SDI
    EMV_ADK_INFO ret = SDI_[...](&sdiStruct, ...);

    // 4. Tratar retorno
    if (ret != EMV_ADK_OK) {
        LOG_PP_ERROR("PP_FuncaoA: SDI failed: 0x%X", static_cast<int>(ret));
        return mapEmvRetToAbecs(ret);
    }

    // 5. Atualizar estado
    g_pp_state = PP_STATE_NOVO;

    LOG_PP_INFO("PP_FuncaoA: exit OK");
    return PP_OK;
}

} // extern "C"
```
