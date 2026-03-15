// tests/integration/test_homologacao_se.cpp
// Testes de homologação Software Express — 18 TCs ABECS 2.20
//
// Estes testes simulam os 18 casos de teste exigidos pela Software Express para
// certificação da libppcomp no ambiente m-SiTef / clisitef-android.
//
// Cada TC executa o fluxo ABECS completo via mock SDI sem hardware real.
// IMPORTANTE: nenhum PAN, PIN ou KSN real é usado — apenas stubs de teste.

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../mocks/sdi_mock.h"
#include "ppcomp.h"
#include "ppcomp_internal.h"

using ::testing::Return;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::SetArgPointee;
using ::testing::Invoke;

// ─── Fixture base para todos os TCs ──────────────────────────────────────────

class HomologacaoSE : public ::testing::Test {
protected:
    SdiEmvMock   emv;
    SdiClassMock cls;

    char outBuf[1024];
    int  outLen;

    void SetUp() override {
        if (g_initialized) PP_Close(0);

        // TC00: Abrir pinpad e carregar tabelas (pré-condição de todos TCs)
        SdiTestHelper::setupOpenSuccess(emv, cls);
        ASSERT_EQ(PP_OK, PP_Open(0, nullptr));
        SdiTestHelper::setupTablesSuccess(emv);
        loadTables();

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

    void loadTables() {
        EXPECT_CALL(emv, SDI_CT_SetTermData(_)).WillOnce(Return(EMV_ADK_OK));
        EXPECT_CALL(emv, SDI_CTLS_SetTermData(_)).WillOnce(Return(EMV_ADK_OK));
        auto t1 = SdiTestHelper::buildTermDataBuffer();
        PP_SetTermData(0, reinterpret_cast<char*>(t1.data()), static_cast<int>(t1.size()));

        EXPECT_CALL(emv, SDI_CT_SetAppliData(_, _)).WillRepeatedly(Return(EMV_ADK_OK));
        EXPECT_CALL(emv, SDI_CTLS_SetAppliDataSchemeSpecific(_, _, _)).WillRepeatedly(Return(EMV_ADK_OK));
        auto t2 = SdiTestHelper::buildAIDBuffer(3);
        PP_LoadAIDTable(0, reinterpret_cast<char*>(t2.data()), static_cast<int>(t2.size()));

        EXPECT_CALL(emv, SDI_CT_StoreCAPKey(_, _)).WillRepeatedly(Return(EMV_ADK_OK));
        EXPECT_CALL(emv, SDI_CTLS_StoreCAPKey(_, _)).WillRepeatedly(Return(EMV_ADK_OK));
        EXPECT_CALL(emv, SDI_CT_ApplyConfiguration()).WillOnce(Return(EMV_ADK_OK));
        EXPECT_CALL(emv, SDI_CTLS_ApplyConfiguration()).WillOnce(Return(EMV_ADK_OK));
        auto t3 = SdiTestHelper::buildCAPKBuffer(2);
        PP_LoadCAPKTable(0, reinterpret_cast<char*>(t3.data()), static_cast<int>(t3.size()));

        ASSERT_TRUE(g_tables_loaded);
    }

    // Simula detecção de cartão chip CT
    void detectCardCT() {
        EXPECT_CALL(cls, cardDetection_startSelection(_, _, _, _)).WillOnce(Return(EMV_ADK_OK));
        ASSERT_EQ(PP_OK, PP_StartCheckEvent(0, 0x07, 30));

        EXPECT_CALL(cls, cardDetection_pollTechnology())
            .WillOnce(Return(libsdi::Technology::CHIP_CT));
        char evtBuf[256] = {};
        int  evtLen = sizeof(evtBuf);
        ASSERT_EQ(PP_OK, PP_CheckEvent(0, evtBuf, &evtLen));
        ASSERT_EQ(PPState::CARD_CT, g_pp_state);
    }

    // Simula detecção de cartão CTLS
    void detectCardCTLS() {
        EXPECT_CALL(cls, cardDetection_startSelection(_, _, _, _)).WillOnce(Return(EMV_ADK_OK));
        ASSERT_EQ(PP_OK, PP_StartCheckEvent(0, 0x07, 30));

        EXPECT_CALL(cls, cardDetection_pollTechnology())
            .WillOnce(Return(libsdi::Technology::CTLS));
        char evtBuf[256] = {};
        int  evtLen = sizeof(evtBuf);
        ASSERT_EQ(PP_OK, PP_CheckEvent(0, evtBuf, &evtLen));
        ASSERT_EQ(PPState::CARD_CTLS, g_pp_state);
    }

    // Simula detecção de tarja magnética
    void detectCardMSR() {
        EXPECT_CALL(cls, cardDetection_startSelection(_, _, _, _)).WillOnce(Return(EMV_ADK_OK));
        ASSERT_EQ(PP_OK, PP_StartCheckEvent(0, 0x07, 30));

        EXPECT_CALL(cls, cardDetection_pollTechnology())
            .WillOnce(Return(libsdi::Technology::MSR));
        EXPECT_CALL(emv, SDI_fetchTxnTags(_, _, _, _)).WillOnce(Return(EMV_ADK_OK));
        char evtBuf[512] = {};
        int  evtLen = sizeof(evtBuf);
        ASSERT_EQ(PP_OK, PP_CheckEvent(0, evtBuf, &evtLen));
        ASSERT_EQ(PPState::CARD_MSR, g_pp_state);
    }

    // Inicia transação CT e chega ao estado EMV_CT_STARTED
    void startCTTransaction(uint32_t amount = 10000) {
        EMV_CT_STARTRESULT_STRUCT sr = {};
        sr.NumCandidates = 1;
        sr.SelectedAIDLen = 7;
        EXPECT_CALL(emv, SDI_CT_StartTransaction(amount, 0, CURRENCY_BRL_CODE, _, _))
            .WillOnce(DoAll(SetArgPointee<4>(sr), Return(EMV_ADK_OK)));
        ASSERT_EQ(PP_OK, PP_GoOnChip(0, amount, 0, 0, 1, outBuf, &outLen));
        ASSERT_EQ(PPState::EMV_CT_STARTED, g_pp_state);
    }

    // Inicia transação CTLS e chega ao estado EMV_CTLS_SETUP
    void startCTLSTransaction(uint32_t amount = 5000) {
        EXPECT_CALL(emv, SDI_CTLS_SetupTransaction(amount, 0, CURRENCY_BRL_CODE, _, _))
            .WillOnce(Return(EMV_ADK_OK));
        EXPECT_CALL(cls, dialog_requestCard(_, _)).WillOnce(Return(EMV_ADK_OK));
        ASSERT_EQ(PP_CTLS_WAITING, PP_GoOnChipCTLS(0, amount, 0, 0, 1, outBuf, &outLen));
        g_pp_state = PPState::EMV_CTLS_POLLING;
    }

    // Finaliza transação CT normalmente
    void endCTTransaction(int cancelFlag = 0) {
        EXPECT_CALL(emv, SDI_CT_EndTransaction(_)).WillOnce(Return(EMV_ADK_OK));
        EXPECT_CALL(cls, SDI_waitCardRemoval(30)).WillOnce(Return(EMV_ADK_OK));
        g_pp_state    = PPState::EMV_CT_ONLINE;
        g_current_tec = TEC_CHIP_CT;
        PP_EndChip(0, cancelFlag);
    }
};

// ─── TC01: Compra crédito chip — aprovação offline ────────────────────────────

TEST_F(HomologacaoSE, TC01_CompraChip_AprovacaoOffline) {
    // Fase 1: Detecção
    detectCardCT();

    // Fase 2: Início de transação CT
    startCTTransaction(10000);
    outLen = sizeof(outBuf);

    // Fase 3: Processamento offline (kernel decide localmente)
    EMV_CT_TRANSRES_STRUCT tr = {};
    tr.CryptogramType = EMV_CRYPTOGRAM_TC;  // TC = offline approved
    EXPECT_CALL(emv, SDI_CT_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(tr), Return(EMV_ADK_OFFLINE_APPROVE)));

