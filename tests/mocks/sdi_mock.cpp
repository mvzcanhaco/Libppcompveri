// tests/mocks/sdi_mock.cpp
// Implementação das funções C/C++ stub que delegam ao mock.
// Os mocks são singleton por teste — criados no SetUp(), destruídos no TearDown().

#include "sdi_mock.h"
#include <cstring>
#include <stdexcept>

// ─── Singletons de mock ───────────────────────────────────────────────────────

SdiEmvMock*   SdiEmvMock::instance   = nullptr;
SdiClassMock* SdiClassMock::instance = nullptr;

SdiEmvMock::SdiEmvMock() {
    if (instance != nullptr) {
        throw std::runtime_error("SdiEmvMock: instância já existe");
    }
    instance = this;
}

SdiEmvMock::~SdiEmvMock() {
    instance = nullptr;
}

SdiClassMock::SdiClassMock() {
    if (instance != nullptr) {
        throw std::runtime_error("SdiClassMock: instância já existe");
    }
    instance = this;
}

SdiClassMock::~SdiClassMock() {
    instance = nullptr;
}

// ─── Stubs das funções C do SDI ───────────────────────────────────────────────

extern "C" {

EMV_ADK_INFO SDI_ProtocolInit(const char* j) {
    return SdiEmvMock::instance->SDI_ProtocolInit(j);
}
EMV_ADK_INFO SDI_ProtocolExit() {
    return SdiEmvMock::instance->SDI_ProtocolExit();
}
EMV_ADK_INFO SDI_CT_Init_Framework(EMV_CT_CALLBACK_FN cb) {
    return SdiEmvMock::instance->SDI_CT_Init_Framework(cb);
}
EMV_ADK_INFO SDI_CT_Exit_Framework() {
    return SdiEmvMock::instance->SDI_CT_Exit_Framework();
}
EMV_ADK_INFO SDI_CTLS_Init_Framework(EMV_CTLS_CALLBACK_FN cb) {
    return SdiEmvMock::instance->SDI_CTLS_Init_Framework(cb);
}
EMV_ADK_INFO SDI_CTLS_Exit_Framework() {
    return SdiEmvMock::instance->SDI_CTLS_Exit_Framework();
}
EMV_ADK_INFO SDI_SetSdiCallback(void* cb, uint8_t sw1, uint8_t sw2) {
    return SdiEmvMock::instance->SDI_SetSdiCallback(cb, sw1, sw2);
}
EMV_ADK_INFO SDI_CT_SetTermData(const EMV_CT_TERMDATA_STRUCT* td) {
    return SdiEmvMock::instance->SDI_CT_SetTermData(td);
}
EMV_ADK_INFO SDI_CTLS_SetTermData(const EMV_CTLS_TERMDATA_STRUCT* td) {
    return SdiEmvMock::instance->SDI_CTLS_SetTermData(td);
}
EMV_ADK_INFO SDI_CT_SetAppliData(const EMV_CT_APPLI_DATA_STRUCT* ad, uint32_t flags) {
    return SdiEmvMock::instance->SDI_CT_SetAppliData(ad, flags);
}
EMV_ADK_INFO SDI_CTLS_SetAppliDataSchemeSpecific(const EMV_CTLS_APPLI_DATA_STRUCT* ad,
                                                   uint8_t k, uint32_t flags) {
    return SdiEmvMock::instance->SDI_CTLS_SetAppliDataSchemeSpecific(ad, k, flags);
}
EMV_ADK_INFO SDI_CT_StoreCAPKey(const EMV_CT_CAPKEY_STRUCT* key, uint32_t flags) {
    return SdiEmvMock::instance->SDI_CT_StoreCAPKey(key, flags);
}
EMV_ADK_INFO SDI_CTLS_StoreCAPKey(const EMV_CTLS_CAPKEY_STRUCT* key, uint32_t flags) {
    return SdiEmvMock::instance->SDI_CTLS_StoreCAPKey(key, flags);
}
EMV_ADK_INFO SDI_CT_ApplyConfiguration() {
    return SdiEmvMock::instance->SDI_CT_ApplyConfiguration();
}
EMV_ADK_INFO SDI_CTLS_ApplyConfiguration() {
    return SdiEmvMock::instance->SDI_CTLS_ApplyConfiguration();
}
EMV_ADK_INFO SDI_CT_StartTransaction(uint32_t a, uint32_t cb, uint16_t cur,
                                       uint8_t t, EMV_CT_STARTRESULT_STRUCT* r) {
    return SdiEmvMock::instance->SDI_CT_StartTransaction(a, cb, cur, t, r);
}
EMV_ADK_INFO SDI_CT_SetSelectedApp(const uint8_t* aid, uint8_t len) {
    return SdiEmvMock::instance->SDI_CT_SetSelectedApp(aid, len);
}
EMV_ADK_INFO SDI_CT_ContinueOffline(uint32_t o, EMV_CT_TRANSRES_STRUCT* tr,
                                     EMV_SDI_CT_TRANSRES_STRUCT* s) {
    return SdiEmvMock::instance->SDI_CT_ContinueOffline(o, tr, s);
}
EMV_ADK_INFO SDI_CT_ContinueOnline(const uint8_t* arpc, uint32_t al, const uint8_t* sc,
                                    uint32_t sl, EMV_CT_TRANSRES_STRUCT* tr,
                                    EMV_SDI_CT_TRANSRES_STRUCT* s) {
    return SdiEmvMock::instance->SDI_CT_ContinueOnline(arpc, al, sc, sl, tr, s);
}
EMV_ADK_INFO SDI_CT_EndTransaction(uint32_t o) {
    return SdiEmvMock::instance->SDI_CT_EndTransaction(o);
}
EMV_ADK_INFO SDI_CT_Break() {
    return SdiEmvMock::instance->SDI_CT_Break();
}
EMV_ADK_INFO SDI_CTLS_SetupTransaction(uint32_t a, uint32_t cb, uint16_t cur,
                                         uint8_t t, void* s) {
    return SdiEmvMock::instance->SDI_CTLS_SetupTransaction(a, cb, cur, t, s);
}
EMV_ADK_INFO SDI_CTLS_ContinueOffline(uint32_t o, EMV_CTLS_TRANSRES_STRUCT* tr,
                                        EMV_SDI_CTLS_TRANSRES_STRUCT* s) {
    return SdiEmvMock::instance->SDI_CTLS_ContinueOffline(o, tr, s);
}
EMV_ADK_INFO SDI_CTLS_ContinueOnline(const uint8_t* arpc, uint32_t al,
                                       EMV_CTLS_TRANSRES_STRUCT* tr,
                                       EMV_SDI_CTLS_TRANSRES_STRUCT* s) {
    return SdiEmvMock::instance->SDI_CTLS_ContinueOnline(arpc, al, tr, s);
}
EMV_ADK_INFO SDI_CTLS_EndTransaction(uint32_t o) {
    return SdiEmvMock::instance->SDI_CTLS_EndTransaction(o);
}
EMV_ADK_INFO SDI_CTLS_Break() {
    return SdiEmvMock::instance->SDI_CTLS_Break();
}
EMV_ADK_INFO SDI_fetchTxnTags(const uint32_t* tags, uint32_t count,
                                uint8_t* out, uint32_t* outLen) {
    return SdiEmvMock::instance->SDI_fetchTxnTags(tags, count, out, outLen);
}

}  // extern "C"

