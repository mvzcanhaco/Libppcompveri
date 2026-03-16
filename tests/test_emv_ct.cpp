// tests/test_emv_ct.cpp
// Testes unitários: fluxo EMV Contact (CT)
// Cenários: aprovação offline, aprovação online, declinação, multiapp, fallback

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

class EMVCTTest : public ::testing::Test {
protected:
    SdiEmvMock   emv;
    SdiClassMock cls;

    char outBuf[512];
    int  outLen;

    void SetUp() override {
        if (g_initialized) PP_Close(0);
        SdiTestHelper::setupOpenSuccess(emv, cls);
        ASSERT_EQ(PP_OK, PP_Open(0, nullptr));

        // Simular tabelas carregadas
        g_tables_loaded = true;
        g_current_tec   = TEC_CHIP_CT;
        g_pp_state      = PPState::CARD_CT;

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

// ─── PP_GoOnChip ──────────────────────────────────────────────────────────────

TEST_F(EMVCTTest, GoOnChip_Success) {
    EMV_CT_STARTRESULT_STRUCT sr = {};
    sr.NumCandidates = 1;
    sr.SelectedAIDLen = 7;
    EXPECT_CALL(emv, SDI_CT_StartTransaction(10000, 0, CURRENCY_BRL_CODE, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(sr), Return(EMV_ADK_OK)));

    EXPECT_EQ(PP_OK, PP_GoOnChip(0, 10000, 0, 0, 1, outBuf, &outLen));
    EXPECT_EQ(PPState::EMV_CT_STARTED, g_pp_state);
}

TEST_F(EMVCTTest, GoOnChip_MultiApp) {
    EMV_CT_STARTRESULT_STRUCT sr = {};
    sr.NumCandidates = 2;
    EXPECT_CALL(emv, SDI_CT_StartTransaction(_, _, _, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(sr), Return(EMV_ADK_MULTI_APP)));

    EXPECT_EQ(PP_MULTIAPP, PP_GoOnChip(0, 5000, 0, 0, 1, outBuf, &outLen));
    EXPECT_GT(outLen, 0);  // Buffer deve conter a lista de candidatos
}

TEST_F(EMVCTTest, GoOnChip_WrongTechnology) {
    g_current_tec = TEC_CTLS;
    EXPECT_EQ(PP_ERR_NOTCHIP, PP_GoOnChip(0, 10000, 0, 0, 1, outBuf, &outLen));
}

TEST_F(EMVCTTest, GoOnChip_TablesNotLoaded) {
    g_tables_loaded = false;
    EXPECT_EQ(PP_ERR_STATE, PP_GoOnChip(0, 10000, 0, 0, 1, outBuf, &outLen));
}

TEST_F(EMVCTTest, GoOnChip_WrongState) {
    g_pp_state = PPState::IDLE;
    EXPECT_EQ(PP_ERR_STATE, PP_GoOnChip(0, 10000, 0, 0, 1, outBuf, &outLen));
}

TEST_F(EMVCTTest, GoOnChip_NullBuffer) {
    EXPECT_EQ(PP_ERR_BUFFER, PP_GoOnChip(0, 10000, 0, 0, 1, nullptr, nullptr));
}

TEST_F(EMVCTTest, GoOnChip_SDIFails) {
    EXPECT_CALL(emv, SDI_CT_StartTransaction(_, _, _, _, _))
        .WillOnce(Return(EMV_ADK_COMM_ERROR));
    EXPECT_EQ(PP_ERR_PINPAD, PP_GoOnChip(0, 10000, 0, 0, 1, outBuf, &outLen));
}

// ─── PP_GoOnChipContinue ──────────────────────────────────────────────────────

TEST_F(EMVCTTest, GoOnChipContinue_OfflineApproval) {
    g_pp_state = PPState::EMV_CT_STARTED;

    EMV_CT_TRANSRES_STRUCT tr = {};
    tr.CryptogramType = EMV_CRYPTOGRAM_TC;
    EXPECT_CALL(emv, SDI_CT_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(tr), Return(EMV_ADK_OFFLINE_APPROVE)));

    EXPECT_EQ(PP_OFFLINE_APPROVE, PP_GoOnChipContinue(0, outBuf, &outLen));
    EXPECT_EQ(PPState::EMV_CT_OFFLINE, g_pp_state);
    EXPECT_GT(outLen, 0);
}

TEST_F(EMVCTTest, GoOnChipContinue_GoOnline) {
    g_pp_state = PPState::EMV_CT_STARTED;

    EMV_CT_TRANSRES_STRUCT tr = {};
    tr.CryptogramType = EMV_CRYPTOGRAM_ARQC;
    EXPECT_CALL(emv, SDI_CT_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(tr), Return(EMV_ADK_GO_ONLINE)));

