// tests/test_emv_ctls.cpp
// Testes unitários: fluxo EMV Contactless (CTLS)
// Cenários: aprovação, declinação online, fallback CT, segundo tap mobile

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "mocks/sdi_mock.h"
#include "ppcomp.h"
#include "ppcomp_internal.h"

using ::testing::Return;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::SetArgPointee;

class EMVCTLSTest : public ::testing::Test {
protected:
    SdiEmvMock   emv;
    SdiClassMock cls;

    char outBuf[512];
    int  outLen;

    void SetUp() override {
        if (g_initialized) PP_Close(0);
        SdiTestHelper::setupOpenSuccess(emv, cls);
        ASSERT_EQ(PP_OK, PP_Open(0, nullptr));

        g_tables_loaded = true;
        g_current_tec   = TEC_CTLS;
        g_pp_state      = PPState::CARD_CTLS;

        outLen = sizeof(outBuf);
        memset(outBuf, 0, sizeof(outBuf));
    }

    void TearDown() override {
        EXPECT_CALL(emv, SDI_CT_Exit_Framework()).WillRepeatedly(Return(EMV_ADK_OK));
        EXPECT_CALL(emv, SDI_CTLS_Exit_Framework()).WillRepeatedly(Return(EMV_ADK_OK));
        EXPECT_CALL(emv, SDI_ProtocolExit()).WillRepeatedly(Return(EMV_ADK_OK));
        EXPECT_CALL(cls, SDI_waitCardRemoval(_)).WillRepeatedly(Return(EMV_ADK_OK));
        PP_Close(0);
    }
};

// ─── PP_GoOnChipCTLS ──────────────────────────────────────────────────────────

TEST_F(EMVCTLSTest, GoOnChipCTLS_Success) {
    EXPECT_CALL(emv, SDI_CTLS_SetupTransaction(10000, 0, CURRENCY_BRL_CODE, _, _))
        .WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, dialog_requestCard(_, _)).WillOnce(Return(EMV_ADK_OK));

    EXPECT_EQ(PP_CTLS_WAITING, PP_GoOnChipCTLS(0, 10000, 0, 0, 1, outBuf, &outLen));
    EXPECT_EQ(PPState::EMV_CTLS_SETUP, g_pp_state);
}

TEST_F(EMVCTLSTest, GoOnChipCTLS_WrongTechnology) {
    g_current_tec = TEC_CHIP_CT;
    EXPECT_EQ(PP_ERR_NOTCHIP, PP_GoOnChipCTLS(0, 10000, 0, 0, 1, outBuf, &outLen));
}

TEST_F(EMVCTLSTest, GoOnChipCTLS_TablesNotLoaded) {
    g_tables_loaded = false;
    EXPECT_EQ(PP_ERR_STATE, PP_GoOnChipCTLS(0, 10000, 0, 0, 1, outBuf, &outLen));
}

TEST_F(EMVCTLSTest, GoOnChipCTLS_WrongState) {
    g_pp_state = PPState::IDLE;
    EXPECT_EQ(PP_ERR_STATE, PP_GoOnChipCTLS(0, 10000, 0, 0, 1, outBuf, &outLen));
}

TEST_F(EMVCTLSTest, GoOnChipCTLS_SDIFails) {
    EXPECT_CALL(emv, SDI_CTLS_SetupTransaction(_, _, _, _, _))
        .WillOnce(Return(EMV_ADK_COMM_ERROR));
    EXPECT_EQ(PP_ERR_PINPAD, PP_GoOnChipCTLS(0, 10000, 0, 0, 1, outBuf, &outLen));
}

TEST_F(EMVCTLSTest, GoOnChipCTLS_NullBuffer) {
    EXPECT_EQ(PP_ERR_BUFFER, PP_GoOnChipCTLS(0, 10000, 0, 0, 1, nullptr, nullptr));
}

// ─── PP_PollCTLS ──────────────────────────────────────────────────────────────

TEST_F(EMVCTLSTest, PollCTLS_OfflineApproval) {
    g_pp_state = PPState::EMV_CTLS_POLLING;

    EMV_CTLS_TRANSRES_STRUCT tr = {};
    tr.CryptogramType = EMV_CRYPTOGRAM_TC;
    EXPECT_CALL(emv, SDI_CTLS_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(tr), Return(EMV_ADK_OFFLINE_APPROVE)));

    EXPECT_EQ(PP_OFFLINE_APPROVE, PP_PollCTLS(0, outBuf, &outLen));
    EXPECT_EQ(PPState::EMV_CTLS_SETUP, g_pp_state);
    EXPECT_GT(outLen, 0);
}

TEST_F(EMVCTLSTest, PollCTLS_GoOnline) {
    g_pp_state = PPState::EMV_CTLS_POLLING;

    EMV_CTLS_TRANSRES_STRUCT tr = {};
    tr.CryptogramType = EMV_CRYPTOGRAM_ARQC;
    EXPECT_CALL(emv, SDI_CTLS_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(tr), Return(EMV_ADK_GO_ONLINE)));

    EXPECT_EQ(PP_GO_ONLINE, PP_PollCTLS(0, outBuf, &outLen));
}

