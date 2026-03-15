// tests/test_tables.cpp
// Testes unitários: PP_SetTermData, PP_LoadAIDTable, PP_LoadCAPKTable,
//                   table_parse_*, table_get_kernel_id

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "mocks/sdi_mock.h"
#include "ppcomp.h"
#include "ppcomp_internal.h"
#include "tables/table_loader.h"

using ::testing::Return;
using ::testing::_;
using ::testing::AnyNumber;

class TablesTest : public ::testing::Test {
protected:
    SdiEmvMock   emv;
    SdiClassMock cls;

    void SetUp() override {
        if (g_initialized) PP_Close(0);
        SdiTestHelper::setupOpenSuccess(emv, cls);
        ASSERT_EQ(PP_OK, PP_Open(0, nullptr));
    }

    void TearDown() override {
        EXPECT_CALL(emv, SDI_CT_Exit_Framework()).WillRepeatedly(Return(EMV_ADK_OK));
        EXPECT_CALL(emv, SDI_CTLS_Exit_Framework()).WillRepeatedly(Return(EMV_ADK_OK));
        EXPECT_CALL(emv, SDI_ProtocolExit()).WillRepeatedly(Return(EMV_ADK_OK));
        EXPECT_CALL(cls, SDI_waitCardRemoval(_)).WillRepeatedly(Return(EMV_ADK_OK));
        PP_Close(0);
    }
};

// ─── PP_SetTermData ───────────────────────────────────────────────────────────

TEST_F(TablesTest, SetTermData_Success) {
    auto buf = SdiTestHelper::buildTermDataBuffer();
    EXPECT_CALL(emv, SDI_CT_SetTermData(_)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CTLS_SetTermData(_)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_EQ(PP_OK, PP_SetTermData(0, reinterpret_cast<char*>(buf.data()),
                                    static_cast<int>(buf.size())));
}

TEST_F(TablesTest, SetTermData_NullBuffer) {
    EXPECT_EQ(PP_ERR_PARAM, PP_SetTermData(0, nullptr, 0));
}

TEST_F(TablesTest, SetTermData_SDIFails) {
    auto buf = SdiTestHelper::buildTermDataBuffer();
    EXPECT_CALL(emv, SDI_CT_SetTermData(_)).WillOnce(Return(EMV_ADK_PARAM_ERROR));
    EXPECT_EQ(PP_ERR_PARAM, PP_SetTermData(0, reinterpret_cast<char*>(buf.data()),
                                           static_cast<int>(buf.size())));
}

TEST_F(TablesTest, SetTermData_NotInitialized) {
    PP_Close(0);
    // TearDown usa EXPECT_CALL com WillRepeatedly, então PP_Close duplo é OK
    auto buf = SdiTestHelper::buildTermDataBuffer();
    EXPECT_EQ(PP_ERR_INIT, PP_SetTermData(0, reinterpret_cast<char*>(buf.data()),
                                          static_cast<int>(buf.size())));
    // Re-open para TearDown
    SdiTestHelper::setupOpenSuccess(emv, cls);
    PP_Open(0, nullptr);
}

// ─── PP_LoadAIDTable ──────────────────────────────────────────────────────────

TEST_F(TablesTest, LoadAIDTable_OneAID) {
    auto buf = SdiTestHelper::buildAIDBuffer(1);
    EXPECT_CALL(emv, SDI_CT_SetAppliData(_, _)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CTLS_SetAppliDataSchemeSpecific(_, _, _))
        .WillRepeatedly(Return(EMV_ADK_OK));
    EXPECT_EQ(PP_OK, PP_LoadAIDTable(0, reinterpret_cast<char*>(buf.data()),
                                     static_cast<int>(buf.size())));
}

TEST_F(TablesTest, LoadAIDTable_TwentyAIDs) {
    auto buf = SdiTestHelper::buildAIDBuffer(20);
    EXPECT_CALL(emv, SDI_CT_SetAppliData(_, _)).Times(20).WillRepeatedly(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CTLS_SetAppliDataSchemeSpecific(_, _, _))
        .WillRepeatedly(Return(EMV_ADK_OK));
    EXPECT_EQ(PP_OK, PP_LoadAIDTable(0, reinterpret_cast<char*>(buf.data()),
                                     static_cast<int>(buf.size())));
}

TEST_F(TablesTest, LoadAIDTable_NullBuffer) {
    EXPECT_EQ(PP_ERR_PARAM, PP_LoadAIDTable(0, nullptr, 0));
}

TEST_F(TablesTest, LoadAIDTable_TablesMustBeLoaded_BeforeGoOnChip) {
    // tabelas NÃO carregadas — PP_GoOnChip deve retornar PP_ERR_STATE
    g_current_tec = TEC_CHIP_CT;
    g_pp_state    = PPState::CARD_CT;
    char out[256];
    int  outLen = sizeof(out);
    EXPECT_EQ(PP_ERR_STATE, PP_GoOnChip(0, 10000, 0, 0, 1, out, &outLen));
}

// ─── PP_LoadCAPKTable ─────────────────────────────────────────────────────────

TEST_F(TablesTest, LoadCAPKTable_OneCAPK) {
    auto buf = SdiTestHelper::buildCAPKBuffer(1);
    EXPECT_CALL(emv, SDI_CT_StoreCAPKey(_, _)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CTLS_StoreCAPKey(_, _)).WillRepeatedly(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CT_ApplyConfiguration()).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CTLS_ApplyConfiguration()).WillOnce(Return(EMV_ADK_OK));
    EXPECT_EQ(PP_OK, PP_LoadCAPKTable(0, reinterpret_cast<char*>(buf.data()),
                                      static_cast<int>(buf.size())));
    EXPECT_TRUE(g_tables_loaded);
}

