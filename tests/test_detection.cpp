// tests/test_detection.cpp
// Testes unitários: PP_StartCheckEvent, PP_CheckEvent, PP_AbortCheckEvent

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "mocks/sdi_mock.h"
#include "ppcomp.h"
#include "ppcomp_internal.h"

using ::testing::Return;
using ::testing::_;
using ::testing::AnyNumber;

class DetectionTest : public ::testing::Test {
protected:
    SdiEmvMock   emv;
    SdiClassMock cls;

    void SetUp() override {
        if (g_initialized) PP_Close(0);
        SdiTestHelper::setupOpenSuccess(emv, cls);
        ASSERT_EQ(PP_OK, PP_Open(0, nullptr));
        g_tables_loaded = true;
        g_pp_state      = PPState::IDLE;
        g_detection_active = false;
    }

    void TearDown() override {
        EXPECT_CALL(emv, SDI_CT_Exit_Framework()).WillRepeatedly(Return(EMV_ADK_OK));
        EXPECT_CALL(emv, SDI_CTLS_Exit_Framework()).WillRepeatedly(Return(EMV_ADK_OK));
        EXPECT_CALL(emv, SDI_ProtocolExit()).WillRepeatedly(Return(EMV_ADK_OK));
        EXPECT_CALL(cls, SDI_waitCardRemoval(_)).WillRepeatedly(Return(EMV_ADK_OK));
        if (g_detection_active) {
            EXPECT_CALL(cls, cardDetection_stopSelection()).WillRepeatedly(Return(EMV_ADK_OK));
        }
        PP_Close(0);
    }
};

// ─── PP_StartCheckEvent ──────────────────────────────────────────────────────

TEST_F(DetectionTest, StartCheckEvent_Success) {
    EXPECT_CALL(cls, cardDetection_startSelection(_, _, _, _)).WillOnce(Return(EMV_ADK_OK));

    // modalidades: CT (0x01) | CTLS (0x02) | MSR (0x04)
    EXPECT_EQ(PP_OK, PP_StartCheckEvent(0, 0x07, 30));
    EXPECT_EQ(PPState::DETECTING, g_pp_state);
    EXPECT_TRUE(g_detection_active);
}

TEST_F(DetectionTest, StartCheckEvent_TablesNotLoaded) {
    g_tables_loaded = false;
    EXPECT_EQ(PP_ERR_STATE, PP_StartCheckEvent(0, 0x07, 30));
    EXPECT_EQ(PPState::IDLE, g_pp_state);
}

TEST_F(DetectionTest, StartCheckEvent_WrongState) {
    g_pp_state = PPState::DETECTING;
    g_detection_active = true;
    EXPECT_CALL(cls, cardDetection_stopSelection()).WillRepeatedly(Return(EMV_ADK_OK));
    EXPECT_EQ(PP_ERR_STATE, PP_StartCheckEvent(0, 0x07, 30));
}

TEST_F(DetectionTest, StartCheckEvent_NotInitialized) {
    PP_Close(0);
    EXPECT_EQ(PP_ERR_INIT, PP_StartCheckEvent(0, 0x07, 30));
    // Re-open para TearDown
    SdiTestHelper::setupOpenSuccess(emv, cls);
    PP_Open(0, nullptr);
    g_tables_loaded = true;
}

TEST_F(DetectionTest, StartCheckEvent_SDIFails) {
    EXPECT_CALL(cls, cardDetection_startSelection(_, _, _, _)).WillOnce(Return(EMV_ADK_COMM_ERROR));
    EXPECT_EQ(PP_ERR_PINPAD, PP_StartCheckEvent(0, 0x07, 30));
    EXPECT_EQ(PPState::IDLE, g_pp_state);
    EXPECT_FALSE(g_detection_active);
}

TEST_F(DetectionTest, StartCheckEvent_NoModalidades) {
    // Máscara zero — nenhuma tecnologia selecionada
    EXPECT_EQ(PP_ERR_PARAM, PP_StartCheckEvent(0, 0x00, 30));
}

// ─── PP_CheckEvent ───────────────────────────────────────────────────────────

