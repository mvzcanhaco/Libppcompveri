# Contexto de Arquitetura — libppcomp

## POR QUE ESTE PROJETO EXISTE

O terminal Verifone V660P-A roda VAOS (Verifone Android OS, baseado em VOS3).
Seu SDK nativo de pagamentos é o ADK (Application Development Kit), que expõe
o hardware via SDI Server (processo local em 127.0.0.1:12000).

O ecossistema de TEF brasileiro usa CliSiTef (Software Express) como middleware
universal. A CliSiTef espera uma "Biblioteca Compartilhada" no padrão ABECS 2.20 —
uma `.so` com símbolos C como `PP_Open`, `PP_GetCard`, `PP_GoOnChip`, etc.

**O problema:** A Verifone não fornece essa `.so` no padrão ABECS para o V660P-A.
Existe apenas para o Carbon 10 (terminal mais antigo, VxEOS). Portanto, é
necessário criá-la mapeando ABECS 2.20 → ADK SDI.

## DECISÕES DE DESIGN

### Por que C++ e não C puro?
- `libsdiclient.a` é C++ (usa `libsdi::CardDetection`, `libsdi::PED`, etc.)
- std::vector, std::mutex necessários para callbacks thread-safe
- A ABI externa (símbolos exportados para `libclisitef.so`) permanece C puro
  com `extern "C"` — exigência da ABECS

### Por que não JNI/Java?
- A `libclisitef.so` é uma biblioteca nativa que faz `dlopen` da `libppcomp.so`
- Não há JVM no contexto de execução da CliSiTef
- JNI introduziria overhead e complexidade desnecessários
- O SDI Server aceita conexões de qualquer processo nativo — sem necessidade
  de Android runtime

### Por que socket local e não Binder/AIDL?
- O SDI Server do Verifone ADK usa socket TCP local (127.0.0.1:12000) por design
- Isso permite que aplicações nativas C++ se conectem sem Android SDK
- É o mesmo mecanismo que o Carbon 10 usa

### Por que DETECTION_MODE_BLOCKING?
- A ABECS 2.20 usa modelo de poll: PP_StartCheckEvent() + PP_CheckEvent() em loop
- DETECTION_MODE_BLOCKING permite que o SDI gerencie internamente o RF polling
  enquanto a libppcomp expõe uma interface de poll para a libclisitef
- Alternativa CALLBACK seria mais complexa e menos compatível com o modelo ABECS

### Por que ApplyConfiguration() é obrigatório?
- O SDI Server mantém configuração EMV em XML no sistema de arquivos do VAOS
  (`/mnt/appdata/versioned/globalshare/sdi/emv/` no VOS3)
- `SetAppliData` e `StoreCAPKey` escrevem na RAM do processo SDI
- `ApplyConfiguration` persiste para o sistema de arquivos E sincroniza com
  os kernels EMV (que rodam em processo separado no VAOS)
- Sem ApplyConfiguration: configuração perdida no próximo restart do SDI

## FLUXO DE DADOS SENSÍVEIS — DIAGRAMA P2PE

```
Cartão físico
  │ chip / NFC / tarja
SDI Server (VAOS — enclave seguro)
  │ dados cifrados DUKPT ou tokenizados
  │ PAN: ofuscado (6 primeiros + 4 últimos visíveis)
  │ PIN block: ISO 0 cifrado com chave de trabalho DUKPT
  │ Trilhas: cifradas com DUKPT
  ▼
libppcomp.so  ← NUNCA VÊ DADOS SENSÍVEIS EM CLARO
  │ tags EMV (AC, ATC, TVR, IAD — não sensíveis)
  │ PIN block cifrado (blob opaco)
  │ PAN ofuscado (para display/log apenas)
  ▼
libclisitef.so → SiTef Server (Fiserv) → Autorizador
                 ↑ descriptografa no host
```

## ESTRUTURA DE CONFIGURAÇÃO EMV NO VAOS

