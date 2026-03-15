# Prompt Chain-of-Thought — Fluxo EMV Completo

## USO

Este prompt deve ser usado quando o Copilot precisar implementar ou debugar
o fluxo EMV completo de ponta a ponta. É um prompt de raciocínio em cadeia
(chain-of-thought) que força o Copilot a pensar em cada etapa antes de codificar.

---

## PROMPT PRINCIPAL

```
Você está implementando o fluxo EMV Contact completo da libppcomp para o
terminal Verifone V660P-A, mapeando ABECS 2.20 para ADK SDI.

Antes de escrever qualquer código, raciocine em voz alta sobre cada passo:

PASSO 1 — VERIFICAÇÃO DE PRÉ-CONDIÇÕES
Quais verificações de estado são necessárias antes de chamar SDI_CT_StartTransaction?
- O terminal está inicializado? (g_initialized)
- O estado está correto? (g_pp_state)
- A tecnologia detectada é chip CT? (g_current_tec == TEC_CHIP_CT)
- As tabelas foram carregadas? (g_tables_loaded)
Se qualquer check falhar: qual código PP_* retornar?

PASSO 2 — MAPEAMENTO DE PARÂMETROS
Como mapear os parâmetros ABECS para SDI_CT_StartTransaction?
- amount: ABECS usa centavos inteiros → SDI também usa centavos (unsigned long)
- cashback: pode ser zero → passar 0UL
- currency: sempre BRL → hardcodar 986
- txnType: como mapear ABECS_TXN_COMPRA=0 → EMV txnType 0x00?
  Fazer tabela completa de mapeamento antes de codificar.

PASSO 3 — TRATAMENTO DO RETORNO STARTRESULT
SDI_CT_StartTransaction pode retornar:
- EMV_ADK_OK: AID único selecionado → o que serializar no outBuf?
- EMV_ADK_SEL_APP_OK: seleção confirmada → mesmo que OK?
- EMV_ADK_MULTI_APP: múltiplos candidatos → como serializar a lista de AIDs?
- EMV_ADK_NO_CARD: cartão removido antes de iniciar → qual PP_* retornar?
- EMV_ADK_FALLBACK: chip inoperante → qual PP_* retornar?
- Outros erros: usar mapEmvRetToAbecs()

PASSO 4 — CALLBACK DE SELEÇÃO DE AID
O callback cb_emv_ct é chamado DURANTE SDI_CT_StartTransaction.
Ele roda em thread separada. Como coordenar com a thread principal?
- Mutex necessário?
- Precisa aguardar seleção do usuário ou auto-seleciona?
- Se EMV_ADK_MULTI_APP: quando a libclisitef chama PP_SetAID?
Explicar o fluxo temporal completo.

PASSO 5 — CONTINUE OFFLINE E DECISÃO
SDI_CT_ContinueOffline retorna a decisão do kernel EMV.
Para cada retorno, qual é a semântica exata:
- EMV_ADK_OK: significa aprovação offline (TC gerado)? Confirmar.
- EMV_ADK_GO_ONLINE vs EMV_ADK_ONLINE: qual a diferença?
- EMV_ADK_DECLINE: AAC definitivo ou pode tentar online?
Mapear authReqCryptoType de transResult para confirmação da decisão.

PASSO 6 — DADOS PARA O COMPROVANTE
Quais tags EMV são necessárias para o comprovante fiscal brasileiro?
Listar todas as tags de SDI_fetchTxnTags necessárias e por quê cada uma.
Atenção especial: a libclisitef vai usar esses dados para o SiTef.

PASSO 7 — CLEANUP E ESTADO FINAL
Após PP_EndChip, qual é o estado correto do sistema?
- g_pp_state = PP_STATE_IDLE ✓
- g_current_tec = TEC_NONE ✓
- g_pin_entered = false ✓
- Aguardar remoção do cartão? Qual timeout?
- E se o cartão já foi removido mid-transaction?

Após raciocinar sobre todos os passos acima, implemente o código.
```

---

## PROMPT CTLS CHAIN-OF-THOUGHT