TEST_F(EMVCTLSTest, PollCTLS_Decline) {
    g_pp_state = PPState::EMV_CTLS_POLLING;

    EMV_CTLS_TRANSRES_STRUCT tr = {};
    tr.CryptogramType = EMV_CRYPTOGRAM_AAC;
    EXPECT_CALL(emv, SDI_CTLS_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(tr), Return(EMV_ADK_DECLINE)));

    EXPECT_EQ(PP_OFFLINE_DECLINE, PP_PollCTLS(0, outBuf, &outLen));
}

TEST_F(EMVCTLSTest, PollCTLS_NoCard) {
    g_pp_state = PPState::EMV_CTLS_POLLING;

    EXPECT_CALL(emv, SDI_CTLS_ContinueOffline(_, _, _))
        .WillOnce(Return(EMV_ADK_NO_CARD));

    EXPECT_EQ(PP_ERR_NOEVENT, PP_PollCTLS(0, outBuf, &outLen));
    EXPECT_EQ(PPState::EMV_CTLS_POLLING, g_pp_state);  // Estado permanece
}

TEST_F(EMVCTLSTest, PollCTLS_Fallback) {
    g_pp_state = PPState::EMV_CTLS_POLLING;

    EXPECT_CALL(emv, SDI_CTLS_ContinueOffline(_, _, _))
        .WillOnce(Return(EMV_ADK_FALLBACK));
    EXPECT_CALL(emv, SDI_CTLS_Break()).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CTLS_EndTransaction(_)).WillOnce(Return(EMV_ADK_OK));

    EXPECT_EQ(PP_FALLBACK_CT, PP_PollCTLS(0, outBuf, &outLen));
}

TEST_F(EMVCTLSTest, PollCTLS_MobileSecondTap) {
    g_pp_state = PPState::EMV_CTLS_POLLING;

    EMV_CTLS_TRANSRES_STRUCT tr = {};
    EXPECT_CALL(emv, SDI_CTLS_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(tr), Return(EMV_ADK_TXN_CTLS_MOBILE)));

    EXPECT_EQ(PP_CTLS_WAITING, PP_PollCTLS(0, outBuf, &outLen));
    // Estado volta a aguardar segundo tap
    EXPECT_EQ(PPState::EMV_CTLS_POLLING, g_pp_state);
}

TEST_F(EMVCTLSTest, PollCTLS_WrongState) {
    g_pp_state = PPState::CARD_CTLS;  // Não está em POLLING
    EXPECT_EQ(PP_ERR_STATE, PP_PollCTLS(0, outBuf, &outLen));
}

TEST_F(EMVCTLSTest, PollCTLS_NullBuffer) {
    g_pp_state = PPState::EMV_CTLS_POLLING;
    EXPECT_EQ(PP_ERR_BUFFER, PP_PollCTLS(0, nullptr, nullptr));
}

// ─── PP_FinishChipCTLS ───────────────────────────────────────────────────────

TEST_F(EMVCTLSTest, FinishChipCTLS_OnlineApproval) {
    g_pp_state = PPState::EMV_CTLS_SETUP;

    uint8_t arpc[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    EMV_CTLS_TRANSRES_STRUCT tr = {};
    tr.CryptogramType = EMV_CRYPTOGRAM_TC;

    EXPECT_CALL(emv, SDI_CTLS_ContinueOnline(_, 8, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(tr), Return(EMV_ADK_OFFLINE_APPROVE)));
    EXPECT_CALL(emv, SDI_fetchTxnTags(_, _, _, _)).WillOnce(Return(EMV_ADK_OK));

    EXPECT_EQ(PP_OFFLINE_APPROVE,
              PP_FinishChipCTLS(0, reinterpret_cast<char*>(arpc), 8, nullptr, 0, outBuf, &outLen));
}

TEST_F(EMVCTLSTest, FinishChipCTLS_OnlineDecline) {
    g_pp_state = PPState::EMV_CTLS_SETUP;

    uint8_t arpc[] = {0x00, 0x01};
    EMV_CTLS_TRANSRES_STRUCT tr = {};
    tr.CryptogramType = EMV_CRYPTOGRAM_AAC;

    EXPECT_CALL(emv, SDI_CTLS_ContinueOnline(_, _, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(tr), Return(EMV_ADK_DECLINE)));
    EXPECT_CALL(emv, SDI_fetchTxnTags(_, _, _, _)).WillOnce(Return(EMV_ADK_OK));

    EXPECT_EQ(PP_OFFLINE_DECLINE,
              PP_FinishChipCTLS(0, reinterpret_cast<char*>(arpc), 2, nullptr, 0, outBuf, &outLen));
}

// ─── PP_AbortCTLS ─────────────────────────────────────────────────────────────

TEST_F(EMVCTLSTest, AbortCTLS_WhilePolling) {
    g_pp_state    = PPState::EMV_CTLS_POLLING;
    g_current_tec = TEC_CTLS;

    EXPECT_CALL(emv, SDI_CTLS_Break()).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CTLS_EndTransaction(_)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, SDI_waitCardRemoval(30)).WillOnce(Return(EMV_ADK_OK));

    EXPECT_EQ(PP_OK, PP_AbortCTLS(0));
    EXPECT_EQ(PPState::IDLE, g_pp_state);
}

TEST_F(EMVCTLSTest, AbortCTLS_NotInCTLSState) {
    g_pp_state = PPState::IDLE;
    EXPECT_EQ(PP_OK, PP_AbortCTLS(0));  // No-op silencioso
}