TEST_F(DetectionTest, CheckEvent_ChipCT_Detected) {
    g_pp_state         = PPState::DETECTING;
    g_detection_active = true;

    EXPECT_CALL(cls, cardDetection_pollTechnology())
        .WillOnce(Return(libsdi::Technology::CHIP_CT));

    char out[256] = {};
    int  outLen   = sizeof(out);
    EXPECT_EQ(PP_OK, PP_CheckEvent(0, out, &outLen));
    EXPECT_EQ(PPState::CARD_CT, g_pp_state);
    EXPECT_EQ(TEC_CHIP_CT, g_current_tec);
}

TEST_F(DetectionTest, CheckEvent_CTLS_Detected) {
    g_pp_state         = PPState::DETECTING;
    g_detection_active = true;

    EXPECT_CALL(cls, cardDetection_pollTechnology())
        .WillOnce(Return(libsdi::Technology::CTLS));

    char out[256] = {};
    int  outLen   = sizeof(out);
    EXPECT_EQ(PP_OK, PP_CheckEvent(0, out, &outLen));
    EXPECT_EQ(PPState::CARD_CTLS, g_pp_state);
    EXPECT_EQ(TEC_CTLS, g_current_tec);
}

TEST_F(DetectionTest, CheckEvent_MSR_Detected) {
    g_pp_state         = PPState::DETECTING;
    g_detection_active = true;

    EXPECT_CALL(cls, cardDetection_pollTechnology())
        .WillOnce(Return(libsdi::Technology::MSR));
    // SDI_fetchTxnTags para obter trilhas criptografadas
    EXPECT_CALL(emv, SDI_fetchTxnTags(_, _, _, _)).WillOnce(Return(EMV_ADK_OK));

    char out[512] = {};
    int  outLen   = sizeof(out);
    EXPECT_EQ(PP_OK, PP_CheckEvent(0, out, &outLen));
    EXPECT_EQ(PPState::CARD_MSR, g_pp_state);
    EXPECT_EQ(TEC_MSR, g_current_tec);
}

TEST_F(DetectionTest, CheckEvent_NoCard_YetDetected) {
    g_pp_state         = PPState::DETECTING;
    g_detection_active = true;

    EXPECT_CALL(cls, cardDetection_pollTechnology())
        .WillOnce(Return(libsdi::Technology::NONE));

    char out[256] = {};
    int  outLen   = sizeof(out);
    EXPECT_EQ(PP_ERR_NOEVENT, PP_CheckEvent(0, out, &outLen));
    EXPECT_EQ(PPState::DETECTING, g_pp_state);  // Estado não muda
}

TEST_F(DetectionTest, CheckEvent_NotDetecting) {
    g_pp_state = PPState::IDLE;
    char out[256] = {};
    int  outLen   = sizeof(out);
    EXPECT_EQ(PP_ERR_STATE, PP_CheckEvent(0, out, &outLen));
}

TEST_F(DetectionTest, CheckEvent_NullBuffer) {
    g_pp_state         = PPState::DETECTING;
    g_detection_active = true;
    EXPECT_EQ(PP_ERR_BUFFER, PP_CheckEvent(0, nullptr, nullptr));
}

// ─── PP_AbortCheckEvent ──────────────────────────────────────────────────────

TEST_F(DetectionTest, AbortCheckEvent_WhileDetecting) {
    g_pp_state         = PPState::DETECTING;
    g_detection_active = true;

    EXPECT_CALL(cls, cardDetection_stopSelection()).WillOnce(Return(EMV_ADK_OK));

    EXPECT_EQ(PP_OK, PP_AbortCheckEvent(0));
    EXPECT_EQ(PPState::IDLE, g_pp_state);
    EXPECT_FALSE(g_detection_active);
}

TEST_F(DetectionTest, AbortCheckEvent_NotDetecting) {
    g_pp_state = PPState::IDLE;
    // Não deve chamar stopSelection se não estava detectando
    EXPECT_CALL(cls, cardDetection_stopSelection()).Times(0);
    EXPECT_EQ(PP_OK, PP_AbortCheckEvent(0));
}

TEST_F(DetectionTest, AbortCheckEvent_NotInitialized) {
    PP_Close(0);
    EXPECT_EQ(PP_ERR_INIT, PP_AbortCheckEvent(0));
    // Re-open para TearDown
    SdiTestHelper::setupOpenSuccess(emv, cls);
    PP_Open(0, nullptr);
    g_tables_loaded = true;
}
