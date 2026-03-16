#pragma once
// tests/mocks/sdi_mock.h
// Google Mock para todas as funções e classes SDI usadas pela libppcomp.
// Permite testar a lógica ABECS sem hardware Verifone real.

#include <gmock/gmock.h>
#include <sdiclient/sdi_emv.h>
#include <sdiclient/sdi_if.h>
#include <vector>

// ─── Mock para funções C do SDI ──────────────────────────────────────────────

class SdiEmvMock {
public:
    SdiEmvMock();
    ~SdiEmvMock();

    static SdiEmvMock* instance;

    MOCK_METHOD(EMV_ADK_INFO, SDI_ProtocolInit, (const char* jsonConfig));
    MOCK_METHOD(EMV_ADK_INFO, SDI_ProtocolExit, ());
    MOCK_METHOD(EMV_ADK_INFO, SDI_CT_Init_Framework, (EMV_CT_CALLBACK_FN cb));
    MOCK_METHOD(EMV_ADK_INFO, SDI_CT_Exit_Framework, ());
    MOCK_METHOD(EMV_ADK_INFO, SDI_CTLS_Init_Framework, (EMV_CTLS_CALLBACK_FN cb));
    MOCK_METHOD(EMV_ADK_INFO, SDI_CTLS_Exit_Framework, ());
    MOCK_METHOD(EMV_ADK_INFO, SDI_SetSdiCallback, (void* cb, uint8_t sw1, uint8_t sw2));

    MOCK_METHOD(EMV_ADK_INFO, SDI_CT_SetTermData,
                (const EMV_CT_TERMDATA_STRUCT* td));
    MOCK_METHOD(EMV_ADK_INFO, SDI_CTLS_SetTermData,
                (const EMV_CTLS_TERMDATA_STRUCT* td));
    MOCK_METHOD(EMV_ADK_INFO, SDI_CT_SetAppliData,
                (const EMV_CT_APPLI_DATA_STRUCT* ad, uint32_t flags));
    MOCK_METHOD(EMV_ADK_INFO, SDI_CTLS_SetAppliDataSchemeSpecific,
                (const EMV_CTLS_APPLI_DATA_STRUCT* ad, uint8_t kernelID, uint32_t flags));
    MOCK_METHOD(EMV_ADK_INFO, SDI_CT_StoreCAPKey,
                (const EMV_CT_CAPKEY_STRUCT* key, uint32_t flags));
    MOCK_METHOD(EMV_ADK_INFO, SDI_CTLS_StoreCAPKey,
                (const EMV_CTLS_CAPKEY_STRUCT* key, uint32_t flags));
    MOCK_METHOD(EMV_ADK_INFO, SDI_CT_ApplyConfiguration, ());
    MOCK_METHOD(EMV_ADK_INFO, SDI_CTLS_ApplyConfiguration, ());

    MOCK_METHOD(EMV_ADK_INFO, SDI_CT_StartTransaction,
                (uint32_t amount, uint32_t cashback, uint16_t currency,
                 uint8_t txnType, EMV_CT_STARTRESULT_STRUCT* result));
    MOCK_METHOD(EMV_ADK_INFO, SDI_CT_SetSelectedApp,
                (const uint8_t* aid, uint8_t aidLen));
    MOCK_METHOD(EMV_ADK_INFO, SDI_CT_ContinueOffline,
                (uint32_t opts, EMV_CT_TRANSRES_STRUCT* tr, EMV_SDI_CT_TRANSRES_STRUCT* sdi));
    MOCK_METHOD(EMV_ADK_INFO, SDI_CT_ContinueOnline,
                (const uint8_t* arpc, uint32_t arpcLen, const uint8_t* scripts,
                 uint32_t scriptsLen, EMV_CT_TRANSRES_STRUCT* tr,
                 EMV_SDI_CT_TRANSRES_STRUCT* sdi));
    MOCK_METHOD(EMV_ADK_INFO, SDI_CT_EndTransaction, (uint32_t opts));
    MOCK_METHOD(EMV_ADK_INFO, SDI_CT_Break, ());