    EXPECT_EQ(PP_OFFLINE_APPROVE, PP_GoOnChipContinue(0, outBuf, &outLen));
    EXPECT_EQ(PPState::EMV_CT_OFFLINE, g_pp_state);
    EXPECT_GT(outLen, 0);  // Buffer deve conter dados para comprovante

    // Fase 4: PIN offline (kernel gerencia — sem chamada PED)
    EXPECT_EQ(PP_OK, PP_GetPIN(0, 4, 6, 30, 1, PIN_VERIFICATION_OFFLINE));

    // Fase 5: Encerramento
    endCTTransaction();
}

// ─── TC02: Compra crédito chip — go online → aprovado pelo autorizador ────────

TEST_F(HomologacaoSE, TC02_CompraChip_GoOnline_Aprovado) {
    detectCardCT();
    startCTTransaction(25000);
    outLen = sizeof(outBuf);

    // Fase 3: Kernel pede autorização online (ARQC)
    EMV_CT_TRANSRES_STRUCT tr = {};
    tr.CryptogramType = EMV_CRYPTOGRAM_ARQC;
    EXPECT_CALL(emv, SDI_CT_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(tr), Return(EMV_ADK_GO_ONLINE)));

    EXPECT_EQ(PP_GO_ONLINE, PP_GoOnChipContinue(0, outBuf, &outLen));
    EXPECT_GT(outLen, 0);

    // Fase 4: PIN online
    EXPECT_CALL(cls, ped_setDefaultTimeout(30000)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_sendPinInputParameters(4, 6, 0)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_startPinInput(1)).WillOnce(Return(EMV_PED_OK));
    ASSERT_EQ(PP_OK, PP_GetPIN(0, 4, 6, 30, 1, PIN_VERIFICATION_ONLINE));
    ASSERT_TRUE(g_pin_entered);

    // Fase 5: Obter PIN block
    SdiTestHelper::setupPinSuccess(cls);
    unsigned char pinBlock[8] = {};
    int           pbLen       = 8;
    unsigned char ksn[10]     = {};
    int           ksnLen      = 10;
    ASSERT_EQ(PP_OK, PP_GetPINBlock(0, pinBlock, &pbLen, ksn, &ksnLen));

    // Fase 6: Autorizador aprova — enviar ARPC ao chip
    outLen = sizeof(outBuf);
    uint8_t arpc[8] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};
    EMV_CT_TRANSRES_STRUCT trFinal = {};
    trFinal.CryptogramType = EMV_CRYPTOGRAM_TC;
    EXPECT_CALL(emv, SDI_CT_ContinueOnline(_, 8, _, _, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(trFinal), Return(EMV_ADK_OFFLINE_APPROVE)));
    EXPECT_CALL(emv, SDI_fetchTxnTags(_, _, _, _)).WillOnce(Return(EMV_ADK_OK));

    EXPECT_EQ(PP_OFFLINE_APPROVE,
              PP_FinishChip(0, reinterpret_cast<char*>(arpc), 8, nullptr, 0, outBuf, &outLen));

    // Fase 7: Encerramento
    endCTTransaction();
}

