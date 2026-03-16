// tests/test_mapping.cpp
// Testes unitários: mapEmvRetToAbecs, mapTxnTypeAbecsToEmv, mapModalidadeToTec,
//                   pp_tlv_get_byte, pp_tlv_get_bytes, pp_extract_ksn_from_inventory

#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include "ppcomp_internal.h"
#include "pp_sdi_bridge.h"
#include "mocks/sdi_mock.h"

// ─── mapEmvRetToAbecs — cobertura 100% ───────────────────────────────────────

TEST(MappingTest, MapEmvRetToAbecs_OK) {
    EXPECT_EQ(PP_OK,              mapEmvRetToAbecs(EMV_ADK_OK));
}

TEST(MappingTest, MapEmvRetToAbecs_GoOnline) {
    EXPECT_EQ(PP_GO_ONLINE,       mapEmvRetToAbecs(EMV_ADK_GO_ONLINE));
}

TEST(MappingTest, MapEmvRetToAbecs_OfflineApprove) {
    EXPECT_EQ(PP_OFFLINE_APPROVE, mapEmvRetToAbecs(EMV_ADK_OFFLINE_APPROVE));
}

TEST(MappingTest, MapEmvRetToAbecs_Decline) {
    EXPECT_EQ(PP_OFFLINE_DECLINE, mapEmvRetToAbecs(EMV_ADK_DECLINE));
}

TEST(MappingTest, MapEmvRetToAbecs_NoCard) {
    EXPECT_EQ(PP_ERR_NOEVENT,     mapEmvRetToAbecs(EMV_ADK_NO_CARD));
}

TEST(MappingTest, MapEmvRetToAbecs_Fallback) {
    EXPECT_EQ(PP_FALLBACK_CT,     mapEmvRetToAbecs(EMV_ADK_FALLBACK));
}

TEST(MappingTest, MapEmvRetToAbecs_MultiApp) {
    EXPECT_EQ(PP_MULTIAPP,        mapEmvRetToAbecs(EMV_ADK_MULTI_APP));
}

TEST(MappingTest, MapEmvRetToAbecs_TxnAbort) {
    EXPECT_EQ(PP_ABORT,           mapEmvRetToAbecs(EMV_ADK_TXN_ABORT));
}

TEST(MappingTest, MapEmvRetToAbecs_CtlsMobile) {
    EXPECT_EQ(PP_CTLS_WAITING,    mapEmvRetToAbecs(EMV_ADK_TXN_CTLS_MOBILE));
}

TEST(MappingTest, MapEmvRetToAbecs_WrongPIN) {
    EXPECT_EQ(PP_ERR_WRONG_PIN,   mapEmvRetToAbecs(EMV_ADK_WRONG_PIN));
}

TEST(MappingTest, MapEmvRetToAbecs_PINBlocked) {
    EXPECT_EQ(PP_ERR_PIN_BLOCKED, mapEmvRetToAbecs(EMV_ADK_PIN_BLOCKED));
}

TEST(MappingTest, MapEmvRetToAbecs_CardBlocked) {
    EXPECT_EQ(PP_ERR_CARD_BLOCKED, mapEmvRetToAbecs(EMV_ADK_CARD_BLOCKED));
}

TEST(MappingTest, MapEmvRetToAbecs_CommError) {
    // Crítico: EMV_ADK_COMM_ERROR → PP_ERR_PINPAD (-31)
    EXPECT_EQ(PP_ERR_PINPAD, mapEmvRetToAbecs(EMV_ADK_COMM_ERROR));
}

TEST(MappingTest, MapEmvRetToAbecs_Timeout) {
    EXPECT_EQ(PP_ERR_TIMEOUT,     mapEmvRetToAbecs(EMV_ADK_TIMEOUT));
}

TEST(MappingTest, MapEmvRetToAbecs_UserCancel) {
    EXPECT_EQ(PP_ABORT,           mapEmvRetToAbecs(EMV_ADK_USER_CANCEL));
}

TEST(MappingTest, MapEmvRetToAbecs_InternalError) {
    // Crítico: EMV_ADK_INTERNAL_ERROR → PP_ERR_PINPAD (-31)
    EXPECT_EQ(PP_ERR_PINPAD, mapEmvRetToAbecs(EMV_ADK_INTERNAL_ERROR));
}

TEST(MappingTest, MapEmvRetToAbecs_ParamError) {
    EXPECT_EQ(PP_ERR_PARAM, mapEmvRetToAbecs(EMV_ADK_PARAM_ERROR));
}

TEST(MappingTest, MapEmvRetToAbecs_UnknownCode) {
    EXPECT_EQ(PP_ERR_GENERIC, mapEmvRetToAbecs(0xFF));
}

// ─── mapTxnTypeAbecsToEmv ────────────────────────────────────────────────────

