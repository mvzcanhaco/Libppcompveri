// tests/test_lifecycle.cpp
// Testes unitários: PP_Open, PP_Close, PP_Reset, PP_GetInfo, PP_Abort

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "mocks/sdi_mock.h"
#include "ppcomp.h"
#include "ppcomp_internal.h"

using ::testing::Return;
using ::testing::_;
using ::testing::AnyNumber;

class LifecycleTest : public ::testing::Test {
protected:
    SdiEmvMock   emv;
    SdiClassMock cls;

    void SetUp() override {
        // Estado limpo antes de cada teste
        if (g_initialized) PP_Close(0);
    }

    void TearDown() override {
        if (g_initialized) {
            EXPECT_CALL(emv, SDI_CT_Exit_Framework()).WillOnce(Return(EMV_ADK_OK));
            EXPECT_CALL(emv, SDI_CTLS_Exit_Framework()).WillOnce(Return(EMV_ADK_OK));
            EXPECT_CALL(emv, SDI_ProtocolExit()).WillOnce(Return(EMV_ADK_OK));
            EXPECT_CALL(cls, SDI_waitCardRemoval(_)).WillRepeatedly(Return(EMV_ADK_OK));
            PP_Close(0);
        }
    }

    void openPinpad() {
        SdiTestHelper::setupOpenSuccess(emv, cls);
        ASSERT_EQ(PP_OK, PP_Open(0, nullptr));
    }
};

// ─── PP_Open ──────────────────────────────────────────────────────────────────

TEST_F(LifecycleTest, OpenSuccess) {
    SdiTestHelper::setupOpenSuccess(emv, cls);
    EXPECT_EQ(PP_OK, PP_Open(0, nullptr));
    EXPECT_TRUE(g_initialized);
    EXPECT_EQ(PPState::IDLE, g_pp_state);
}

TEST_F(LifecycleTest, OpenFailsWhenSDIProtocolInitFails) {
    EXPECT_CALL(emv, SDI_ProtocolInit(_)).WillOnce(Return(EMV_ADK_COMM_ERROR));
    EXPECT_EQ(PP_ERR_INIT, PP_Open(0, nullptr));
    EXPECT_FALSE(g_initialized);
}

TEST_F(LifecycleTest, OpenFailsWhenCTInitFails) {
    EXPECT_CALL(emv, SDI_ProtocolInit(_)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CT_Init_Framework(_)).WillOnce(Return(EMV_ADK_INTERNAL_ERROR));
    EXPECT_CALL(emv, SDI_ProtocolExit()).WillOnce(Return(EMV_ADK_OK));
    EXPECT_EQ(PP_ERR_INIT, PP_Open(0, nullptr));
}

TEST_F(LifecycleTest, OpenFailsWhenCTLSInitFails) {
    EXPECT_CALL(emv, SDI_ProtocolInit(_)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CT_Init_Framework(_)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CTLS_Init_Framework(_)).WillOnce(Return(EMV_ADK_INTERNAL_ERROR));
    EXPECT_CALL(emv, SDI_CT_Exit_Framework()).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_ProtocolExit()).WillOnce(Return(EMV_ADK_OK));
    EXPECT_EQ(PP_ERR_INIT, PP_Open(0, nullptr));
}

// ─── PP_Close ─────────────────────────────────────────────────────────────────

TEST_F(LifecycleTest, CloseSuccess) {
    openPinpad();
    EXPECT_CALL(emv, SDI_CT_Exit_Framework()).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CTLS_Exit_Framework()).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_ProtocolExit()).WillOnce(Return(EMV_ADK_OK));
    EXPECT_EQ(PP_OK, PP_Close(0));
    EXPECT_FALSE(g_initialized);
    EXPECT_FALSE(g_tables_loaded);
}

TEST_F(LifecycleTest, CloseWhenNotInitialized) {
    EXPECT_EQ(PP_ERR_INIT, PP_Close(0));
}

// ─── PP_Reset ─────────────────────────────────────────────────────────────────

TEST_F(LifecycleTest, ResetFromIdle) {
    openPinpad();
    EXPECT_EQ(PP_OK, PP_Reset(0));
    EXPECT_EQ(PPState::IDLE, g_pp_state);
}

TEST_F(LifecycleTest, ResetWhenNotInitialized) {
    EXPECT_EQ(PP_ERR_INIT, PP_Reset(0));
}

// ─── PP_GetInfo ───────────────────────────────────────────────────────────────

TEST_F(LifecycleTest, GetInfoVersion) {
    openPinpad();
    char buf[128] = {};
    EXPECT_EQ(PP_OK, PP_GetInfo(0, 0, buf, sizeof(buf)));
    EXPECT_STRNE("", buf);
    // Deve conter "libppcomp" mas não dados sensíveis
    EXPECT_NE(nullptr, strstr(buf, "libppcomp"));
}

TEST_F(LifecycleTest, GetInfoModel) {
    openPinpad();
    char buf[128] = {};
    EXPECT_EQ(PP_OK, PP_GetInfo(0, 1, buf, sizeof(buf)));
    EXPECT_NE(nullptr, strstr(buf, "V660P"));
}

TEST_F(LifecycleTest, GetInfoInvalidType) {
    openPinpad();
    char buf[128] = {};
    EXPECT_EQ(PP_ERR_PARAM, PP_GetInfo(0, 99, buf, sizeof(buf)));
}

TEST_F(LifecycleTest, GetInfoNullBuffer) {
    openPinpad();
    EXPECT_EQ(PP_ERR_PARAM, PP_GetInfo(0, 0, nullptr, 0));
}

TEST_F(LifecycleTest, GetInfoNotInitialized) {
    char buf[128] = {};
    EXPECT_EQ(PP_ERR_INIT, PP_GetInfo(0, 0, buf, sizeof(buf)));
}

// ─── PP_Abort ─────────────────────────────────────────────────────────────────

TEST_F(LifecycleTest, AbortFromIdle) {
    openPinpad();
    EXPECT_EQ(PP_OK, PP_Abort(0));
    EXPECT_EQ(PPState::IDLE, g_pp_state);
}

TEST_F(LifecycleTest, AbortFromDetecting) {
    openPinpad();
    g_pp_state = PPState::DETECTING;
    g_detection_active = true;

    EXPECT_CALL(cls, cardDetection_stopSelection()).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, SDI_waitCardRemoval(_)).WillOnce(Return(EMV_ADK_OK));

    EXPECT_EQ(PP_OK, PP_Abort(0));
    EXPECT_EQ(PPState::IDLE, g_pp_state);
}

TEST_F(LifecycleTest, AbortFromEMVCTStarted) {
    openPinpad();
    g_pp_state = PPState::EMV_CT_STARTED;

    EXPECT_CALL(emv, SDI_CT_EndTransaction(EMV_ADK_TXN_ABORT)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, SDI_waitCardRemoval(_)).WillOnce(Return(EMV_ADK_OK));

    EXPECT_EQ(PP_OK, PP_Abort(0));
    EXPECT_EQ(PPState::IDLE, g_pp_state);
}

TEST_F(LifecycleTest, AbortFromPINEntry) {
    openPinpad();
    g_pp_state = PPState::PIN;

    EXPECT_CALL(cls, ped_stopPinEntry()).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, SDI_waitCardRemoval(_)).WillOnce(Return(EMV_ADK_OK));

    EXPECT_EQ(PP_OK, PP_Abort(0));
    EXPECT_EQ(PPState::IDLE, g_pp_state);
}

TEST_F(LifecycleTest, AbortWhenNotInitialized) {
    EXPECT_EQ(PP_OK, PP_Abort(0));  // Não deve crashar
}