    MOCK_METHOD(EMV_ADK_INFO, SDI_CTLS_SetupTransaction,
                (uint32_t amount, uint32_t cashback, uint16_t currency,
                 uint8_t txnType, void* setup));
    MOCK_METHOD(EMV_ADK_INFO, SDI_CTLS_ContinueOffline,
                (uint32_t opts, EMV_CTLS_TRANSRES_STRUCT* tr,
                 EMV_SDI_CTLS_TRANSRES_STRUCT* sdi));
    MOCK_METHOD(EMV_ADK_INFO, SDI_CTLS_ContinueOnline,
                (const uint8_t* arpc, uint32_t arpcLen,
                 EMV_CTLS_TRANSRES_STRUCT* tr, EMV_SDI_CTLS_TRANSRES_STRUCT* sdi));
    MOCK_METHOD(EMV_ADK_INFO, SDI_CTLS_EndTransaction, (uint32_t opts));
    MOCK_METHOD(EMV_ADK_INFO, SDI_CTLS_Break, ());

    MOCK_METHOD(EMV_ADK_INFO, SDI_fetchTxnTags,
                (const uint32_t* tags, uint32_t count, uint8_t* out, uint32_t* outLen));
};

// ─── Mock para classes C++ do libsdi namespace ────────────────────────────────

class SdiClassMock {
public:
    SdiClassMock();
    ~SdiClassMock();

    static SdiClassMock* instance;

    // libsdi::SDI
    MOCK_METHOD(EMV_ADK_INFO, SDI_connect, (const std::string& host, uint16_t port));
    MOCK_METHOD(EMV_ADK_INFO, SDI_disconnect, ());
    MOCK_METHOD(EMV_ADK_INFO, SDI_waitCardRemoval, (uint32_t timeoutSec));

    // libsdi::CardDetection
    MOCK_METHOD(EMV_ADK_INFO, cardDetection_startSelection,
                (uint8_t tecMask, libsdi::DetectionMode mode,
                 uint32_t timeoutMs, uint32_t ctFlags));
    MOCK_METHOD(libsdi::Technology, cardDetection_pollTechnology, ());
    MOCK_METHOD(EMV_ADK_INFO, cardDetection_stopSelection, ());

    // libsdi::PED
    MOCK_METHOD(EMV_ADK_INFO, ped_setDefaultTimeout, (uint32_t timeoutMs));
    MOCK_METHOD(EMV_ADK_INFO, ped_sendPinInputParameters,
                (uint8_t minDig, uint8_t maxDig, uint8_t flags));
    MOCK_METHOD(int, ped_startPinInput, (uint8_t cancelMode));
    MOCK_METHOD(int, ped_startPinEntry, (uint8_t cancelMode));
    MOCK_METHOD(int, ped_pollPinEntry, ());
    MOCK_METHOD(EMV_ADK_INFO, ped_stopPinEntry, ());

    // libsdi::SdiCrypt
    MOCK_METHOD(EMV_ADK_INFO, crypt_getEncryptedPin, (std::vector<uint8_t>& encPin));
    MOCK_METHOD(EMV_ADK_INFO, crypt_getKeyInventory, (std::vector<uint8_t>& inv));

    // libsdi::Dialog
    MOCK_METHOD(EMV_ADK_INFO, dialog_requestCard,
                (const std::string& msg, uint32_t timeoutMs));
};

// ─── Helpers para configurar cenários comuns ─────────────────────────────────

namespace SdiTestHelper {

/// Configura mock para PP_Open bem-sucedido
void setupOpenSuccess(SdiEmvMock& emv, SdiClassMock& cls);

/// Configura mock para carga de tabelas bem-sucedida
void setupTablesSuccess(SdiEmvMock& emv, int aidCount = 1, int capkCount = 1);

/// Configura mock para aprovação offline CT
void setupOfflineApprovalCT(SdiEmvMock& emv);

/// Configura mock para transação online CT (ARQC → aprovado)
void setupOnlineApprovalCT(SdiEmvMock& emv);

/// Configura mock para declinação online CT
void setupOnlineDeclineCT(SdiEmvMock& emv);

/// Configura mock para aprovação CTLS offline
void setupCTLSApproval(SdiEmvMock& emv, SdiClassMock& cls);

/// Configura mock para fallback CTLS → CT
void setupCTLSFallback(SdiEmvMock& emv, SdiClassMock& cls);

/// Configura mock para PIN online bem-sucedido
void setupPinSuccess(SdiClassMock& cls);

/// Constrói buffer T1 (TermData) mínimo válido
std::vector<uint8_t> buildTermDataBuffer();

/// Constrói buffer T2 (AIDs) com N AIDs de teste
std::vector<uint8_t> buildAIDBuffer(int count = 1);

/// Constrói buffer T3 (CAPKs) com N CAPKs de teste
std::vector<uint8_t> buildCAPKBuffer(int count = 1);

}  // namespace SdiTestHelper