TEST(MappingTest, TxnType_Purchase) {
    EXPECT_EQ(static_cast<uint8_t>(EMV_TXN_PURCHASE), mapTxnTypeAbecsToEmv(0x01));
}

TEST(MappingTest, TxnType_Cash) {
    EXPECT_EQ(static_cast<uint8_t>(EMV_TXN_CASH), mapTxnTypeAbecsToEmv(0x02));
}

TEST(MappingTest, TxnType_Cashback) {
    EXPECT_EQ(static_cast<uint8_t>(EMV_TXN_CASHBACK), mapTxnTypeAbecsToEmv(0x09));
}

TEST(MappingTest, TxnType_Unknown_DefaultsToPurchase) {
    EXPECT_EQ(static_cast<uint8_t>(EMV_TXN_PURCHASE), mapTxnTypeAbecsToEmv(0xFF));
}

// ─── mapModalidadeToTec ───────────────────────────────────────────────────────

TEST(MappingTest, Modalidade_CT_Only) {
    EXPECT_EQ(TEC_CHIP_CT, mapModalidadeToTec(0x01));
}

TEST(MappingTest, Modalidade_CTLS_Only) {
    EXPECT_EQ(TEC_CTLS, mapModalidadeToTec(0x02));
}

TEST(MappingTest, Modalidade_MSR_Only) {
    EXPECT_EQ(TEC_MSR, mapModalidadeToTec(0x04));
}

TEST(MappingTest, Modalidade_All) {
    uint8_t tec = mapModalidadeToTec(0x07);
    EXPECT_TRUE(tec & TEC_CHIP_CT);
    EXPECT_TRUE(tec & TEC_CTLS);
    EXPECT_TRUE(tec & TEC_MSR);
}

TEST(MappingTest, Modalidade_None) {
    EXPECT_EQ(TEC_NONE, mapModalidadeToTec(0x00));
}

// ─── pp_tlv_get_byte / pp_tlv_get_bytes ──────────────────────────────────────

TEST(MappingTest, TLV_GetByte_SimpleTag) {
    // TLV: tag=9C(1byte) len=01 value=00
    const uint8_t tlv[] = {0x9C, 0x01, 0x00};
    EXPECT_EQ(0x00, pp_tlv_get_byte(tlv, sizeof(tlv), 0x9C));
}

TEST(MappingTest, TLV_GetByte_NotFound) {
    const uint8_t tlv[] = {0x9C, 0x01, 0x42};
    EXPECT_EQ(0x00, pp_tlv_get_byte(tlv, sizeof(tlv), 0x9F26));
}

TEST(MappingTest, TLV_GetBytes_MultipleTagsFindsCorrect) {
    // TLV: [9C 01 05] [9F36 02 00 01]
    const uint8_t tlv[] = {
        0x9C, 0x01, 0x05,
        0x9F, 0x36, 0x02, 0x00, 0x01
    };
    uint8_t  out[2];
    uint32_t outLen = sizeof(out);
    EXPECT_TRUE(pp_tlv_get_bytes(tlv, sizeof(tlv), 0x9F36, out, &outLen));
    EXPECT_EQ(2u, outLen);
    EXPECT_EQ(0x00, out[0]);
    EXPECT_EQ(0x01, out[1]);
}

TEST(MappingTest, TLV_GetBytes_TagNotFound) {
    const uint8_t tlv[] = {0x9C, 0x01, 0x00};
    uint8_t  out[4];
    uint32_t outLen = sizeof(out);
    EXPECT_FALSE(pp_tlv_get_bytes(tlv, sizeof(tlv), 0xABCD, out, &outLen));
}

TEST(MappingTest, TLV_GetBytes_NullInput) {
    uint8_t  out[4];
    uint32_t outLen = sizeof(out);
    EXPECT_FALSE(pp_tlv_get_bytes(nullptr, 10, 0x9C, out, &outLen));
}

// ─── pp_extract_ksn_from_inventory ───────────────────────────────────────────

TEST(MappingTest, ExtractKSN_NormalInventory) {
    // Inventário de 20 bytes: KSN nos últimos 10
    std::vector<uint8_t> inv(20, 0x00);
    for (int i = 0; i < 10; i++) inv[10 + i] = static_cast<uint8_t>(i + 1);

    uint8_t ksn[10] = {};
    int ksnLen = 10;
    pp_extract_ksn_from_inventory(inv, ksn, &ksnLen);

    EXPECT_EQ(10, ksnLen);
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(static_cast<uint8_t>(i + 1), ksn[i]);
    }
}

TEST(MappingTest, ExtractKSN_ShortInventory) {
    std::vector<uint8_t> inv(5, 0xFF);  // Menor que KSN_LEN
    uint8_t ksn[10] = {};
    int ksnLen = 10;
    pp_extract_ksn_from_inventory(inv, ksn, &ksnLen);
    EXPECT_EQ(0, ksnLen);
}
