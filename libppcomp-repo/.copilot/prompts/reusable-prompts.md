# Prompts Reutilizáveis — libppcomp

## COMO USAR ESTES PROMPTS

Copie o prompt no chat do GitHub Copilot ou use como comentário `// @copilot`
antes do código a ser gerado. Adapte os parâmetros entre [colchetes].

---

## PROMPT: Implementar nova função PP_*

```
Implemente a função ABECS 2.20 [NOME_DA_FUNCAO] para a libppcomp.

Contexto:
- Esta função mapeia para a chamada SDI [SDI_FUNCAO]
- Arquivo: src/[arquivo].cpp
- Estado esperado de entrada: g_pp_state == [ESTADO]
- Estado esperado de saída: g_pp_state = [NOVO_ESTADO]

A implementação DEVE:
1. Verificar pré-condições (g_initialized, g_pp_state, g_current_tec)
2. Log de entrada com LOG_PP_INFO (sem dados sensíveis)
3. Chamar a função SDI correspondente com structs zero-inicializadas
4. Mapear o retorno EMV_ADK_INFO via mapEmvRetToAbecs()
5. Setar g_pp_state no estado correto
6. Log de saída com resultado
7. Retornar código PP_* correto

Regras absolutas:
- NUNCA retornar código SDI diretamente (sempre mapEmvRetToAbecs)
- NUNCA logar PAN, PIN, trilhas em claro, chaves
- NUNCA usar exceções
- Inicializar todas as structs SDI com = {}
- Verificar TODOS os retornos de função SDI
```

---

## PROMPT: Implementar parser de tabela ABECS

```
Implemente o parser para a tabela ABECS 2.20 [TIPO_TABELA] (T1/T2/T3/T4).

Formato da tabela: [descrever formato binário ou TLV]

O parser deve:
1. Receber um buffer binário e tamanho
2. Validar o header da tabela (versão, checksum)
3. Para cada registro: extrair campos com tamanhos fixos definidos na ABECS 2.20
4. Preencher o array de structs ABECS_[TIPO]_RECORD
5. Retornar quantidade de registros lidos ou código de erro negativo

Campos obrigatórios a validar:
- [listar campos críticos]

Campos opcionais (usar defaults se ausentes):
- [listar campos com defaults]

Incluir verificação de buffer overflow em cada leitura.
Usar memcpy com verificação de bounds — nunca pointer cast direto.
```

---

## PROMPT: Implementar mock SDI para testes

```
Implemente um mock Google Mock para a função SDI [SDI_FUNCAO] a ser usado
nos testes unitários de [COMPONENTE].

O mock deve:
1. Ter assinatura idêntica à função real do libsdiclient
2. Ter helpers para configurar cenários comuns:
   - ConfigureApprovalOffline(): configura mock para retornar TC
   - ConfigureGoOnline(arqc_bytes): configura para retornar ARQC
   - ConfigureDecline(): configura para retornar AAC
   - ConfigureNoCard(): configura para retornar EMV_ADK_NO_CARD
   - ConfigureTimeout(): configura para retornar EMV_ADK_TIMEOUT

3. Verificar que foi chamado com os parâmetros esperados:
   - amount correto (em centavos)
   - currency = 986 (BRL)
   - txnType no range válido

Usar EXPECT_CALL com InSequence para fluxos multi-passo (StartTransaction
→ ContinueOffline → ContinueOnline).

O mock deve rodar em x86_64 Linux (sem NDK) para CI.
```

---

## PROMPT: Implementar cenário de homologação

```
Implemente o teste de homologação [TC_XXX] para a libppcomp.

Cenário: [DESCRIÇÃO DO CENÁRIO]

Setup:
- Inicializar libppcomp com tabelas de homologação (fixtures em tests/fixtures/)
- Configurar mock SDI para o fluxo esperado

Steps:
1. [Passo 1]
2. [Passo 2]
...

Assertions:
- Verificar retorno de cada PP_* call
- Verificar g_pp_state após cada passo
- Verificar que dados sensíveis NÃO aparecem em logs
- Verificar sequência de chamadas SDI (InSequence)

O teste deve:
- Ter nome claro: TEST_F(HomologacaoTest, [TC_XXX_NOME])
- Ter comentário descrevendo o cenário ABECS/EMV
- Executar em < 100ms (sem I/O real, apenas mocks)
- Ser determinístico (sem dependência de tempo real)
```