// ─── TC03: Compra crédito chip — go online → declinado pelo autorizador ───────

TEST_F(HomologacaoSE, TC03_CompraChip_GoOnline_Declinado) {
    detectCardCT();
    startCTTransaction(50000);
    outLen = sizeof(outBuf);

    EMV_CT_TRANSRES_STRUCT tr = {};
    tr.CryptogramType = EMV_CRYPTOGRAM_ARQC;
    EXPECT_CALL(emv, SDI_CT_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(tr), Return(EMV_ADK_GO_ONLINE)));
    ASSERT_EQ(PP_GO_ONLINE, PP_GoOnChipContinue(0, outBuf, &outLen));

    // Autorizador nega
    outLen = sizeof(outBuf);
    uint8_t arpc[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    EMV_CT_TRANSRES_STRUCT trDecl = {};
    trDecl.CryptogramType = EMV_CRYPTOGRAM_AAC;
    EXPECT_CALL(emv, SDI_CT_ContinueOnline(_, 4, _, _, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(trDecl), Return(EMV_ADK_DECLINE)));
    EXPECT_CALL(emv, SDI_fetchTxnTags(_, _, _, _)).WillOnce(Return(EMV_ADK_OK));

    EXPECT_EQ(PP_OFFLINE_DECLINE,
              PP_FinishChip(0, reinterpret_cast<char*>(arpc), 4, nullptr, 0, outBuf, &outLen));

    endCTTransaction(1);
}

