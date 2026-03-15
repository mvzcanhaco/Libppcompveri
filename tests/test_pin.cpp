// tests/test_pin.cpp
// Testes unitários: PP_GetPIN, PP_GetPINBlock
// IMPORTANTE: testes usam mocks — nunca PIN real ou dados sensíveis reais

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "mocks/sdi_mock.h"
#include "ppcomp.h"
#include "ppcomp_internal.h"

using ::testing::Return;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;

class PINTest : public ::testing::Test {
protected:
    SdiEmvMock   emv;
    SdiClassMock cls;

    void SetUp() override {
        if (g_initialized) PP_Close(0);
        SdiTestHelper::setupOpenSuccess(emv, cls);
        ASSERT_EQ(PP_OK, PP_Open(0, nullptr));
        g_pp_state = PPState::EMV_CT_OFFLINE;
    }

    void TearDown() override {
        EXPECT_CALL(emv, SDI_CT_Exit_Framework()).WillRepeatedly(Return(EMV_ADK_OK));
        EXPECT_CALL(emv, SDI_CTLS_Exit_Framework()).WillRepeatedly(Return(EMV_ADK_OK));
        EXPECT_CALL(emv, SDI_ProtocolExit()).WillRepeatedly(Return(EMV_ADK_OK));
        EXPECT_CALL(cls, SDI_waitCardRemoval(_)).WillRepeatedly(Return(EMV_ADK_OK));
        PP_Close(0);
    }
};

// ─── PP_GetPIN ────────────────────────────────────────────────────────────────

TEST_F(PINTest, GetPIN_Online_Success) {
    EXPECT_CALL(cls, ped_setDefaultTimeout(30000)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_sendPinInputParameters(4, 6, 0)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_startPinInput(1)).WillOnce(Return(EMV_PED_OK));

    EXPECT_EQ(PP_OK, PP_GetPIN(0, 4, 6, 30, 1, PIN_VERIFICATION_ONLINE));
    EXPECT_TRUE(g_pin_entered);
    EXPECT_EQ(PPState::PIN, g_pp_state);
}

TEST_F(PINTest, GetPIN_Offline_NoHardwareCall) {
    // PIN offline não deve chamar o PED — kernel EMV gerencia
    EXPECT_CALL(cls, ped_startPinInput(_)).Times(0);
    EXPECT_EQ(PP_OK, PP_GetPIN(0, 4, 6, 30, 1, PIN_VERIFICATION_OFFLINE));
}

TEST_F(PINTest, GetPIN_Bypass) {
    EXPECT_CALL(cls, ped_setDefaultTimeout(_)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_sendPinInputParameters(_, _, _)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_startPinInput(_)).WillOnce(Return(EMV_PED_BYPASS));

    EXPECT_EQ(PP_PIN_BYPASS, PP_GetPIN(0, 4, 6, 30, 1, PIN_VERIFICATION_ONLINE));
    EXPECT_FALSE(g_pin_entered);
}

TEST_F(PINTest, GetPIN_UserCancel) {
    EXPECT_CALL(cls, ped_setDefaultTimeout(_)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_sendPinInputParameters(_, _, _)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_startPinInput(_)).WillOnce(Return(EMV_PED_CANCEL));

    EXPECT_EQ(PP_ABORT, PP_GetPIN(0, 4, 6, 30, 1, PIN_VERIFICATION_ONLINE));
}

TEST_F(PINTest, GetPIN_Timeout) {
    EXPECT_CALL(cls, ped_setDefaultTimeout(_)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_sendPinInputParameters(_, _, _)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_startPinInput(_)).WillOnce(Return(EMV_PED_TIMEOUT));

    EXPECT_EQ(PP_ERR_TIMEOUT, PP_GetPIN(0, 4, 6, 30, 1, PIN_VERIFICATION_ONLINE));
}

TEST_F(PINTest, GetPIN_WrongPIN) {
    EXPECT_CALL(cls, ped_setDefaultTimeout(_)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_sendPinInputParameters(_, _, _)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_startPinInput(_)).WillOnce(Return(EMV_PED_ERROR));
    // Callback sinaliza wrong_pin
    g_pin_retry_count = 1;

    EXPECT_EQ(PP_ERR_WRONG_PIN, PP_GetPIN(0, 4, 6, 30, 1, PIN_VERIFICATION_ONLINE));
}

TEST_F(PINTest, GetPIN_PINBlocked) {
    EXPECT_CALL(cls, ped_setDefaultTimeout(_)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_sendPinInputParameters(_, _, _)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_startPinInput(_)).WillOnce(Return(EMV_PED_ERROR));
    // Callback sinaliza pin_blocked
    g_pin_blocked = true;

    EXPECT_EQ(PP_ERR_PIN_BLOCKED, PP_GetPIN(0, 4, 6, 30, 1, PIN_VERIFICATION_ONLINE));
}

TEST_F(PINTest, GetPIN_NotInitialized) {
    PP_Close(0);
    EXPECT_EQ(PP_ERR_INIT, PP_GetPIN(0, 4, 6, 30, 1, PIN_VERIFICATION_ONLINE));
    // Re-open para TearDown
    SdiTestHelper::setupOpenSuccess(emv, cls);
    PP_Open(0, nullptr);
}

// ─── PP_GetPINBlock ───────────────────────────────────────────────────────────

TEST_F(PINTest, GetPINBlock_Success) {
    g_pin_entered = true;

    SdiTestHelper::setupPinSuccess(cls);

    unsigned char pinBlock[8] = {};
    int           pinBlockLen = 8;
    unsigned char ksn[10]     = {};
    int           ksnLen      = 10;

    EXPECT_EQ(PP_OK, PP_GetPINBlock(0, pinBlock, &pinBlockLen, ksn, &ksnLen));
    EXPECT_EQ(8, pinBlockLen);
    EXPECT_EQ(10, ksnLen);
    // NUNCA verificar conteúdo real do PIN block em testes de unidade
    // Apenas verificar tamanhos
}

TEST_F(PINTest, GetPINBlock_PINNotEntered) {
    g_pin_entered = false;
    unsigned char pinBlock[8] = {};
    int           pinBlockLen = 8;
    unsigned char ksn[10]     = {};
    int           ksnLen      = 10;
    EXPECT_EQ(PP_ERR_INIT, PP_GetPINBlock(0, pinBlock, &pinBlockLen, ksn, &ksnLen));
}

TEST_F(PINTest, GetPINBlock_BufferTooSmall) {
    g_pin_entered = true;
    unsigned char pinBlock[4] = {};  // Menor que PIN_BLOCK_LEN
    int           pinBlockLen = 4;
    unsigned char ksn[10]     = {};
    int           ksnLen      = 10;
    EXPECT_EQ(PP_ERR_BUFFER, PP_GetPINBlock(0, pinBlock, &pinBlockLen, ksn, &ksnLen));
}

TEST_F(PINTest, GetPINBlock_CryptGetEncryptedPinFails) {
    g_pin_entered = true;
    EXPECT_CALL(cls, ped_setDefaultTimeout(_)).Times(0);
    EXPECT_CALL(cls, crypt_getEncryptedPin(_)).WillOnce(Return(EMV_ADK_COMM_ERROR));

    unsigned char pinBlock[8] = {};
    int           pinBlockLen = 8;
    unsigned char ksn[10]     = {};
    int           ksnLen      = 10;
    EXPECT_EQ(PP_ERR_PINPAD, PP_GetPINBlock(0, pinBlock, &pinBlockLen, ksn, &ksnLen));
}