// ─── Stubs das classes C++ libsdi ─────────────────────────────────────────────

namespace libsdi {

EMV_ADK_INFO SDI::connect(const std::string& host, uint16_t port) {
    return SdiClassMock::instance->SDI_connect(host, port);
}
EMV_ADK_INFO SDI::disconnect() {
    return SdiClassMock::instance->SDI_disconnect();
}
EMV_ADK_INFO SDI::waitCardRemoval(uint32_t t) {
    return SdiClassMock::instance->SDI_waitCardRemoval(t);
}
bool SDI::isConnected() { return true; }

EMV_ADK_INFO CardDetection::startSelection(uint8_t m, DetectionMode dm,
                                             uint32_t t, uint32_t f) {
    return SdiClassMock::instance->cardDetection_startSelection(m, dm, t, f);
}
Technology CardDetection::pollTechnology() {
    return SdiClassMock::instance->cardDetection_pollTechnology();
}
EMV_ADK_INFO CardDetection::stopSelection() {
    return SdiClassMock::instance->cardDetection_stopSelection();
}
EMV_ADK_INFO CardDetection::startMsrRead() { return EMV_ADK_OK; }

EMV_ADK_INFO PED::setDefaultTimeout(uint32_t t) {
    return SdiClassMock::instance->ped_setDefaultTimeout(t);
}
EMV_ADK_INFO PED::sendPinInputParameters(uint8_t mn, uint8_t mx, uint8_t f) {
    return SdiClassMock::instance->ped_sendPinInputParameters(mn, mx, f);
}
int PED::startPinInput(uint8_t c) {
    return SdiClassMock::instance->ped_startPinInput(c);
}
int PED::startPinEntry(uint8_t c) {
    return SdiClassMock::instance->ped_startPinEntry(c);
}
int PED::pollPinEntry() {
    return SdiClassMock::instance->ped_pollPinEntry();
}
EMV_ADK_INFO PED::stopPinEntry() {
    return SdiClassMock::instance->ped_stopPinEntry();
}

EMV_ADK_INFO SdiCrypt::getEncryptedPin(std::vector<uint8_t>& v) {
    return SdiClassMock::instance->crypt_getEncryptedPin(v);
}
EMV_ADK_INFO SdiCrypt::getKeyInventory(std::vector<uint8_t>& v) {
    return SdiClassMock::instance->crypt_getKeyInventory(v);
}

EMV_ADK_INFO Dialog::showMessage(const std::string&) { return EMV_ADK_OK; }
EMV_ADK_INFO Dialog::requestCard(const std::string& m, uint32_t t) {
    return SdiClassMock::instance->dialog_requestCard(m, t);
}
EMV_ADK_INFO Dialog::clearScreen() { return EMV_ADK_OK; }

}  // namespace libsdi