O SDI Server armazena configuração em:
```
/mnt/appdata/versioned/globalshare/sdi/emv/
├── EMV_Terminal.xml          ← TermData CT e CTLS
├── EMV_Application.xml       ← AppliData (AIDs) CT
├── EMV_CTLS_Application.xml  ← AppliData CTLS por kernel
├── EMV_Keys.xml              ← CAPKs CT
├── EMV_CTLS_Keys.xml         ← CAPKs CTLS
└── EMV_Config.xml            ← opções do framework
```

Esses arquivos são **gerenciados pelo ADK** — não modificar diretamente.
A libppcomp os atualiza via `SDI_CT_SetAppliData` + `ApplyConfiguration`.

## KERNELS EMV CTLS SUPORTADOS NO BRASIL

| Kernel ID | Nome        | Bandeiras      | AID exemplo          |
|-----------|-------------|----------------|----------------------|
| 2 (MK)    | Mastercard  | Master, Maestro| A0000000041010       |
| 3 (VK)    | Visa        | Visa, Electron | A0000000031010       |
| 4 (AK)    | Amex        | Amex           | A000000025010801     |
| 6 (DK)    | Discover    | Discover, Diners| A0000001523010      |
| 7 (EK)    | Elo         | Elo            | A000000651010110     |

Hipercard usa kernel Mastercard (MK) por acordo de processamento.
VR/Ticket/Sodexo: kernels proprietários — verificar com cada bandeira.

## DEPENDÊNCIAS DO PROJETO

### Fornecidas pela Verifone (via Developer Central — acesso restrito)
```
libs/arm64-v8a/
├── libsdiclient.a          # interface C++ para SDI Server
├── libEMV_CT_Client.a      # serialização EMV Contact
├── libEMV_CTLS_Client.a    # serialização EMV Contactless
├── libTLV_Util.a           # parser/builder TLV
├── libsdiprotocol.so       # protocolo IPC com SDI Server
├── libipc.so               # IPC Verifone
└── libvfilog.so            # logging Verifone

libs/include/sdiclient/
├── sdi_emv.h               # SDI_CT_*, SDI_CTLS_*
├── sdi_if.h                # libsdi::SDI, CardDetection, PED, Dialog, SdiCrypt
└── sdi_nfc.h               # NFC APIs
```

### Fornecidas pela Software Express (via parceria)
```
# Headers da interface ABECS (não redistribuíveis)
include/ppcomp.h            # assinaturas das funções PP_*
```

### Open source (build-time apenas)
```
# Google Test — apenas para tests/
# Baixado via CMake FetchContent no ambiente de CI
```

## PROCESSO DE ASSINATURA

Todo `.so` que roda no V660P-A precisa ser assinado pela Verifone.
O processo usa o portal Verifone Developer Central:

1. Build da `libppcomp.so` não assinada localmente
2. Upload para https://developer.verifone.com (seção "Sign Application")
3. Download da versão assinada
4. Inclusão no pacote final com `PaymentService.apk`

**Consequência:** A CI/CD não produz o artefato final — apenas a versão de
desenvolvimento para testes. O binário de produção sempre passa pelo portal.

## RELAÇÃO COM O CARBON 10 (MODELO DE REFERÊNCIA)

O Carbon 10 é o único terminal Verifone com suporte documentado na CliSiTef.
Seus artefatos no pacote CliSiTef são:
- `DeveloperSDK-<versão>.aar`
- `Carbon_Connection-<versão>.aar`
- `PaymentService-<versão>.apk`

Para o V660P-A, os equivalentes serão:
- `libppcomp-<versão>.so` (esta biblioteca)
- `VerifonePPService-<versão>.apk` (PaymentService para VAOS — escopo futuro)

A `libppcomp.so` cobre o caso onde o SDI Server já está rodando no VAOS —
que é o caso normal do V660P-A em produção.
