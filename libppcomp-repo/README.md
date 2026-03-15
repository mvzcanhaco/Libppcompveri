# libppcomp — Biblioteca Compartilhada ABECS 2.20 para Verifone V660P-A

> Implementação da interface ABECS 2.20 sobre ADK 5.0.3 / SDI para
> integração com CliSiTef (Software Express) no ecossistema de TEF brasileiro.

## STATUS DO PROJETO

| Módulo              | Status     | Testes |
|---------------------|------------|--------|
| Scaffold/Headers    | ⬜ Pendente | —      |
| Lifecycle           | ⬜ Pendente | —      |
| Callbacks SDI       | ⬜ Pendente | —      |
| Mapeamento/Erros    | ⬜ Pendente | —      |
| Carga de Tabelas    | ⬜ Pendente | —      |
| Detecção de Cartão  | ⬜ Pendente | —      |
| EMV Contato (CT)    | ⬜ Pendente | —      |
| EMV Contactless     | ⬜ Pendente | —      |
| PIN Entry (PED)     | ⬜ Pendente | —      |
| Build System        | ⬜ Pendente | —      |
| Testes Integração   | ⬜ Pendente | —      |

## PARA O AGENTE COPILOT

Se você é o GitHub Copilot Workspace iniciando este projeto:

**1. Leia primeiro:**
```
.github/copilot-instructions.md    ← regras absolutas (leia antes de TUDO)
.copilot/context/architecture.md   ← por que existe e como funciona
.copilot/context/coding-standards.md ← padrões e checklist
```

**2. Siga a ordem dos tasks:**
```
.copilot/tasks/dependency-map.md   ← ordem obrigatória de implementação
.copilot/tasks/00-bootstrap.md     ← comece aqui
.github/copilot/tasks.yml          ← especificação detalhada de cada task
```

**3. Use os prompts para situações específicas:**
```
.copilot/prompts/reusable-prompts.md  ← prompts para código, testes, debug
.copilot/prompts/emv-flow-cot.md      ← chain-of-thought para fluxo EMV
```

## ARQUITETURA EM UMA LINHA

```
Automação → Intent → m-SiTef.apk → clisitef-android.jar →
libclisitef.so → [libppcomp.so] → libsdiclient.a → SDI Server → V660P-A
```

## PRÉ-REQUISITOS DE DESENVOLVIMENTO

| Dependência          | Versão      | Origem                        |
|----------------------|-------------|-------------------------------|
| NDK Android          | r26b        | Android SDK Manager           |
| CMake                | ≥ 3.22      | Android SDK Manager           |
| clang-format         | 14+         | apt/brew                      |
| Google Test          | auto        | CMake FetchContent (CI only)  |
| libsdiclient.a       | ADK 5.0.3   | **Verifone Developer Central** |
| libEMV_CT_Client.a   | ADK 5.0.3   | **Verifone Developer Central** |
| libEMV_CTLS_Client.a | ADK 5.0.3   | **Verifone Developer Central** |
| ppcomp.h             | ABECS 2.20  | **Software Express (parceria)**|

> ⚠️ As bibliotecas Verifone e o header ppcomp.h são de acesso restrito.
> Sem eles, apenas os testes com mock SDI (x86_64) podem ser executados.

## BUILD

```bash
# Testes unitários (x86_64, sem hardware)
./scripts/build.sh --test

# Build Android arm64-v8a (requer NDK + libs Verifone)
./scripts/build.sh --android

# Build de release (requer assinatura Verifone — ver docs/signing.md)
./scripts/build.sh --release
```

## DOCUMENTAÇÃO TÉCNICA

```
docs/
├── libppcomp_plano_tecnico.md    ← plano completo de implementação
├── ABECS_Pinpad_Protocolo_v2.20.pdf     ← spec ABECS (confidencial SE)
├── ADK_SDI_Client_Programmers_Guide.pdf  ← SDK Verifone (acesso restrito)
├── ADK_EMV_Contact_Programmers_Guide.pdf
└── ADK_EMV_Contactless_Programmers_Guide.pdf
```

## HOMOLOGAÇÃO

Após implementação completa:

1. **Software Express:** enviar `libppcomp.so` + `VERSIONS.TXT` para inclusão
   no pacote CliSiTef Android. Contato: developer@softwareexpress.com.br

2. **Verifone Brasil:** assinar o `.so` no Developer Central e validar
   compatibilidade com VAOS do V660P-A.

3. **Adquirentes:** homologar com Cielo, Rede, Stone, Getnet, Fiserv Brasil
   para aprovação nos ambientes de produção.

## SEGURANÇA

Este projeto lida com dados de pagamento. Regras de segurança em
`.copilot/context/coding-standards.md` são **inegociáveis**.

PAN, PIN, trilhas e chaves criptográficas **nunca** aparecem em claro
fora do domínio seguro do SDI Server. O domínio P2PE é mantido pelo
hardware do V660P-A.