// ─── Helpers ─────────────────────────────────────────────────────────────────

namespace SdiTestHelper {

using ::testing::Return;
using ::testing::_;
using ::testing::DoAll;
using ::testing::SetArgPointee;

void setupOpenSuccess(SdiEmvMock& emv, SdiClassMock& cls) {
    (void)cls;
    EXPECT_CALL(emv, SDI_ProtocolInit(_)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CT_Init_Framework(_)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CTLS_Init_Framework(_)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_SetSdiCallback(_, _, _)).Times(3).WillRepeatedly(Return(EMV_ADK_OK));
}

void setupTablesSuccess(SdiEmvMock& emv, int aidCount, int capkCount) {
    EXPECT_CALL(emv, SDI_CT_SetTermData(_)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CTLS_SetTermData(_)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CT_SetAppliData(_, _))
        .Times(aidCount).WillRepeatedly(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CTLS_SetAppliDataSchemeSpecific(_, _, _))
        .Times(::testing::AtLeast(0)).WillRepeatedly(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CT_StoreCAPKey(_, _))
        .Times(capkCount).WillRepeatedly(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CTLS_StoreCAPKey(_, _))
        .Times(::testing::AtLeast(0)).WillRepeatedly(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CT_ApplyConfiguration()).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CTLS_ApplyConfiguration()).WillOnce(Return(EMV_ADK_OK));
}

