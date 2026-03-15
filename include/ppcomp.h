#pragma once
// include/ppcomp.h
// API pública da libppcomp — interface ABECS 2.20 para Pinpad.
//
// NOTA: Em produção, este header é fornecido pela Software Express.
//       Esta versão stub foi criada para builds de desenvolvimento/teste.
//       Os protótipos de função são derivados da especificação ABECS 2.20.
//
// A libclisitef.so carrega estes símbolos via dlopen("libppcomp.so") + dlsym().
// Todos os símbolos exportados DEVEM ter linkagem C (extern "C").

#ifdef __cplusplus
extern "C" {
#endif

// ─── Ciclo de vida ────────────────────────────────────────────────────────────

/**
 * PP_Open — Inicializa o pinpad e conecta ao SDI Server.
 * @param portaFisica  Identificador de porta (ignorado; SDI usa socket)
 * @param parametros   String JSON ou parâmetros de configuração (pode ser NULL)
 * @return PP_OK ou PP_ERR_INIT
 */
int PP_Open(int portaFisica, char* parametros);

/**
 * PP_Close — Encerra conexão com SDI Server.
 * @param idPP  Handle retornado pelo PP_Open (reservado — usar 0)
 * @return PP_OK
 */
int PP_Close(int idPP);

/**
 * PP_Reset — Reinicia o pinpad para estado IDLE.
 * @return PP_OK ou PP_ERR_INIT
 */
int PP_Reset(int idPP);

/**
 * PP_GetInfo — Obtém informação sobre o pinpad.
 * @param tipo      Tipo de informação (0=versão, 1=serial, 2=modelo)
 * @param buffer    Buffer de saída
 * @param tamBuffer Tamanho do buffer
 * @return PP_OK ou PP_ERR_PARAM
 */
int PP_GetInfo(int idPP, int tipo, char* buffer, int tamBuffer);

/**
 * PP_Abort — Cancela operação em andamento em qualquer estado.
 * @return PP_OK
 */
int PP_Abort(int idPP);

// ─── Configuração de tabelas ──────────────────────────────────────────────────

/**
 * PP_SetTermData — Configura dados do terminal (T1 ABECS).
 * @param buffer  Buffer com estrutura T1 no formato ABECS
 * @param tamBuf  Tamanho do buffer
 * @return PP_OK ou PP_ERR_PARAM
 */
int PP_SetTermData(int idPP, char* buffer, int tamBuf);

/**
 * PP_LoadAIDTable — Carrega tabela de AIDs (T2 ABECS).
 * @param buffer  Buffer com AIDs no formato ABECS binário
 * @param tamBuf  Tamanho do buffer
 * @return PP_OK ou PP_ERR_PARAM
 */
int PP_LoadAIDTable(int idPP, char* buffer, int tamBuf);

/**
 * PP_LoadCAPKTable — Carrega tabela de chaves públicas (T3 ABECS).
 * @param buffer  Buffer com CAPKs no formato ABECS binário
 * @param tamBuf  Tamanho do buffer
 * @return PP_OK ou PP_ERR_PARAM
 */
int PP_LoadCAPKTable(int idPP, char* buffer, int tamBuf);

// ─── Detecção de tecnologia ───────────────────────────────────────────────────

/**
 * PP_StartCheckEvent — Inicia detecção de tecnologia de cartão.
 * @param modalidades   Bitmask: 0x01=CT, 0x02=CTLS, 0x04=MSR
 * @param timeout       Timeout em segundos
 * @param podeCancelar  1=permitir cancelamento pelo usuário
 * @return PP_OK ou erro
 */
int PP_StartCheckEvent(int idPP, int modalidades, int timeout, int podeCancelar);

/**
 * PP_CheckEvent — Verifica se cartão foi detectado (modelo poll).
 * @param tecnologia   [out] Tecnologia detectada (1=CT, 2=CTLS, 4=MSR)
 * @param dadosCartao  [out] Buffer com dados do cartão (preenchido para MSR)
 * @param tamDados     [in/out] Tamanho do buffer / bytes escritos
 * @return PP_OK (cartão detectado), PP_ERR_NOEVENT (sem evento), ou erro
 */
int PP_CheckEvent(int idPP, int* tecnologia, char* dadosCartao, int* tamDados);

/**
 * PP_AbortCheckEvent — Cancela detecção de cartão.
 * @return PP_OK
 */
int PP_AbortCheckEvent(int idPP);

// ─── EMV Contato (Chip CT) ────────────────────────────────────────────────────

/**
 * PP_GoOnChip — Inicia transação EMV Contact.
 * @param valor      Valor em centavos
 * @param cashback   Valor de cashback em centavos (0 se não aplicável)
 * @param tipoConta  Tipo de conta (0=crédito, 1=débito)
 * @param tipoTrans  Tipo de transação ABECS (1=compra, 2=saque, 9=cashback...)
 * @param outBuf     [out] Buffer com resultado (startResult serializado)
 * @param outLen     [in/out] Tamanho do buffer / bytes escritos
 * @return PP_OK, PP_MULTIAPP, ou erro
 */
int PP_GoOnChip(int idPP, long valor, int cashback, int tipoConta,
                int tipoTrans, char* outBuf, int* outLen);

/**
 * PP_SetAID — Seleciona AID após PP_MULTIAPP.
 * @param aid    AID selecionado pelo operador
 * @param aidLen Comprimento do AID
 * @return PP_OK ou PP_ERR_PARAM
 */
int PP_SetAID(int idPP, char* aid, int aidLen);

/**
 * PP_GoOnChipContinue — Continua transação EMV (offline decision).
 * @param outBuf  [out] Resultado serializado (cryptogram, TVR, etc.)
 * @param outLen  [in/out]
 * @return PP_GO_ONLINE, PP_OFFLINE_APPROVE, PP_OFFLINE_DECLINE, ou erro
 */
int PP_GoOnChipContinue(int idPP, char* outBuf, int* outLen);

/**
 * PP_FinishChip — Finaliza transação EMV com resposta online (ARPC + scripts).
 * @param arpc       ARPC do autorizador
 * @param arpcLen    Comprimento do ARPC
 * @param scripts    Scripts issuer (pode ser NULL)
 * @param scriptsLen Comprimento dos scripts (0 se NULL)
 * @param outBuf     [out] Resultado final para comprovante
 * @param outLen     [in/out]
 * @return PP_OFFLINE_APPROVE (TC), PP_OFFLINE_DECLINE (AAC), ou erro
 */
int PP_FinishChip(int idPP, char* arpc, int arpcLen,
                  char* scripts, int scriptsLen,
                  char* outBuf, int* outLen);

/**
 * PP_UpdateTags — Atualiza tags EMV antes do ContinueOffline.
 */
int PP_UpdateTags(int idPP, char* tlvData, int tlvLen);

/**
 * PP_EndChip — Encerra transação EMV Contact.
 * @param resultado  0=normal, 1=cancelado
 * @return PP_OK
 */
int PP_EndChip(int idPP, int resultado);

// ─── EMV Contactless (CTLS/NFC) ───────────────────────────────────────────────

/**
 * PP_GoOnChipCTLS — Inicia transação EMV Contactless.
 * @return PP_CTLS_WAITING (aguardando toque), ou erro
 */
int PP_GoOnChipCTLS(int idPP, long valor, int tipoConta, int tipoTrans,
                    char* outBuf, int* outLen);

/**
 * PP_PollCTLS — Verifica status do polling CTLS (chamado em loop).
 * @return PP_ERR_NOEVENT (ainda aguardando), PP_GO_ONLINE, PP_OFFLINE_APPROVE,
 *         PP_OFFLINE_DECLINE, PP_FALLBACK_CT, ou erro
 */
int PP_PollCTLS(int idPP, char* outBuf, int* outLen);

/**
 * PP_FinishChipCTLS — Finaliza transação CTLS online.
 */
int PP_FinishChipCTLS(int idPP, char* arpc, int arpcLen,
                      char* outBuf, int* outLen);

/**
 * PP_AbortCTLS — Cancela transação CTLS em andamento.
 */
int PP_AbortCTLS(int idPP);

// ─── PIN Entry ────────────────────────────────────────────────────────────────

/**
 * PP_GetPIN — Captura PIN do portador (bloqueante).
 * @param minDig       Mínimo de dígitos
 * @param maxDig       Máximo de dígitos
 * @param timeout      Timeout em segundos
 * @param podeCancelar 1=permite cancelamento
 * @param tipoVerif    0=offline (kernel EMV), 1=online (PED envia PIN block)
 * @return PP_OK, PP_PIN_BYPASS, PP_ABORT, PP_ERR_WRONG_PIN, PP_ERR_TIMEOUT
 */
int PP_GetPIN(int idPP, int minDig, int maxDig, int timeout,
              int podeCancelar, int tipoVerif);

/**
 * PP_StartGetPIN — Inicia captura de PIN (assíncrono).
 */
int PP_StartGetPIN(int idPP, int minDig, int maxDig, int timeout,
                   int podeCancelar, int tipoVerif);

/**
 * PP_PollGetPIN — Verifica status da captura de PIN.
 * @return PP_ERR_NOEVENT (ainda digitando), PP_OK (concluído), ou erro
 */
int PP_PollGetPIN(int idPP);

/**
 * PP_AbortGetPIN — Cancela captura de PIN.
 */
int PP_AbortGetPIN(int idPP);

/**
 * PP_GetPINBlock — Obtém PIN block criptografado após PP_GetPIN.
 * @param pinBlock    [out] PIN block (8 bytes ISO format 0, DUKPT)
 * @param pinBlockLen [in/out]
 * @param ksn         [out] Key Serial Number (10 bytes DUKPT)
 * @param ksnLen      [in/out]
 * @return PP_OK ou PP_ERR_INIT (se PIN não foi capturado)
 */
int PP_GetPINBlock(int idPP, unsigned char* pinBlock, int* pinBlockLen,
                   unsigned char* ksn, int* ksnLen);

// ─── Cartão magnético (MSR) ───────────────────────────────────────────────────

/**
 * PP_GetCard — Lê dados do cartão MSR (após PP_CheckEvent retornar MSR).
 * @param dadosCartao  [out] Dados serializado (trilhas cifradas + KSN)
 * @param tamDados     [in/out]
 * @return PP_OK ou erro
 */
int PP_GetCard(int idPP, char* dadosCartao, int* tamDados);

#ifdef __cplusplus
}
#endif
