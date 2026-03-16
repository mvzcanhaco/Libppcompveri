# Mapa de Dependências e Sequência de Desenvolvimento

## ORDEM DE EXECUÇÃO DOS TASKS

O agente Copilot DEVE executar os tasks nesta ordem.
Cada task depende dos anteriores estarem completos e compilando.

```
00-bootstrap
    ├── Cria: headers, globals, CMakeLists, .clang-format
    └── Valida: compila sem erros (apenas globals — sem implementações ainda)
            │
            ▼
01-scaffold-headers (task: scaffold-headers)
    ├── Cria: include/ppcomp_internal.h, include/pp_sdi_bridge.h
    └── Valida: headers incluídos em globals.cpp sem erros de compilação
            │
            ▼
02-lifecycle (task: implement-lifecycle)
    ├── Cria: src/ppcomp_lifecycle.cpp, tests/test_lifecycle.cpp
    ├── Depende: ppcomp_internal.h, pp_sdi_bridge.h
    └── Valida: testes passam com mock SDI
            │
            ▼
03-callbacks (task: implement-callbacks)
    ├── Cria: src/ppcomp_callbacks.cpp, tests/test_callbacks.cpp
    ├── Depende: lifecycle (usa g_pp_state, registra callbacks em PP_Open)
    └── Valida: testes de callback thread-safety passam
            │
            ▼
04-mapping (task: implement-mapping)
    ├── Cria: src/ppcomp_mapping.cpp, src/ppcomp_errors.cpp, tests/test_mapping.cpp
    ├── Depende: ppcomp_internal.h (constantes PP_*)
    └── Valida: cobertura 100% de mapEmvRetToAbecs
            │
            ▼
05-tables (task: implement-tables)
    ├── Cria: src/ppcomp_tables.cpp, tables/table_loader.cpp
    ├── Depende: lifecycle (PP_Open deve ter sido chamado), mapping
    └── Valida: carga de 20 AIDs + 50 CAPKs sem erro, ApplyConfiguration chamado
            │
            ▼
06-detection (task: implement-detection)
    ├── Cria: src/ppcomp_detection.cpp, tests/test_detection.cpp
    ├── Depende: tables (tabelas carregadas antes de detectar), callbacks, mapping
    └── Valida: detecção CT/CTLS/MSR, abort, timeout
            │
            ├──────────────────────────────────┐
            ▼                                  ▼
07-emv-ct (task: implement-emv-ct)     08-emv-ctls (task: implement-emv-ctls)
    ├── Cria: ppcomp_emv_ct.cpp             ├── Cria: ppcomp_emv_ctls.cpp
    ├── Depende: detection, callbacks       ├── Depende: detection, callbacks
    └── Valida: 5 cenários de fluxo CT      └── Valida: toque, fallback, mobile
            │                                          │
            └───────────────────┬──────────────────────┘
                                ▼
                    09-pin (task: implement-pin)
                        ├── Cria: ppcomp_pin.cpp, tests/test_pin.cpp
                        ├── Depende: emv_ct (PIN chamado durante ContinueOffline)
                        └── Valida: bloqueante, async, bypass, errado, bloqueado
                                │
                                ▼
                    10-build (task: configure-build)
                        ├── Cria: CMakeLists.txt completo, build.gradle, scripts
                        ├── Depende: todos os .cpp existem
                        └── Valida: build Android arm64-v8a sucesso, doNotStrip
                                │
                                ▼
                    11-integration (task: implement-integration-tests)
                        ├── Cria: tests/integration/, mocks/
                        ├── Depende: todos os módulos implementados
                        └── Valida: 18 TCs de homologação SE passam
```

## CRITÉRIOS DE CONCLUSÃO DE CADA FASE

### Fase 0-1: Estrutura
- Compila sem warnings em x86_64 com `-Wall -Werror`
- Headers incluíveis sem order dependency
- `clang-format` sem diff

### Fase 2-4: Core
- `test_lifecycle`, `test_callbacks`, `test_mapping` passam 100%
- `PP_ERR_PINPAD (-31)` nunca retornado sem log de causa
- `mapEmvRetToAbecs` cobre todos os valores EMV_ADK_* conhecidos

### Fase 5: Tabelas
- Carga de tabelas de homologação (fixtures) sem erro SDI
- `g_tables_loaded = true` somente após `ApplyConfiguration` bem-sucedido
- PP_GoOnChip retorna PP_ERR_STATE se tabelas não carregadas

### Fase 6-8: Transação
- Fluxo completo CT: StartTransaction → ContinueOffline → ContinueOnline → End
- Fluxo completo CTLS: Setup → Poll(×N NO_CARD) → ContinueOffline(OK) → End
- Fallback CTLS→CT funciona e não corrompe g_pp_state

### Fase 9: PIN
- PIN block disponível via PP_GetPINBlock após PP_GetPIN
- KSN disponível (10 bytes)
- Nenhum dado de PIN aparece em logs mesmo com LOG_LEVEL=DEBUG

### Fase 10-11: Build e Integração
- `libppcomp.so` arm64-v8a gerada, não stripped
- Todos os 18 TCs de homologação passam
- Build time < 2 minutos em CI

## DEFINIÇÃO DE "PRONTO" PARA ENTREGA

Antes de enviar para homologação na Software Express:

1. `./scripts/build.sh --release` sucesso
2. `nm build/arm64-v8a/libppcomp.so | grep " T PP_"` mostra todos os símbolos:
   PP_Open, PP_Close, PP_GetCard, PP_GoOnChip, PP_GoOnChipContinue,
   PP_FinishChip, PP_EndChip, PP_GetPIN, PP_Abort, PP_GetInfo,
   PP_SetTermData, PP_LoadAIDTable, PP_LoadCAPKTable
3. `strings build/arm64-v8a/libppcomp.so | grep -iE "PAN|track_clear"` vazio
4. VERSIONS.TXT presente e correto
5. Todos os 18 TCs de homologação passando
6. Zero warnings em CI