void setupOfflineApprovalCT(SdiEmvMock& emv) {
    EMV_CT_STARTRESULT_STRUCT startResult = {};
    startResult.NumCandidates = 1;
    const uint8_t testAID[] = {0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10};
    memcpy(startResult.SelectedAID, testAID, sizeof(testAID));
    startResult.SelectedAIDLen = sizeof(testAID);

    EXPECT_CALL(emv, SDI_CT_StartTransaction(_, _, _, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(startResult), Return(EMV_ADK_OK)));

    EMV_CT_TRANSRES_STRUCT transResult = {};
    transResult.CryptogramType = EMV_CRYPTOGRAM_TC;
    memset(transResult.Cryptogram, 0xAB, 8);  // Fake TC

    EXPECT_CALL(emv, SDI_CT_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(transResult), Return(EMV_ADK_OFFLINE_APPROVE)));
    EXPECT_CALL(emv, SDI_fetchTxnTags(_, _, _, _)).WillRepeatedly(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CT_EndTransaction(_)).WillOnce(Return(EMV_ADK_OK));
}

void setupOnlineApprovalCT(SdiEmvMock& emv) {
    EMV_CT_STARTRESULT_STRUCT startResult = {};
    startResult.NumCandidates = 1;
    startResult.SelectedAIDLen = 7;

    EXPECT_CALL(emv, SDI_CT_StartTransaction(_, _, _, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(startResult), Return(EMV_ADK_OK)));

    EMV_CT_TRANSRES_STRUCT arqcResult = {};
    arqcResult.CryptogramType = EMV_CRYPTOGRAM_ARQC;

    EXPECT_CALL(emv, SDI_CT_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(arqcResult), Return(EMV_ADK_GO_ONLINE)));

    EMV_CT_TRANSRES_STRUCT tcResult = {};
    tcResult.CryptogramType = EMV_CRYPTOGRAM_TC;

    EXPECT_CALL(emv, SDI_CT_ContinueOnline(_, _, _, _, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(tcResult), Return(EMV_ADK_OFFLINE_APPROVE)));
    EXPECT_CALL(emv, SDI_fetchTxnTags(_, _, _, _)).WillRepeatedly(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CT_EndTransaction(_)).WillOnce(Return(EMV_ADK_OK));
}

void setupOnlineDeclineCT(SdiEmvMock& emv) {
    EMV_CT_STARTRESULT_STRUCT startResult = {};
    startResult.NumCandidates = 1;

    EXPECT_CALL(emv, SDI_CT_StartTransaction(_, _, _, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(startResult), Return(EMV_ADK_OK)));

    EMV_CT_TRANSRES_STRUCT arqcResult = {};
    arqcResult.CryptogramType = EMV_CRYPTOGRAM_ARQC;

    EXPECT_CALL(emv, SDI_CT_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(arqcResult), Return(EMV_ADK_GO_ONLINE)));

    EMV_CT_TRANSRES_STRUCT aacResult = {};
    aacResult.CryptogramType = EMV_CRYPTOGRAM_AAC;

    EXPECT_CALL(emv, SDI_CT_ContinueOnline(_, _, _, _, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(aacResult), Return(EMV_ADK_DECLINE)));
    EXPECT_CALL(emv, SDI_fetchTxnTags(_, _, _, _)).WillRepeatedly(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CT_EndTransaction(_)).WillOnce(Return(EMV_ADK_OK));
}

void setupCTLSApproval(SdiEmvMock& emv, SdiClassMock& cls) {
    EXPECT_CALL(emv, SDI_CTLS_SetupTransaction(_, _, _, _, _)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, dialog_requestCard(_, _)).WillRepeatedly(Return(EMV_ADK_OK));

    EMV_CTLS_TRANSRES_STRUCT ctlsResult = {};
    ctlsResult.CryptogramType = EMV_CRYPTOGRAM_TC;

    EXPECT_CALL(emv, SDI_CTLS_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(ctlsResult), Return(EMV_ADK_OFFLINE_APPROVE)));
}

