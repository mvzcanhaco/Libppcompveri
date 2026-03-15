#pragma once
// tests/mocks/sdiclient/sdi_if.h
// Stub das classes C++ do Verifone ADK (libsdi namespace) para builds de teste.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "sdi_emv.h"

namespace libsdi {

// ─── Tecnologias de cartão ────────────────────────────────────────────────────

enum class Technology : uint8_t {
    NONE    = 0x00,
    CHIP_CT = 0x01,
    CTLS    = 0x02,
    MSR     = 0x04,
};

// ─── Modo de detecção ─────────────────────────────────────────────────────────

enum class DetectionMode : uint8_t {
    BLOCKING    = 0x01,
    NON_BLOCKING = 0x02,
};

// ─── Resultados PED ───────────────────────────────────────────────────────────

#define EMV_PED_OK      0
#define EMV_PED_BYPASS  1
#define EMV_PED_CANCEL  2
#define EMV_PED_TIMEOUT 3
#define EMV_PED_ERROR   4

// ─── Classe SDI (conexão com SDI Server) ─────────────────────────────────────

class SDI {
public:
    static EMV_ADK_INFO connect(const std::string& host, uint16_t port);
    static EMV_ADK_INFO disconnect(void);
    static EMV_ADK_INFO waitCardRemoval(uint32_t timeoutSeconds);
    static bool isConnected(void);
};

// ─── Classe CardDetection ─────────────────────────────────────────────────────

class CardDetection {
public:
    CardDetection() = default;
    ~CardDetection() = default;

    EMV_ADK_INFO startSelection(uint8_t techMask, DetectionMode mode,
                                 uint32_t timeoutMs, uint32_t ctFlags);
    Technology   pollTechnology(void);
    EMV_ADK_INFO stopSelection(void);
    EMV_ADK_INFO startMsrRead(void);
};

// ─── Classe PED (PIN Entry Device) ───────────────────────────────────────────

class PED {
public:
    PED() = default;
    ~PED() = default;

    EMV_ADK_INFO setDefaultTimeout(uint32_t timeoutMs);
    EMV_ADK_INFO sendPinInputParameters(uint8_t minDigits, uint8_t maxDigits,
                                         uint8_t flags);
    int startPinInput(uint8_t cancelMode);       // returns EMV_PED_*
    int startPinEntry(uint8_t cancelMode);        // async version
    int pollPinEntry(void);                       // returns EMV_PED_* or EMV_PED_OK
    EMV_ADK_INFO stopPinEntry(void);
};

// ─── Classe SdiCrypt (criptografia P2PE) ─────────────────────────────────────

class SdiCrypt {
public:
    SdiCrypt() = default;
    ~SdiCrypt() = default;

    // Retorna PIN block criptografado DUKPT (ISO format 0, 8 bytes)
    // NUNCA logar o resultado
    EMV_ADK_INFO getEncryptedPin(std::vector<uint8_t>& encPin);

    // Retorna inventário de chaves para extração do KSN
    EMV_ADK_INFO getKeyInventory(std::vector<uint8_t>& inventory);
};

// ─── Classe Dialog (display do terminal) ─────────────────────────────────────

class Dialog {
public:
    static EMV_ADK_INFO showMessage(const std::string& message);
    static EMV_ADK_INFO requestCard(const std::string& message, uint32_t timeoutMs);
    static EMV_ADK_INFO clearScreen(void);
};

// ─── Classe ManualEntry ───────────────────────────────────────────────────────

class ManualEntry {
public:
    ManualEntry() = default;
    ~ManualEntry() = default;

    EMV_ADK_INFO startEntry(uint8_t type, uint32_t timeoutMs);
    EMV_ADK_INFO pollEntry(void);
    EMV_ADK_INFO stopEntry(void);
    EMV_ADK_INFO getResult(uint8_t* outBuf, uint32_t* outLen);
};

}  // namespace libsdi

// Variáveis globais de instâncias SDI — definidas em ppcomp_globals.cpp
// (extensão: as instâncias são globais pois o SDI Server aceita 1 conexão)
extern libsdi::CardDetection g_cardDetection;
extern libsdi::PED           g_ped;
