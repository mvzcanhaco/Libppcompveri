# Prompt de Bootstrap — Primeiro Passo do Projeto

## INSTRUÇÃO PARA O AGENTE COPILOT

Este é o prompt de inicialização. Execute-o antes de qualquer outro task.

```
Você é o agente de desenvolvimento da libppcomp. Sua missão inicial é criar
o scaffold completo do projeto antes de qualquer implementação.

CONTEXTO: Leia COMPLETAMENTE os seguintes arquivos antes de começar:
1. .github/copilot-instructions.md  — regras absolutas do projeto
2. .copilot/context/architecture.md — por que existe e como funciona
3. .copilot/context/coding-standards.md — padrões de código

PASSO 1 — Criar include/ppcomp_internal.h
Deve conter:
- enum class PPState com TODOS os estados listados em copilot-instructions.md
- Todas as constantes PP_OK, PP_ERR_*, PP_GO_ONLINE, etc. como constexpr int
- Constantes de moeda: CURRENCY_BRL_CODE=0x0986, COUNTRY_BRAZIL_CODE=0x0076
- Macros LOG_PP_DEBUG/INFO/ERROR usando __android_log_print com tag "libppcomp"
  mas NUNCA logando dados sensíveis (PAN, PIN, chaves)
- extern de todas as variáveis globais: g_pp_state, g_current_tec,
  g_initialized, g_tables_loaded, g_pin_entered, g_card_removed_mid_txn,
  g_sdi_callback_mutex, g_cardDetection, g_ped
- Definição de struct ABECS_TERMDATA_STRUCT com todos os campos T1 mapeados
- Definição de struct ABECS_AID_RECORD com campos CT e CTLS separados
- Definição de struct ABECS_CAPK_RECORD
- Definição de struct ABECS_TRANSRESULT
- Definição de struct ABECS_MSR_DATA
- Definição de struct ABECS_CTLS_PARAMS

PASSO 2 — Criar include/pp_sdi_bridge.h
Declarações (não definições) de TODAS as funções de mapeamento:
- mapEmvRetToAbecs, mapTxnTypeAbecsToEmv, mapModalidadeToTec
- pp_serialize_*, pp_parse_*, pp_extract_*, pp_abecs_serialize
- pp_tlv_get_byte, pp_tlv_get_bytes
- pp_save_script_result

PASSO 3 — Criar CMakeLists.txt raiz
Com as configurações exatas de .copilot/tasks/tasks.yml, task "configure-build".
Flags: -std=c++17 -fno-rtti -fno-exceptions -Wall -Werror -Wno-unused-parameter
ABI target: arm64-v8a SOMENTE

PASSO 4 — Criar src/ppcomp_globals.cpp
Definição (não declaração) de todas as variáveis globais.
Inicialização segura:
  PPState g_pp_state = PPState::IDLE;
  unsigned char g_current_tec = 0;
  bool g_initialized = false;
  bool g_tables_loaded = false;
  std::mutex g_sdi_callback_mutex;
  // etc.

PASSO 5 — Criar .clang-format
BasedOnStyle: Google, ColumnLimit: 100, IndentWidth: 4

Após criar todos os arquivos, verifique que:
- Todos os headers têm #pragma once
- Todos os tipos usam uint8_t/uint32_t/int32_t (não int/char para tamanhos)
- Não há dependências circulares entre headers
- ppcomp.h (fornecido pela SE) NÃO foi criado — é fornecido externamente
```

---

## INSTRUÇÃO PARA CI/CD PIPELINE

```yaml
# .github/workflows/build.yml
# Instrução para o Copilot gerar este arquivo:

Gere um GitHub Actions workflow para a libppcomp com os seguintes jobs:

JOB 1: lint
- Rodar clang-format --dry-run --Werror em todos os .cpp e .h
- Rodar cppcheck com --enable=all em src/

JOB 2: test-unit
- Compilar os testes em x86_64 Linux (não Android — usar mocks SDI)
- CMake com -DBUILD_TESTS=ON -DUSE_SDI_MOCK=ON
- Rodar ./build/tests/test_lifecycle, test_tables, test_detection,
  test_emv_ct, test_emv_ctls, test_pin, test_callbacks, test_mapping
- Upload coverage report (lcov)

JOB 3: build-android
- Usar ubuntu-latest com NDK r26b instalado
- cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28
- Verificar que libppcomp.so foi gerada em build/arm64-v8a/libppcomp.so
- Verificar que NÃO foi stripped (nm libppcomp.so | grep -c debug)
- NOTA: não instalar no terminal — só validar build

JOB 4: security-check
- Rodar semgrep com rules/c para buffer overflows e format strings
- Verificar que nenhum símbolo "PAN", "pin_plain", "track_clear" aparece
  nos binários (strings libppcomp.so | grep -iE "pan|pin_plain|track_clear")

Todos os jobs devem rodar em PR e push para main.
Usar cache de NDK e dependências para velocidade.
```