// ─── TC04: Compra crédito chip — múltiplas aplicações (MULTI_APP) ─────────────

TEST_F(HomologacaoSE, TC04_CompraChip_MultiApp) {
    detectCardCT();
    outLen = sizeof(outBuf);

    // Cartão tem 2 apps — kernel retorna lista de candidatos
    EMV_CT_STARTRESULT_STRUCT sr = {};
    sr.NumCandidates = 2;
    memcpy(sr.Candidates[0].AppLabel, "VISA CREDIT", 11);
    memcpy(sr.Candidates[1].AppLabel, "MASTER DEBIT", 12);
    EXPECT_CALL(emv, SDI_CT_StartTransaction(_, _, _, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(sr), Return(EMV_ADK_MULTI_APP)));

    EXPECT_EQ(PP_MULTIAPP, PP_GoOnChip(0, 10000, 0, 0, 1, outBuf, &outLen));
    EXPECT_GT(outLen, 0);  // Buffer deve conter a lista serializada

    // Operador seleciona a primeira aplicação
    uint8_t selectedAid[] = {0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10};
    EXPECT_CALL(emv, SDI_CT_SetSelectedApp(_, 7)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_EQ(PP_OK, PP_SetAID(0, selectedAid, sizeof(selectedAid)));

    // Continua com transação normalmente
    outLen = sizeof(outBuf);
    EMV_CT_TRANSRES_STRUCT tr = {};
    tr.CryptogramType = EMV_CRYPTOGRAM_TC;
    EXPECT_CALL(emv, SDI_CT_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(tr), Return(EMV_ADK_OFFLINE_APPROVE)));
    EXPECT_EQ(PP_OFFLINE_APPROVE, PP_GoOnChipContinue(0, outBuf, &outLen));

    endCTTransaction();
}

// ─── TC05: Compra crédito CTLS — aprovação offline ────────────────────────────

TEST_F(HomologacaoSE, TC05_CompraCtls_AprovaçãoOffline) {
    detectCardCTLS();
    startCTLSTransaction(5000);
    outLen = sizeof(outBuf);

    // Cartão aproximado — aprovação offline
    EMV_CTLS_TRANSRES_STRUCT tr = {};
    tr.CryptogramType = EMV_CRYPTOGRAM_TC;
    EXPECT_CALL(emv, SDI_CTLS_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(tr), Return(EMV_ADK_OFFLINE_APPROVE)));

    EXPECT_EQ(PP_OFFLINE_APPROVE, PP_PollCTLS(0, outBuf, &outLen));
    EXPECT_GT(outLen, 0);
}

// ─── TC06: Compra crédito CTLS — go online → aprovado ────────────────────────

TEST_F(HomologacaoSE, TC06_CompraCtls_GoOnline_Aprovado) {
    detectCardCTLS();
    startCTLSTransaction(8000);
    outLen = sizeof(outBuf);

    // Kernel CTLS pede autorização online
    EMV_CTLS_TRANSRES_STRUCT tr = {};
    tr.CryptogramType = EMV_CRYPTOGRAM_ARQC;
    EXPECT_CALL(emv, SDI_CTLS_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(tr), Return(EMV_ADK_GO_ONLINE)));
    ASSERT_EQ(PP_GO_ONLINE, PP_PollCTLS(0, outBuf, &outLen));

    // Autorizador aprova
    outLen = sizeof(outBuf);
    uint8_t arpc[8] = {0x00};
    EMV_CTLS_TRANSRES_STRUCT trApv = {};
    trApv.CryptogramType = EMV_CRYPTOGRAM_TC;
    EXPECT_CALL(emv, SDI_CTLS_ContinueOnline(_, 8, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(trApv), Return(EMV_ADK_OFFLINE_APPROVE)));
    EXPECT_CALL(emv, SDI_fetchTxnTags(_, _, _, _)).WillOnce(Return(EMV_ADK_OK));

    EXPECT_EQ(PP_OFFLINE_APPROVE,
              PP_FinishChipCTLS(0, reinterpret_cast<char*>(arpc), 8, nullptr, 0, outBuf, &outLen));
}