TEST_F(TablesTest, LoadCAPKTable_ApplyConfigurationMustBeCalled) {
    auto buf = SdiTestHelper::buildCAPKBuffer(1);
    EXPECT_CALL(emv, SDI_CT_StoreCAPKey(_, _)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CTLS_StoreCAPKey(_, _)).WillRepeatedly(Return(EMV_ADK_OK));
    // Simulamos ApplyConfiguration falhando
    EXPECT_CALL(emv, SDI_CT_ApplyConfiguration()).WillOnce(Return(EMV_ADK_COMM_ERROR));
    EXPECT_EQ(PP_ERR_PINPAD, PP_LoadCAPKTable(0, reinterpret_cast<char*>(buf.data()),
                                               static_cast<int>(buf.size())));
    EXPECT_FALSE(g_tables_loaded);  // Não deve setar tables_loaded se Apply falhou
}

// ─── table_parse_* ────────────────────────────────────────────────────────────

TEST(TableLoaderTest, ParseTermData_Minimal) {
    auto buf = SdiTestHelper::buildTermDataBuffer();
    ABECS_TERMDATA_STRUCT td = {};
    int consumed = table_parse_termdata(buf.data(), static_cast<int>(buf.size()), &td);
    EXPECT_GT(consumed, 0);
    EXPECT_EQ(0x22, td.terminal_type);
}

TEST(TableLoaderTest, ParseTermData_NullBuffer) {
    ABECS_TERMDATA_STRUCT td = {};
    EXPECT_EQ(-1, table_parse_termdata(nullptr, 0, &td));
}

TEST(TableLoaderTest, ParseAIDs_OneRecord) {
    auto buf = SdiTestHelper::buildAIDBuffer(1);
    ABECS_AID_RECORD aids[10] = {};
    int count = 0;
    int consumed = table_parse_aids(buf.data(), static_cast<int>(buf.size()), aids, &count);
    EXPECT_GT(consumed, 0);
    EXPECT_EQ(1, count);
    EXPECT_EQ(7, aids[0].aid_len);
    EXPECT_TRUE(aids[0].ctls_supported);
    EXPECT_EQ(3, aids[0].kernel_id);  // Visa = VK = 3
}

TEST(TableLoaderTest, ParseAIDs_DefaultTACsWhenAllZero) {
    // Construir AID com TACs todos zero
    std::vector<uint8_t> buf;
    const uint8_t visaAID[] = {0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10};
    buf.push_back(7);
    buf.insert(buf.end(), visaAID, visaAID + 7);
    buf.push_back(0x01);  // ctls
    buf.push_back(0x03);  // kernel VK
    buf.insert(buf.end(), 15, 0x00);  // TACs zero
    buf.insert(buf.end(), {0x00, 0x00, 0x00, 0x00});  // floor limit
    buf.push_back(0x00);  // ddol len
    buf.push_back(0x00);  // tdol len

    ABECS_AID_RECORD aids[5] = {};
    int count = 0;
    table_parse_aids(buf.data(), static_cast<int>(buf.size()), aids, &count);
    ASSERT_EQ(1, count);

    // TACs devem ter sido substituídos por 0xFF
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(0xFF, aids[0].tac_denial[i]);
        EXPECT_EQ(0xFF, aids[0].tac_online[i]);
        EXPECT_EQ(0xFF, aids[0].tac_default[i]);
    }
}

TEST(TableLoaderTest, ParseCAPKs_OneRecord) {
    auto buf = SdiTestHelper::buildCAPKBuffer(1);
    ABECS_CAPK_RECORD capks[10] = {};
    int count = 0;
    int consumed = table_parse_capks(buf.data(), static_cast<int>(buf.size()), capks, &count);
    EXPECT_GT(consumed, 0);
    EXPECT_EQ(1, count);
    EXPECT_EQ(0x07, capks[0].index);
    EXPECT_EQ(128, capks[0].modulus_len);
    EXPECT_TRUE(capks[0].ctls_supported);
}

// ─── table_get_kernel_id ─────────────────────────────────────────────────────

TEST(TableLoaderTest, KernelID_Mastercard) {
    const uint8_t aid[] = {0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10};
    EXPECT_EQ(2, table_get_kernel_id(aid, sizeof(aid)));
}

TEST(TableLoaderTest, KernelID_Visa) {
    const uint8_t aid[] = {0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10};
    EXPECT_EQ(3, table_get_kernel_id(aid, sizeof(aid)));
}

TEST(TableLoaderTest, KernelID_Elo) {
    const uint8_t aid[] = {0xA0, 0x00, 0x00, 0x06, 0x51, 0x01, 0x01, 0x10};
    EXPECT_EQ(7, table_get_kernel_id(aid, sizeof(aid)));
}

TEST(TableLoaderTest, KernelID_Hipercard_UsesMasterKernel) {
    const uint8_t aid[] = {0xA0, 0x00, 0x00, 0x06, 0x04, 0x00, 0x01};
    EXPECT_EQ(2, table_get_kernel_id(aid, sizeof(aid)));
}

TEST(TableLoaderTest, KernelID_Unknown) {
    const uint8_t aid[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    EXPECT_EQ(0, table_get_kernel_id(aid, sizeof(aid)));
}