void setupCTLSFallback(SdiEmvMock& emv, SdiClassMock& cls) {
    EXPECT_CALL(emv, SDI_CTLS_SetupTransaction(_, _, _, _, _)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, dialog_requestCard(_, _)).WillRepeatedly(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CTLS_ContinueOffline(_, _, _)).WillOnce(Return(EMV_ADK_FALLBACK));
    EXPECT_CALL(emv, SDI_CTLS_Break()).WillOnce(Return(EMV_ADK_OK));
}

void setupPinSuccess(SdiClassMock& cls) {
    EXPECT_CALL(cls, ped_setDefaultTimeout(_)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_sendPinInputParameters(_, _, _)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_startPinInput(_)).WillOnce(Return(EMV_PED_OK));

    // getEncryptedPin retorna 8 bytes fake (não é PIN real)
    EXPECT_CALL(cls, crypt_getEncryptedPin(_))
        .WillOnce([](std::vector<uint8_t>& v) {
            v.assign(8, 0x00);  // zeros — jamais são dados reais
            return EMV_ADK_OK;
        });

    // getKeyInventory retorna inventário com KSN no final
    EXPECT_CALL(cls, crypt_getKeyInventory(_))
        .WillOnce([](std::vector<uint8_t>& v) {
            v.assign(20, 0x00);  // 20 bytes, KSN nos últimos 10
            return EMV_ADK_OK;
        });
}

std::vector<uint8_t> buildTermDataBuffer() {
    // T1 mínimo: terminal_type(1) + caps(3) + add_caps(5) + limits(12)
    std::vector<uint8_t> buf(21, 0x00);
    buf[0] = 0x22;  // terminal type: attended attended
    buf[1] = 0xE0; buf[2] = 0xF0; buf[3] = 0xC8;  // caps
    // add_caps: buf[4..8] = 0
    // ctls_floor_limit(4): 0
    // ctls_txn_limit(4): 5000 centavos = R$50,00
    buf[13] = 0x00; buf[14] = 0x00; buf[15] = 0x13; buf[16] = 0x88;
    // ctls_nopin_limit(4): 5000
    buf[17] = 0x00; buf[18] = 0x00; buf[19] = 0x13; buf[20] = 0x88;
    return buf;
}

std::vector<uint8_t> buildAIDBuffer(int count) {
    std::vector<uint8_t> buf;
    // AID Visa: A0000000031010
    const uint8_t visaAID[] = {0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10};
    for (int i = 0; i < count; i++) {
        buf.push_back(7);  // AID len
        buf.insert(buf.end(), visaAID, visaAID + 7);
        buf.push_back(0x01);  // ctls_supported = true
        buf.push_back(0x03);  // kernel_id = VK (Visa)
        // TAC Denial (5 bytes)
        buf.insert(buf.end(), {0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
        // TAC Online (5 bytes)
        buf.insert(buf.end(), {0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
        // TAC Default (5 bytes)
        buf.insert(buf.end(), {0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
        // Floor limit (4 bytes) = 0
        buf.insert(buf.end(), {0x00, 0x00, 0x00, 0x00});
        // DDOL len = 0
        buf.push_back(0x00);
        // TDOL len = 0
        buf.push_back(0x00);
    }
    return buf;
}

std::vector<uint8_t> buildCAPKBuffer(int count) {
    std::vector<uint8_t> buf;
    for (int i = 0; i < count; i++) {
        // RID Visa
        buf.insert(buf.end(), {0xA0, 0x00, 0x00, 0x00, 0x03});
        buf.push_back(0x07);  // index
        buf.push_back(0x01);  // algorithm RSA
        // Modulus: 128 bytes (fake RSA-1024)
        buf.push_back(128);
        buf.insert(buf.end(), 128, 0xAB);
        // Exponent: 3 bytes
        buf.push_back(3);
        buf.insert(buf.end(), {0x01, 0x00, 0x01});
        // Checksum SHA-1: 20 bytes
        buf.insert(buf.end(), 20, 0xCD);
        // ctls_supported
        buf.push_back(0x01);
    }
    return buf;
}

}  // namespace SdiTestHelper
