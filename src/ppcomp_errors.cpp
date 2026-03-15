// src/ppcomp_errors.cpp
// Descrição: Descrições textuais dos códigos de erro ABECS PP_*.
//
// Sem dependências externas — pode ser compilado em qualquer plataforma.

#include "ppcomp_internal.h"

extern "C" {

const char* PP_GetErrorDescription(int ppCode) {
    switch (ppCode) {
        case PP_OK:                return "Sucesso";
        case PP_GO_ONLINE:         return "Ir ao autorizador (ARQC)";
        case PP_OFFLINE_APPROVE:   return "Aprovacao offline (TC)";
        case PP_OFFLINE_DECLINE:   return "Recusa offline (AAC)";
        case PP_FALLBACK_CT:       return "Fallback: tentar chip CT";
        case PP_MULTIAPP:          return "Multiplos AIDs disponíveis";
        case PP_CTLS_WAITING:      return "Aguardando toque NFC";
        case PP_ERR_INIT:          return "Erro: pinpad nao inicializado";
        case PP_ERR_PARAM:         return "Erro: parametro invalido";
        case PP_ERR_STATE:         return "Erro: estado interno incorreto";
        case PP_ERR_NOTCHIP:       return "Erro: tecnologia nao e chip";
        case PP_ERR_BUFFER:        return "Erro: buffer de saida insuficiente";
        case PP_ERR_NOEVENT:       return "Sem evento de cartao";
        case PP_ERR_PINPAD:        return "Erro critico de pinpad (erro 31)";
        case PP_ABORT:             return "Operacao cancelada pelo usuario";
        case PP_ERR_TIMEOUT:       return "Timeout";
        case PP_ERR_WRONG_PIN:     return "PIN incorreto";
        case PP_ERR_PIN_BLOCKED:   return "PIN bloqueado";
        case PP_ERR_CARD_BLOCKED:  return "Cartao bloqueado";
        case PP_ERR_PIN:           return "Erro na captura de PIN";
        case PP_PIN_BYPASS:        return "PIN bypass (sem PIN)";
        case PP_ERR_GENERIC:       return "Erro generico nao mapeado";
        default:                   return "Erro desconhecido";
    }
}

}  // extern "C"