// ─── TC07: Compra débito chip — offline ──────────────────────────────────────

TEST_F(HomologacaoSE, TC07_CompraDebitoChip_Offline) {
    detectCardCT();
    outLen = sizeof(outBuf);

    // Transação de débito (tipo 0x01 no ABECS → EMV_TXN_CASH equivalente)
    EMV_CT_STARTRESULT_STRUCT sr = {};
    sr.NumCandidates = 1;
    EXPECT_CALL(emv, SDI_CT_StartTransaction(15000, 0, CURRENCY_BRL_CODE, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(sr), Return(EMV_ADK_OK)));
    ASSERT_EQ(PP_OK, PP_GoOnChip(0, 15000, 0, 0, 2, outBuf, &outLen));  // tipo=2=débito

    // Aprovação offline
    outLen = sizeof(outBuf);
    EMV_CT_TRANSRES_STRUCT tr = {};
    tr.CryptogramType = EMV_CRYPTOGRAM_TC;
    EXPECT_CALL(emv, SDI_CT_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(tr), Return(EMV_ADK_OFFLINE_APPROVE)));
    EXPECT_EQ(PP_OFFLINE_APPROVE, PP_GoOnChipContinue(0, outBuf, &outLen));

    // PIN offline (obrigatório para débito)
    EXPECT_EQ(PP_OK, PP_GetPIN(0, 4, 6, 30, 1, PIN_VERIFICATION_OFFLINE));

    endCTTransaction();
}

// ─── TC08: Compra débito chip — online ───────────────────────────────────────

TEST_F(HomologacaoSE, TC08_CompraDebitoChip_Online) {
    detectCardCT();
    outLen = sizeof(outBuf);

    EMV_CT_STARTRESULT_STRUCT sr = {};
    sr.NumCandidates = 1;
    EXPECT_CALL(emv, SDI_CT_StartTransaction(20000, 0, CURRENCY_BRL_CODE, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(sr), Return(EMV_ADK_OK)));
    ASSERT_EQ(PP_OK, PP_GoOnChip(0, 20000, 0, 0, 2, outBuf, &outLen));

    outLen = sizeof(outBuf);
    EMV_CT_TRANSRES_STRUCT tr = {};
    tr.CryptogramType = EMV_CRYPTOGRAM_ARQC;
    EXPECT_CALL(emv, SDI_CT_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(tr), Return(EMV_ADK_GO_ONLINE)));
    ASSERT_EQ(PP_GO_ONLINE, PP_GoOnChipContinue(0, outBuf, &outLen));

    // PIN online para débito
    EXPECT_CALL(cls, ped_setDefaultTimeout(_)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_sendPinInputParameters(_, _, _)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_startPinInput(_)).WillOnce(Return(EMV_PED_OK));
    ASSERT_EQ(PP_OK, PP_GetPIN(0, 4, 6, 30, 1, PIN_VERIFICATION_ONLINE));

    SdiTestHelper::setupPinSuccess(cls);
    unsigned char pb[8] = {};
    int pbLen = 8;
    unsigned char ksn[10] = {};
    int ksnLen = 10;
    ASSERT_EQ(PP_OK, PP_GetPINBlock(0, pb, &pbLen, ksn, &ksnLen));

    // Autorização online — aprovado
    outLen = sizeof(outBuf);
    uint8_t arpc[8] = {};
    EMV_CT_TRANSRES_STRUCT trFinal = {};
    trFinal.CryptogramType = EMV_CRYPTOGRAM_TC;
    EXPECT_CALL(emv, SDI_CT_ContinueOnline(_, 8, _, _, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(trFinal), Return(EMV_ADK_OFFLINE_APPROVE)));
    EXPECT_CALL(emv, SDI_fetchTxnTags(_, _, _, _)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_EQ(PP_OFFLINE_APPROVE,
              PP_FinishChip(0, reinterpret_cast<char*>(arpc), 8, nullptr, 0, outBuf, &outLen));

    endCTTransaction();
}

// ─── TC09: PIN online — fluxo completo ───────────────────────────────────────

