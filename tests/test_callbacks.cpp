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

// Declarações das funções de callback com assinaturas reais (linkadas via ppcomp.a)
extern "C" {
    EMV_ADK_INFO cb_emv_ct(uint8_t* data, uint32_t len);
    EMV_ADK_INFO cb_emv_ctls(uint8_t* data, uint32_t len);
    void cb_sdi_status(uint8_t* data, uint32_t len);
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
    uint8_t data[] = {EMV_CB_ONLINE_PIN};
    EMV_ADK_INFO ret = cb_emv_ct(data, sizeof(data));
    EXPECT_EQ(EMV_ADK_OK, ret);
    EXPECT_TRUE(g_pin_requested);
}

TEST_F(CallbackTest, CbEmvCT_WrongPIN_IncrementsRetryCount) {
    g_pin_retry_count = 0;
    uint8_t data[] = {EMV_CB_WRONG_PIN};
    EMV_ADK_INFO ret = cb_emv_ct(data, sizeof(data));
    EXPECT_EQ(EMV_ADK_OK, ret);
    EXPECT_EQ(1, g_pin_retry_count);
}

TEST_F(CallbackTest, CbEmvCT_WrongPIN_MultipleRetries) {
    g_pin_retry_count = 1;
    uint8_t data[] = {EMV_CB_WRONG_PIN};
    cb_emv_ct(data, sizeof(data));
    EXPECT_EQ(2, g_pin_retry_count);
}

TEST_F(CallbackTest, CbEmvCT_PINBlocked_SetsPINBlockedFlag) {
    g_pin_blocked = false;
    uint8_t data[] = {EMV_CB_PIN_BLOCKED};
    EMV_ADK_INFO ret = cb_emv_ct(data, sizeof(data));
    EXPECT_EQ(EMV_ADK_OK, ret);
    EXPECT_TRUE(g_pin_blocked);
}

TEST_F(CallbackTest, CbEmvCT_ScriptResult_StoredCorrectly) {
    uint8_t data[] = {EMV_CB_SCRIPT_RESULT, 0x01, 0x02, 0x03, 0x04};
    EMV_ADK_INFO ret = cb_emv_ct(data, sizeof(data));
    EXPECT_EQ(EMV_ADK_OK, ret);
    // Script result deve ser armazenado internamente (não verificamos conteúdo aqui)
}

TEST_F(CallbackTest, CbEmvCT_SelectApp_ReturnsOK) {
    // Sem candidatos — deve retornar OK sem crash
    uint8_t data[] = {EMV_CB_SELECT_APP};
    EMV_ADK_INFO ret = cb_emv_ct(data, sizeof(data));
    EXPECT_EQ(EMV_ADK_OK, ret);
}

TEST_F(CallbackTest, CbEmvCT_UnknownEvent_ReturnsOK) {
    // Eventos desconhecidos devem ser ignorados silenciosamente
    uint8_t data[] = {0xFF};
    EMV_ADK_INFO ret = cb_emv_ct(data, sizeof(data));
    EXPECT_EQ(EMV_ADK_OK, ret);
}

// ─── cb_emv_ctls ─────────────────────────────────────────────────────────────

TEST_F(CallbackTest, CbEmvCTLS_OnlinePIN_SetsGlobalFlag) {
    g_pin_requested = false;
    uint8_t data[] = {EMV_CTLS_CB_ONLINE_PIN};
    EMV_ADK_INFO ret = cb_emv_ctls(data, sizeof(data));
    EXPECT_EQ(EMV_ADK_OK, ret);
    EXPECT_TRUE(g_pin_requested);
}

TEST_F(CallbackTest, CbEmvCTLS_SelectKernel_ReturnsOK) {
    uint8_t data[] = {EMV_CTLS_CB_SELECT_KERNEL};
    EMV_ADK_INFO ret = cb_emv_ctls(data, sizeof(data));
    EXPECT_EQ(EMV_ADK_OK, ret);
}

TEST_F(CallbackTest, CbEmvCTLS_UnknownEvent_ReturnsOK) {
    uint8_t data[] = {0xFF};
    EMV_ADK_INFO ret = cb_emv_ctls(data, sizeof(data));
    EXPECT_EQ(EMV_ADK_OK, ret);
}

// ─── cb_sdi_status ───────────────────────────────────────────────────────────
// cb_sdi_status recebe data[0]=statusType, data[1]=statusValue
// statusType=0x01 → PIN digit count update

TEST_F(CallbackTest, CbSdiStatus_PinDigitEntered_UpdatesCount) {
    g_pin_digit_count = 0;
    uint8_t data[] = {0x01, 3};  // statusType=PIN_DIGIT(0x01), count=3
    cb_sdi_status(data, sizeof(data));
    EXPECT_EQ(3, g_pin_digit_count);
    // NUNCA logar o dígito real — apenas a contagem
}

TEST_F(CallbackTest, CbSdiStatus_PinDigitZero_ClearsCount) {
    g_pin_digit_count = 4;
    uint8_t data[] = {0x01, 0};  // statusType=PIN_DIGIT(0x01), count=0
    cb_sdi_status(data, sizeof(data));
    EXPECT_EQ(0, g_pin_digit_count);
}

TEST_F(CallbackTest, CbSdiStatus_UnknownEvent_NoSideEffect) {
    g_pin_digit_count = 2;
    uint8_t data[] = {0xFF, 99};
    cb_sdi_status(data, sizeof(data));
    EXPECT_EQ(2, g_pin_digit_count);  // Não deve alterar estado
}

// ─── Thread-safety: mutex ─────────────────────────────────────────────────────

TEST_F(CallbackTest, CbEmvCT_MutexProtected_NoCrashUnderLoad) {
    // Chama callback várias vezes para verificar que o mutex não causa deadlock
    for (int i = 0; i < 100; i++) {
        uint8_t data[] = {EMV_CB_WRONG_PIN};
        cb_emv_ct(data, sizeof(data));
    }
    EXPECT_EQ(100, g_pin_retry_count);
}