```
Você está implementando o fluxo CTLS (contactless) da libppcomp.

O CTLS difere do CT em aspectos críticos. Raciocine:

DIFERENÇA 1 — NÃO HÁ SELEÇÃO DE AID EXPLÍCITA
No CT: StartTransaction → callback de seleção de AID → aplicação selecionada
No CTLS: SetupTransaction não seleciona AID — isso acontece DURANTE ContinueOffline
Por quê? Porque no contactless o cartão apresentado decide qual AID usar.
Como isso afeta a implementação de PP_GoOnChipCTLS?

DIFERENÇA 2 — NÃO BLOQUEIA
SetupTransaction retorna imediatamente após ativar polling RF.
O cartão pode ser tocado a qualquer momento após isso.
O loop de polling (PP_PollCTLS) chama ContinueOffline repetidamente.
ContinueOffline retorna EMV_ADK_NO_CARD enquanto não há toque.
Qual é o tempo máximo de polling antes de timeout?
Quem controla o timeout — a libppcomp ou a libclisitef?

DIFERENÇA 3 — CARTÃO MOBILE (WALLET)
EMV_ADK_TXN_CTLS_MOBILE significa que é um dispositivo mobile (Apple Pay, etc.)
Nesses casos, o contactless requer um segundo toque para confirmar.
O SDI gerencia isso internamente ou a libppcomp precisa tratar?
O que retornar para a libclisitef nesse caso? (PP_CTLS_WAITING)

DIFERENÇA 4 — FALLBACK
Se o toque CTLS falha após N tentativas: EMV_ADK_FALLBACK
A libppcomp deve: SDI_CTLS_Break() para limpar o estado CTLS
E então retornar PP_FALLBACK_CT para a libclisitef tentar chip CT.
Qual estado (g_pp_state) fica após o fallback?
A libclisitef vai chamar PP_GoOnChip normalmente após receber PP_FALLBACK_CT?

DIFERENÇA 5 — MAIORIA OFFLINE
No CTLS brasileiro, transações abaixo do limite de piso CTLS são offline puras.
NÃO há PIN. NÃO há ContinueOnline.
O fluxo é: SetupTransaction → ContinueOffline → EndTransaction
Para transações acima do CVM limit: PIN online pode ser solicitado via callback.

Após raciocinar sobre todas as diferenças, implemente PP_GoOnChipCTLS e PP_PollCTLS.
```

---

## PROMPT DEBUG CHAIN-OF-THOUGHT

```
A libppcomp está retornando PP_ERR_PINPAD (-31) em [CONTEXTO].

Este é o código de erro mais crítico — significa que a libclisitef vai reportar
"Erro 31 - ERRO PINPAD" para o m-SiTef, bloqueando a transação.

Raciocine sistematicamente sobre as causas possíveis:

CAUSA 1 — SDI Server não está rodando?
- O SDI Server é um processo do VAOS que inicia com o sistema
- Se não estiver rodando, SDI_ProtocolInit conecta mas as chamadas falham
- Como verificar: SDI_CT_Init_Framework retorna EMV_ADK_COMM_ERROR
- Verificar no log se há "SDI_ProtocolInit failed" ou "connection refused"

CAUSA 2 — ApplyConfiguration não foi chamado?
- Sem ApplyConfiguration, o SDI não conhece os AIDs configurados
- SDI_CT_StartTransaction vai falhar com EMV_ADK_NO_AID ou similar
- Verificar no código: PP_LoadCAPKTable chama ApplyConfiguration no final?

CAUSA 3 — Tabelas não foram carregadas?
- g_tables_loaded = false → PP_GoOnChip retorna PP_ERR_PINPAD por segurança
- Verificar se PP_SetTermData e PP_LoadAIDTable foram chamados antes de transação

CAUSA 4 — Estado incorreto?
- g_pp_state != PP_STATE_CARD_CT quando PP_GoOnChip é chamado
- A libclisitef pode chamar PP_GoOnChip sem PP_CheckEvent ter detectado chip
- Verificar sequência de chamadas no log

CAUSA 5 — Versão incompatível?
- VERSIONS.TXT define CLISITEF_MIN_VERSION compatível com esta libppcomp
- Se a versão da CliSiTef for anterior ao mínimo, comportamento indefinido

Para cada causa: qual log entry confirma ou descarta a hipótese?
Após identificar a causa, sugerir correção específica.
```