TEST_F(HomologacaoSE, TC09_PINOnline_FluxoCompleto) {
    detectCardCT();
    startCTTransaction();
    outLen = sizeof(outBuf);

    EMV_CT_TRANSRES_STRUCT tr = {};
    tr.CryptogramType = EMV_CRYPTOGRAM_ARQC;
    EXPECT_CALL(emv, SDI_CT_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(tr), Return(EMV_ADK_GO_ONLINE)));
    ASSERT_EQ(PP_GO_ONLINE, PP_GoOnChipContinue(0, outBuf, &outLen));

    // PED deve ser acionado para PIN online
    EXPECT_CALL(cls, ped_setDefaultTimeout(30000)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_sendPinInputParameters(4, 6, 0)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_startPinInput(1)).WillOnce(Return(EMV_PED_OK));

    EXPECT_EQ(PP_OK, PP_GetPIN(0, 4, 6, 30, 1, PIN_VERIFICATION_ONLINE));
    EXPECT_TRUE(g_pin_entered);
    EXPECT_EQ(PPState::PIN, g_pp_state);

    // PIN block deve ser obtido sem logar conteúdo
    SdiTestHelper::setupPinSuccess(cls);
    unsigned char pb[8] = {};
    int pbLen = 8;
    unsigned char ksn[10] = {};
    int ksnLen = 10;
    EXPECT_EQ(PP_OK, PP_GetPINBlock(0, pb, &pbLen, ksn, &ksnLen));
    EXPECT_EQ(8, pbLen);
    EXPECT_EQ(10, ksnLen);
    // Conteúdo de pb e ksn NUNCA deve ser verificado em testes unitários

    endCTTransaction();
}

// ─── TC10: PIN offline (kernel EMV gerencia) ──────────────────────────────────

TEST_F(HomologacaoSE, TC10_PINOffline_KernelGerencia) {
    detectCardCT();
    startCTTransaction();
    outLen = sizeof(outBuf);

    // ContinueOffline devolve OFFLINE_APPROVE (PIN embutido no fluxo EMV)
    EMV_CT_TRANSRES_STRUCT tr = {};
    tr.CryptogramType = EMV_CRYPTOGRAM_TC;
    EXPECT_CALL(emv, SDI_CT_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(tr), Return(EMV_ADK_OFFLINE_APPROVE)));
    ASSERT_EQ(PP_OFFLINE_APPROVE, PP_GoOnChipContinue(0, outBuf, &outLen));

    // PIN offline: sem chamada ao PED — kernel valida internamente
    EXPECT_CALL(cls, ped_startPinInput(_)).Times(0);  // NÃO deve chamar PED
    EXPECT_EQ(PP_OK, PP_GetPIN(0, 4, 6, 30, 1, PIN_VERIFICATION_OFFLINE));

    endCTTransaction();
}

// ─── TC11: PIN bypass (portador não digita PIN) ───────────────────────────────

TEST_F(HomologacaoSE, TC11_PINBypass) {
    detectCardCT();
    startCTTransaction();
    outLen = sizeof(outBuf);

    EMV_CT_TRANSRES_STRUCT tr = {};
    tr.CryptogramType = EMV_CRYPTOGRAM_ARQC;
    EXPECT_CALL(emv, SDI_CT_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(tr), Return(EMV_ADK_GO_ONLINE)));
    ASSERT_EQ(PP_GO_ONLINE, PP_GoOnChipContinue(0, outBuf, &outLen));

    // PED detecta bypass (cartão com CVM "No CVM" ou portador pressionou verde)
    EXPECT_CALL(cls, ped_setDefaultTimeout(_)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_sendPinInputParameters(_, _, _)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_startPinInput(_)).WillOnce(Return(EMV_PED_BYPASS));

    EXPECT_EQ(PP_PIN_BYPASS, PP_GetPIN(0, 4, 6, 30, 1, PIN_VERIFICATION_ONLINE));
    EXPECT_FALSE(g_pin_entered);  // PIN não foi inserido

    endCTTransaction(1);
}

// ─── TC12: PIN bloqueado após tentativas esgotadas ────────────────────────────

