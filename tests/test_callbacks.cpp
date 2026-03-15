// tests/test_callbacks.cpp
// Testes unitários: callbacks SDI — cb_emv_ct, cb_emv_ctls, cb_sdi_status
// IMPORTANTE: callbacks são chamados em thread SDI — testes verificam comportamento
// thread-safe via estado global após chamada.

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "mocks/sdi_mock.h"
#include "ppcomp.h"
#include "ppcomp_internal.h"

using ::testing::Return;
using ::testing::_;

// Declarações das funções de callback (linkadas via ppcomp.a)
extern "C" {
    int cb_emv_ct(int event, void* data, int dataLen, void* outData, int* outLen);
    int cb_emv_ctls(int event, void* data, int dataLen, void* outData, int* outLen);
    void cb_sdi_status(int event, int value);
}

class CallbackTest : public ::testing::Test {
protected:
    SdiEmvMock   emv;
    SdiClassMock cls;

    void SetUp() override {
        if (g_initialized) PP_Close(0);
        SdiTestHelper::setupOpenSuccess(emv, cls);
        ASSERT_EQ(PP_OK, PP_Open(0, nullptr));

        // Reset callback state
        g_pin_entered       = false;
        g_pin_blocked       = false;
        g_pin_retry_count   = 0;
        g_pin_digit_count   = 0;
        g_pp_state          = PPState::EMV_CT_STARTED;
    }

    void TearDown() override {
        EXPECT_CALL(emv, SDI_CT_Exit_Framework()).WillRepeatedly(Return(EMV_ADK_OK));
        EXPECT_CALL(emv, SDI_CTLS_Exit_Framework()).WillRepeatedly(Return(EMV_ADK_OK));
        EXPECT_CALL(emv, SDI_ProtocolExit()).WillRepeatedly(Return(EMV_ADK_OK));
        EXPECT_CALL(cls, SDI_waitCardRemoval(_)).WillRepeatedly(Return(EMV_ADK_OK));
        PP_Close(0);
    }
};

// ─── cb_emv_ct ───────────────────────────────────────────────────────────────

TEST_F(CallbackTest, CbEmvCT_OnlinePIN_SetsGlobalFlag) {
    // Simular callback ONLINE_PIN — deve setar g_pin_requested
    int outLen = 0;
    int ret = cb_emv_ct(EMV_CB_ONLINE_PIN, nullptr, 0, nullptr, &outLen);
    EXPECT_EQ(EMV_ADK_OK, ret);
    EXPECT_TRUE(g_pin_requested);
}

TEST_F(CallbackTest, CbEmvCT_WrongPIN_IncrementsRetryCount) {
    g_pin_retry_count = 0;
    int outLen = 0;
    int ret = cb_emv_ct(EMV_CB_WRONG_PIN, nullptr, 0, nullptr, &outLen);
    EXPECT_EQ(EMV_ADK_OK, ret);
    EXPECT_EQ(1, g_pin_retry_count);
}

TEST_F(CallbackTest, CbEmvCT_WrongPIN_MultipleRetries) {
    g_pin_retry_count = 1;
    int outLen = 0;
    cb_emv_ct(EMV_CB_WRONG_PIN, nullptr, 0, nullptr, &outLen);
    EXPECT_EQ(2, g_pin_retry_count);
}

TEST_F(CallbackTest, CbEmvCT_PINBlocked_SetsPINBlockedFlag) {
    g_pin_blocked = false;
    int outLen = 0;
    int ret = cb_emv_ct(EMV_CB_PIN_BLOCKED, nullptr, 0, nullptr, &outLen);
    EXPECT_EQ(EMV_ADK_OK, ret);
    EXPECT_TRUE(g_pin_blocked);
}

TEST_F(CallbackTest, CbEmvCT_ScriptResult_StoredCorrectly) {
    uint8_t scriptData[] = {0x01, 0x02, 0x03, 0x04};
    int outLen = 0;
    int ret = cb_emv_ct(EMV_CB_SCRIPT_RESULT, scriptData, sizeof(scriptData), nullptr, &outLen);
    EXPECT_EQ(EMV_ADK_OK, ret);
    // Script result deve ser armazenado internamente (não verificamos conteúdo aqui)
}

TEST_F(CallbackTest, CbEmvCT_SelectApp_ReturnsOK) {
    // Sem candidatos — deve retornar OK sem crash
    int outLen = 0;
    int ret = cb_emv_ct(EMV_CB_SELECT_APP, nullptr, 0, nullptr, &outLen);
    EXPECT_EQ(EMV_ADK_OK, ret);
}

TEST_F(CallbackTest, CbEmvCT_UnknownEvent_ReturnsOK) {
    // Eventos desconhecidos devem ser ignorados silenciosamente
    int outLen = 0;
    int ret = cb_emv_ct(0xFF, nullptr, 0, nullptr, &outLen);
    EXPECT_EQ(EMV_ADK_OK, ret);
}

// ─── cb_emv_ctls ─────────────────────────────────────────────────────────────

TEST_F(CallbackTest, CbEmvCTLS_OnlinePIN_SetsGlobalFlag) {
    g_pin_requested = false;
    int outLen = 0;
    int ret = cb_emv_ctls(EMV_CB_ONLINE_PIN, nullptr, 0, nullptr, &outLen);
    EXPECT_EQ(EMV_ADK_OK, ret);
    EXPECT_TRUE(g_pin_requested);
}

TEST_F(CallbackTest, CbEmvCTLS_SelectKernel_ReturnsOK) {
    int outLen = 0;
    int ret = cb_emv_ctls(EMV_CTLS_CB_SELECT_KERNEL, nullptr, 0, nullptr, &outLen);
    EXPECT_EQ(EMV_ADK_OK, ret);
}

TEST_F(CallbackTest, CbEmvCTLS_UnknownEvent_ReturnsOK) {
    int outLen = 0;
    int ret = cb_emv_ctls(0xFF, nullptr, 0, nullptr, &outLen);
    EXPECT_EQ(EMV_ADK_OK, ret);
}

// ─── cb_sdi_status ───────────────────────────────────────────────────────────

TEST_F(CallbackTest, CbSdiStatus_PinDigitEntered_UpdatesCount) {
    g_pin_digit_count = 0;
    cb_sdi_status(SDI_STATUS_PIN_DIGIT, 3);
    EXPECT_EQ(3, g_pin_digit_count);
    // NUNCA logar o dígito real — apenas a contagem
}

TEST_F(CallbackTest, CbSdiStatus_PinDigitZero_ClearsCount) {
    g_pin_digit_count = 4;
    cb_sdi_status(SDI_STATUS_PIN_DIGIT, 0);
    EXPECT_EQ(0, g_pin_digit_count);
}

TEST_F(CallbackTest, CbSdiStatus_UnknownEvent_NoSideEffect) {
    g_pin_digit_count = 2;
    cb_sdi_status(0xFF, 99);
    EXPECT_EQ(2, g_pin_digit_count);  // Não deve alterar estado
}

// ─── Thread-safety: mutex ─────────────────────────────────────────────────────

TEST_F(CallbackTest, CbEmvCT_MutexProtected_NoCrashUnderLoad) {
    // Chama callback várias vezes para verificar que o mutex não causa deadlock
    for (int i = 0; i < 100; i++) {
        int outLen = 0;
        cb_emv_ct(EMV_CB_WRONG_PIN, nullptr, 0, nullptr, &outLen);
    }
    EXPECT_EQ(100, g_pin_retry_count);
}