---

## PROMPT: Serializar struct SDI para buffer ABECS

```
Implemente a função pp_serialize_[NOME] que converte
[STRUCT_SDI] para o formato de buffer ABECS 2.20.

Formato de saída ABECS (descrever layout de bytes):
Offset  Tamanho  Campo
0       1        [campo 1]
1       8        [campo 2 — cryptogram]
...

Regras:
- Little-endian para campos numéricos multi-byte? [SIM/NÃO — verificar ABECS 2.20]
- BCD para datas e valores? [SIM/NÃO]
- Strings: ASCII, sem null terminator, pad com espaços? [verificar]

Verificações obrigatórias:
- *outLen deve ser suficiente para o tamanho total
- Se insuficiente: retornar PP_ERR_BUFFER sem escrever
- Atualizar *outLen com tamanho real escrito

Dados PAN do sdiExtra: usar obfuscatedPAN (nunca PAN real).
```

---

## PROMPT: Diagnóstico de erro

```
Analisando o seguinte erro na libppcomp:

[COLAR OUTPUT DO LOG OU STACK TRACE]

Contexto:
- Estado do terminal no momento: g_pp_state = [ESTADO]
- Tecnologia detectada: g_current_tec = [TEC]
- Última função PP_ chamada: [FUNCAO]
- Código de retorno: [CODIGO]

Por favor:
1. Identificar a causa raiz no mapeamento ABECS → SDI
2. Verificar se o código de retorno SDI foi corretamente mapeado
3. Verificar se o estado g_pp_state foi corretamente atualizado
4. Verificar se ApplyConfiguration() foi chamado após carga de tabelas
5. Sugerir a correção com código C++ específico

Regras: a solução não pode usar exceções, JNI, ou modificar ABI da função PP_.
```

---

## PROMPT: Code review de segurança

```
Faça um code review focado em segurança do seguinte código da libppcomp:

[COLAR CÓDIGO]

Verificar especificamente:
1. PAN ou PIN podem aparecer em claro em algum caminho de execução?
2. Existe buffer overflow possível em alguma operação memcpy/memset?
3. O retorno de TODAS as funções SDI está sendo verificado?
4. Structs SDI estão sendo zero-inicializadas antes do uso?
5. O estado g_pp_state é corretamente atualizado em todos os caminhos?
6. Callbacks SDI podem modificar estado sem mutex?
7. Existe vazamento de recursos (handles SDI não fechados)?

Para cada problema encontrado: indicar linha, severidade (CRÍTICO/ALTO/MÉDIO),
e sugerir correção com código.
```

---

## PROMPT: Gerar tabela de fixtures para testes

```
Gere código C++ para criar fixtures de teste da libppcomp para o cenário
de homologação [NOME_CENARIO].

As fixtures devem conter:
1. ABECS_TERMDATA_STRUCT populada com dados válidos de terminal Brasil
   (BRL, país 0076, tipo 0x22, MCC [CODIGO])

2. Array de 5 ABECS_AID_RECORD representando:
   - Visa Crédito (A0000000031010) com TACs conservadores
   - Mastercard Crédito (A0000000041010)
   - Elo Crédito (A000000651010110) — kernel 7
   - Visa Débito (A0000000032010)
   - Mastercard Débito (A0000000043060)

3. Array de 3 ABECS_CAPK_RECORD com chaves de teste (não produção):
   - RID Visa: A000000003, Index: 0x99 (teste)
   - RID Master: A000000004, Index: 0xEF (teste)
   - RID Elo: A000000651, Index: 0x01 (teste)
   Usar módulos de 512 bits (64 bytes) para rapidez nos testes.

Os dados de chave NÃO precisam ser chaves reais — são apenas para validar
que o carregamento funciona sem erro SDI.

Organizar em tests/fixtures/fixture_[NOME_CENARIO].h como constexpr arrays.
```