TEST_F(HomologacaoSE, TC12_PINBloqueado) {
    detectCardCT();
    startCTTransaction();
    outLen = sizeof(outBuf);

    EMV_CT_TRANSRES_STRUCT tr = {};
    tr.CryptogramType = EMV_CRYPTOGRAM_ARQC;
    EXPECT_CALL(emv, SDI_CT_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(tr), Return(EMV_ADK_GO_ONLINE)));
    ASSERT_EQ(PP_GO_ONLINE, PP_GoOnChipContinue(0, outBuf, &outLen));

    // Callback SDI sinalizou PIN bloqueado antes de iniciar entrada
    g_pin_blocked = true;

    EXPECT_CALL(cls, ped_setDefaultTimeout(_)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_sendPinInputParameters(_, _, _)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, ped_startPinInput(_)).WillOnce(Return(EMV_PED_ERROR));

    EXPECT_EQ(PP_ERR_PIN_BLOCKED, PP_GetPIN(0, 4, 6, 30, 1, PIN_VERIFICATION_ONLINE));

    // Transação deve ser cancelada
    endCTTransaction(1);
}

// ─── TC13: Fallback CT → tarja (chip com problema → tarja magnética) ─────────

TEST_F(HomologacaoSE, TC13_FallbackChipParaTarja) {
    // Tarja detectada (chip falhou na tentativa anterior — simulado pelo POS)
    detectCardMSR();

    // Após detecção de MSR, g_current_tec = TEC_MSR e dados MSR devem estar no outBuf
    EXPECT_EQ(TEC_MSR, g_current_tec);
    EXPECT_EQ(PPState::CARD_MSR, g_pp_state);
    // Dados MSR foram capturados criptografados pelo SDI (DUKPT) — não verificar conteúdo
}

// ─── TC14: CTLS fallback para chip CT ────────────────────────────────────────

TEST_F(HomologacaoSE, TC14_FallbackCtlsParaChip) {
    detectCardCTLS();
    startCTLSTransaction(3000);
    outLen = sizeof(outBuf);

    // Kernel CTLS não consegue processar — sinaliza fallback
    EXPECT_CALL(emv, SDI_CTLS_ContinueOffline(_, _, _))
        .WillOnce(Return(EMV_ADK_FALLBACK));
    EXPECT_CALL(emv, SDI_CTLS_Break()).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CTLS_EndTransaction(_)).WillOnce(Return(EMV_ADK_OK));

    EXPECT_EQ(PP_FALLBACK_CT, PP_PollCTLS(0, outBuf, &outLen));
    // A camada superior deve reiniciar detecção e oferecer chip CT ao portador
}

// ─── TC15: Leitura de tarja magnética (MSR) ──────────────────────────────────

TEST_F(HomologacaoSE, TC15_TarjaMagnetica_MSR) {
    // Detecção via tarja
    EXPECT_CALL(cls, cardDetection_startSelection(_, _, _, _)).WillOnce(Return(EMV_ADK_OK));
    ASSERT_EQ(PP_OK, PP_StartCheckEvent(0, 0x04, 30));  // Somente MSR

    EXPECT_CALL(cls, cardDetection_pollTechnology())
        .WillOnce(Return(libsdi::Technology::MSR));
    EXPECT_CALL(emv, SDI_fetchTxnTags(_, _, _, _)).WillOnce(Return(EMV_ADK_OK));

    char evtBuf[512] = {};
    int  evtLen = sizeof(evtBuf);
    EXPECT_EQ(PP_OK, PP_CheckEvent(0, evtBuf, &evtLen));

    EXPECT_EQ(PPState::CARD_MSR, g_pp_state);
    EXPECT_EQ(TEC_MSR, g_current_tec);
    // Dados MSR (trilhas 1 e 2 criptografadas + KSN) devem estar em evtBuf
    // Nunca verificar conteúdo decriptado — apenas tamanho e estado
}

// ─── TC16: Remoção de cartão durante transação ───────────────────────────────

