#pragma once
// tests/mocks/sdiclient/sdi_nfc.h
// Stub das definições NFC do Verifone ADK para builds de teste.

#include <cstdint>
#include "sdi_emv.h"

// NFC Protocol Types
#define NFC_PT_ISO14443A  0x01
#define NFC_PT_ISO14443B  0x02
#define NFC_PT_ISO15693   0x04
#define NFC_PT_FELICA     0x08

// NFC Status
#define NFC_STATUS_OK          0x00
#define NFC_STATUS_NO_CARD     0x01
#define NFC_STATUS_COLLISION   0x02
#define NFC_STATUS_ERROR       0xFF

// NFC Card data
struct NFC_CARD_DATA_STRUCT {
    uint8_t  ProtocolType;
    uint8_t  UID[10];
    uint8_t  UIDLen;
    uint8_t  ATQ[2];
    uint8_t  SAK;
};

#ifdef __cplusplus
extern "C" {
#endif

EMV_ADK_INFO NFC_Detect(uint8_t protocolMask, NFC_CARD_DATA_STRUCT* cardData);
EMV_ADK_INFO NFC_Disconnect(void);

#ifdef __cplusplus
}
#endif