    EXPECT_EQ(PP_GO_ONLINE, PP_GoOnChipContinue(0, outBuf, &outLen));
}

TEST_F(EMVCTTest, GoOnChipContinue_OfflineDecline) {
    g_pp_state = PPState::EMV_CT_STARTED;

    EMV_CT_TRANSRES_STRUCT tr = {};
    tr.CryptogramType = EMV_CRYPTOGRAM_AAC;
    EXPECT_CALL(emv, SDI_CT_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(tr), Return(EMV_ADK_DECLINE)));

    EXPECT_EQ(PP_OFFLINE_DECLINE, PP_GoOnChipContinue(0, outBuf, &outLen));
}

TEST_F(EMVCTTest, GoOnChipContinue_CardRemovedMidTransaction) {
    g_pp_state = PPState::EMV_CT_STARTED;
    g_card_removed_mid_txn = true;

    EMV_CT_TRANSRES_STRUCT tr = {};
    EXPECT_CALL(emv, SDI_CT_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(tr), Return(EMV_ADK_OFFLINE_APPROVE)));

    EXPECT_EQ(PP_ERR_PINPAD, PP_GoOnChipContinue(0, outBuf, &outLen));
}

// ─── PP_FinishChip ────────────────────────────────────────────────────────────

TEST_F(EMVCTTest, FinishChip_OnlineApproval) {
    g_pp_state = PPState::EMV_CT_OFFLINE;

    uint8_t arpc[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    EMV_CT_TRANSRES_STRUCT tr = {};
    tr.CryptogramType = EMV_CRYPTOGRAM_TC;

    EXPECT_CALL(emv, SDI_CT_ContinueOnline(_, 8, _, _, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(tr), Return(EMV_ADK_OFFLINE_APPROVE)));
    EXPECT_CALL(emv, SDI_fetchTxnTags(_, _, _, _)).WillOnce(Return(EMV_ADK_OK));

    EXPECT_EQ(PP_OFFLINE_APPROVE,
              PP_FinishChip(0, reinterpret_cast<char*>(arpc), 8, nullptr, 0, outBuf, &outLen));
}

TEST_F(EMVCTTest, FinishChip_OnlineDecline) {
    g_pp_state = PPState::EMV_CT_OFFLINE;

    uint8_t arpc[] = {0x00, 0x01, 0x02, 0x03};
    EMV_CT_TRANSRES_STRUCT tr = {};
    tr.CryptogramType = EMV_CRYPTOGRAM_AAC;

    EXPECT_CALL(emv, SDI_CT_ContinueOnline(_, _, _, _, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(tr), Return(EMV_ADK_DECLINE)));
    EXPECT_CALL(emv, SDI_fetchTxnTags(_, _, _, _)).WillOnce(Return(EMV_ADK_OK));

    EXPECT_EQ(PP_OFFLINE_DECLINE,
              PP_FinishChip(0, reinterpret_cast<char*>(arpc), 4, nullptr, 0, outBuf, &outLen));
}

// ─── PP_EndChip ───────────────────────────────────────────────────────────────

TEST_F(EMVCTTest, EndChip_Normal) {
    g_pp_state    = PPState::EMV_CT_ONLINE;
    g_current_tec = TEC_CHIP_CT;

    EXPECT_CALL(emv, SDI_CT_EndTransaction(0)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, SDI_waitCardRemoval(30)).WillOnce(Return(EMV_ADK_OK));

    EXPECT_EQ(PP_OK, PP_EndChip(0, 0));
    EXPECT_EQ(PPState::IDLE, g_pp_state);
    EXPECT_EQ(TEC_NONE, g_current_tec);
}

TEST_F(EMVCTTest, EndChip_Cancelled) {
    g_pp_state = PPState::EMV_CT_STARTED;

    EXPECT_CALL(emv, SDI_CT_EndTransaction(EMV_ADK_TXN_ABORT)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, SDI_waitCardRemoval(30)).WillOnce(Return(EMV_ADK_OK));

    EXPECT_EQ(PP_OK, PP_EndChip(0, 1));
}

// ─── PP_SetAID ────────────────────────────────────────────────────────────────

TEST_F(EMVCTTest, SetAID_Success) {
    g_pp_state = PPState::EMV_CT_STARTED;

    uint8_t aid[] = {0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10};
    EXPECT_CALL(emv, SDI_CT_SetSelectedApp(_, 7)).WillOnce(Return(EMV_ADK_OK));

    EXPECT_EQ(PP_OK, PP_SetAID(0, aid, sizeof(aid)));
}

TEST_F(EMVCTTest, SetAID_NullAID) {
    EXPECT_EQ(PP_ERR_PARAM, PP_SetAID(0, nullptr, 0));
}

TEST_F(EMVCTTest, SetAID_AIDTooLong) {
    // AID máximo é 16 bytes (EMVCo)
    uint8_t aid[17] = {};
    EXPECT_EQ(PP_ERR_PARAM, PP_SetAID(0, aid, sizeof(aid)));
}

TEST_F(EMVCTTest, SetAID_SDIFails) {
    g_pp_state = PPState::EMV_CT_STARTED;
    uint8_t aid[] = {0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10};
    EXPECT_CALL(emv, SDI_CT_SetSelectedApp(_, 7)).WillOnce(Return(EMV_ADK_PARAM_ERROR));
    EXPECT_EQ(PP_ERR_PARAM, PP_SetAID(0, aid, sizeof(aid)));
}

// ─── PP_GetCard ───────────────────────────────────────────────────────────────

TEST_F(EMVCTTest, GetCard_Success) {
    g_pp_state    = PPState::CARD_MSR;
    g_current_tec = TEC_MSR;

    // SDI_fetchTxnTags retorna dados mascarados de PAN e data de expiração
    EXPECT_CALL(emv, SDI_fetchTxnTags(_, _, _, _)).WillOnce(Return(EMV_ADK_OK));

    EXPECT_EQ(PP_OK, PP_GetCard(0, outBuf, &outLen));
    // Não verificar conteúdo — PAN é mascarado pelo SDI (P2PE)
}

TEST_F(EMVCTTest, GetCard_WrongState) {
    g_pp_state = PPState::IDLE;
    EXPECT_EQ(PP_ERR_STATE, PP_GetCard(0, outBuf, &outLen));
}

TEST_F(EMVCTTest, GetCard_NullBuffer) {
    g_pp_state    = PPState::CARD_MSR;
    g_current_tec = TEC_MSR;
    EXPECT_EQ(PP_ERR_BUFFER, PP_GetCard(0, nullptr, nullptr));
}

TEST_F(EMVCTTest, GetCard_SDIFails) {
    g_pp_state    = PPState::EMV_CT_OFFLINE;
    g_current_tec = TEC_CHIP_CT;
    EXPECT_CALL(emv, SDI_fetchTxnTags(_, _, _, _)).WillOnce(Return(EMV_ADK_COMM_ERROR));
    EXPECT_EQ(PP_ERR_PINPAD, PP_GetCard(0, outBuf, &outLen));
}