TEST_F(HomologacaoSE, TC16_RemocaoCartao_DuranteTransacao) {
    detectCardCT();
    startCTTransaction();
    outLen = sizeof(outBuf);

    // Cartão removido antes de ContinueOffline completar
    g_card_removed_mid_txn = true;

    EMV_CT_TRANSRES_STRUCT tr = {};
    EXPECT_CALL(emv, SDI_CT_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(tr), Return(EMV_ADK_OFFLINE_APPROVE)));

    // Deve retornar PP_ERR_PINPAD pois cartão foi removido
    EXPECT_EQ(PP_ERR_PINPAD, PP_GoOnChipContinue(0, outBuf, &outLen));

    // Cancelar transação após remoção
    EXPECT_CALL(emv, SDI_CT_EndTransaction(EMV_ADK_TXN_ABORT)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(cls, SDI_waitCardRemoval(_)).WillOnce(Return(EMV_ADK_OK));
    g_pp_state    = PPState::EMV_CT_STARTED;
    g_current_tec = TEC_CHIP_CT;
    PP_EndChip(0, 1);
}

// ─── TC17: Carga de tabelas AID + CAPK ────────────────────────────────────────

TEST_F(HomologacaoSE, TC17_CargaDeTabelasAidCapk) {
    // Recarregar tabelas com novos dados (simula atualização de parâmetros)
    g_tables_loaded = false;

    EXPECT_CALL(emv, SDI_CT_SetTermData(_)).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CTLS_SetTermData(_)).WillOnce(Return(EMV_ADK_OK));
    auto t1 = SdiTestHelper::buildTermDataBuffer();
    EXPECT_EQ(PP_OK, PP_SetTermData(0, reinterpret_cast<char*>(t1.data()),
                                     static_cast<int>(t1.size())));

    // 5 AIDs (Visa, Master, Amex, Elo, Hipercard)
    EXPECT_CALL(emv, SDI_CT_SetAppliData(_, _)).Times(5).WillRepeatedly(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CTLS_SetAppliDataSchemeSpecific(_, _, _))
        .WillRepeatedly(Return(EMV_ADK_OK));
    auto t2 = SdiTestHelper::buildAIDBuffer(5);
    EXPECT_EQ(PP_OK, PP_LoadAIDTable(0, reinterpret_cast<char*>(t2.data()),
                                      static_cast<int>(t2.size())));

    // 3 CAPKs — ApplyConfiguration DEVE ser chamado no final
    EXPECT_CALL(emv, SDI_CT_StoreCAPKey(_, _)).Times(3).WillRepeatedly(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CTLS_StoreCAPKey(_, _)).WillRepeatedly(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CT_ApplyConfiguration()).WillOnce(Return(EMV_ADK_OK));
    EXPECT_CALL(emv, SDI_CTLS_ApplyConfiguration()).WillOnce(Return(EMV_ADK_OK));
    auto t3 = SdiTestHelper::buildCAPKBuffer(3);
    EXPECT_EQ(PP_OK, PP_LoadCAPKTable(0, reinterpret_cast<char*>(t3.data()),
                                       static_cast<int>(t3.size())));

    EXPECT_TRUE(g_tables_loaded);
}

// ─── TC18: Segundo toque CTLS (mobile/carteira digital) ──────────────────────

TEST_F(HomologacaoSE, TC18_SegundoToqueMobileCtls) {
    detectCardCTLS();
    startCTLSTransaction(12000);
    outLen = sizeof(outBuf);

    // Primeiro polling: mobile requer segundo toque
    EMV_CTLS_TRANSRES_STRUCT tr = {};
    EXPECT_CALL(emv, SDI_CTLS_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(tr), Return(EMV_ADK_TXN_CTLS_MOBILE)));
    EXPECT_EQ(PP_CTLS_WAITING, PP_PollCTLS(0, outBuf, &outLen));
    EXPECT_EQ(PPState::EMV_CTLS_POLLING, g_pp_state);  // Permanece aguardando

    // Segundo toque: aprovação offline
    outLen = sizeof(outBuf);
    EMV_CTLS_TRANSRES_STRUCT tr2 = {};
    tr2.CryptogramType = EMV_CRYPTOGRAM_TC;
    tr2.IsContactlessMobile = true;
    EXPECT_CALL(emv, SDI_CTLS_ContinueOffline(_, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(tr2), Return(EMV_ADK_OFFLINE_APPROVE)));
    EXPECT_EQ(PP_OFFLINE_APPROVE, PP_PollCTLS(0, outBuf, &outLen));
    EXPECT_GT(outLen, 0);
}
