// tables/table_loader.cpp
// Parser do formato binário ABECS 2.20 para tabelas T1 (TermData), T2 (AIDs), T3 (CAPKs).
//
// Formato binário ABECS:
//   T1: [tipo_conta(1)] [moeda(2)] [pais(2)] [tipo_terminal(1)] [caps(3)]
//       [add_caps(5)] [ctls_floor_limit(4)] [ctls_txn_limit(4)] [ctls_nopin_limit(4)]
//
//   T2 record: [aid_len(1)] [aid(N)] [ctls_supported(1)] [kernel_id(1)]
//              [tac_denial(5)] [tac_online(5)] [tac_default(5)]
//              [floor_limit(4)] [ddol_len(1)] [ddol(N)] [tdol_len(1)] [tdol(N)]
//
//   T3 record: [rid(5)] [index(1)] [alg(1)] [mod_len(1)] [modulus(N)]
//              [exp_len(1)] [exp(N)] [checksum(20)] [ctls_supported(1)]

#include <cstring>
#include "tables/table_loader.h"
#include "ppcomp_internal.h"

// ─── RIDs conhecidos no Brasil ────────────────────────────────────────────────

static const uint8_t RID_MASTERCARD[5]  = {0xA0, 0x00, 0x00, 0x00, 0x04};
static const uint8_t RID_VISA[5]        = {0xA0, 0x00, 0x00, 0x00, 0x03};
static const uint8_t RID_AMEX[5]        = {0xA0, 0x00, 0x00, 0x00, 0x25};
static const uint8_t RID_DISCOVER[5]    = {0xA0, 0x00, 0x00, 0x01, 0x52};
static const uint8_t RID_ELO[5]         = {0xA0, 0x00, 0x00, 0x06, 0x51};
static const uint8_t RID_HIPERCARD[5]   = {0xA0, 0x00, 0x00, 0x06, 0x04};

uint8_t table_get_kernel_id(const uint8_t* aid, uint8_t aidLen) {
    if (!aid || aidLen < 5) return 0;

    if (memcmp(aid, RID_MASTERCARD, 5) == 0) return 2;  // MK
    if (memcmp(aid, RID_VISA,       5) == 0) return 3;  // VK
    if (memcmp(aid, RID_AMEX,       5) == 0) return 4;  // AK
    if (memcmp(aid, RID_DISCOVER,   5) == 0) return 6;  // DK
    if (memcmp(aid, RID_ELO,        5) == 0) return 7;  // EK
    if (memcmp(aid, RID_HIPERCARD,  5) == 0) return 2;  // Hipercard usa kernel MK

    LOG_PP_DEBUG("table_get_kernel_id: RID desconhecido %02X%02X%02X%02X%02X",
                 aid[0], aid[1], aid[2], aid[3], aid[4]);
    return 0;
}

// ─── Helper: lê uint16_t big-endian ──────────────────────────────────────────

static inline uint16_t read_u16_be(const uint8_t* p) {
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}

static inline uint32_t read_u32_be(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) <<  8) |
           (static_cast<uint32_t>(p[3])      );
}

// ─── Parse T1: TermData ───────────────────────────────────────────────────────

int table_parse_termdata(const uint8_t* buf, int bufLen, ABECS_TERMDATA_STRUCT* out) {
    if (!buf || !out || bufLen < 22) {
        LOG_PP_ERROR("table_parse_termdata: buffer insuficiente (%d bytes)", bufLen);
        return -1;
    }

    memset(out, 0, sizeof(ABECS_TERMDATA_STRUCT));

    int pos = 0;

    // Tipo de terminal (tag 9F35) - 1 byte
    out->terminal_type = buf[pos++];

    // Capabilities (tag 9F33) - 3 bytes
    memcpy(out->terminal_capabilities, &buf[pos], 3);
    pos += 3;

    // Additional capabilities (tag 9F40) - 5 bytes
    memcpy(out->add_terminal_cap, &buf[pos], 5);
    pos += 5;

    // CTLS floor limit - 4 bytes
    if (pos + 4 <= bufLen) {
        out->ctls_floor_limit = read_u32_be(&buf[pos]);
        pos += 4;
    }

    // CTLS transaction limit - 4 bytes
    if (pos + 4 <= bufLen) {
        out->ctls_txn_limit = read_u32_be(&buf[pos]);
        pos += 4;
    }

    // CTLS no-PIN limit - 4 bytes
    if (pos + 4 <= bufLen) {
        out->ctls_nopin_limit = read_u32_be(&buf[pos]);
        pos += 4;
    }

    return pos;
}

// ─── Parse T2: AID Table ─────────────────────────────────────────────────────

int table_parse_aids(const uint8_t* buf, int bufLen,
                     ABECS_AID_RECORD* out, int* count) {
    if (!buf || !out || !count || bufLen < 2) {
        LOG_PP_ERROR("table_parse_aids: parâmetros inválidos");
        return -1;
    }

    *count = 0;
    int pos = 0;

    while (pos < bufLen && *count < MAX_AID_RECORDS) {
        if (pos >= bufLen) break;

        ABECS_AID_RECORD& rec = out[*count];
        memset(&rec, 0, sizeof(ABECS_AID_RECORD));

        // AID length (1 byte) + AID (N bytes)
        if (pos >= bufLen) break;
        uint8_t aidLen = buf[pos++];
        if (aidLen == 0 || aidLen > static_cast<uint8_t>(MAX_AID_LEN)) break;
        if (pos + aidLen > bufLen) break;

        rec.aid_len = aidLen;
        memcpy(rec.aid, &buf[pos], aidLen);
        pos += aidLen;

        // CTLS supported (1 byte)
        if (pos >= bufLen) break;
        rec.ctls_supported = (buf[pos++] != 0);

        // Kernel ID (1 byte) - 0=auto-detect por RID
        if (pos >= bufLen) break;
        rec.kernel_id = buf[pos++];
        if (rec.ctls_supported && rec.kernel_id == 0) {
            rec.kernel_id = table_get_kernel_id(rec.aid, rec.aid_len);
        }

        // TAC Denial (5 bytes)
        if (pos + 5 > bufLen) break;
        memcpy(rec.tac_denial, &buf[pos], 5);
        pos += 5;

        // TAC Online (5 bytes)
        if (pos + 5 > bufLen) break;
        memcpy(rec.tac_online, &buf[pos], 5);
        pos += 5;

        // TAC Default (5 bytes)
        if (pos + 5 > bufLen) break;
        memcpy(rec.tac_default, &buf[pos], 5);
        pos += 5;

        // Se TACs são todos zero: usar valores conservadores (0xFF)
        bool tacsAllZero = true;
        for (int i = 0; i < 5; i++) {
            if (rec.tac_denial[i] || rec.tac_online[i] || rec.tac_default[i]) {
                tacsAllZero = false;
                break;
            }
        }
        if (tacsAllZero) {
            memset(rec.tac_denial,  0xFF, 5);
            memset(rec.tac_online,  0xFF, 5);
            memset(rec.tac_default, 0xFF, 5);
            LOG_PP_DEBUG("table_parse_aids: TACs zerados para AID[%d], usando 0xFF", *count);
        }

        // Floor limit CT (4 bytes)
        if (pos + 4 > bufLen) break;
        rec.floor_limit_ct = read_u32_be(&buf[pos]);
        pos += 4;

        // DDOL: [len(1)] [data(N)]
        if (pos >= bufLen) break;
        rec.default_ddol_len = buf[pos++];
        if (rec.default_ddol_len > 0) {
            if (pos + rec.default_ddol_len > bufLen ||
                rec.default_ddol_len > sizeof(rec.default_ddol)) break;
            memcpy(rec.default_ddol, &buf[pos], rec.default_ddol_len);
            pos += rec.default_ddol_len;
        }

        // TDOL: [len(1)] [data(N)]
        if (pos >= bufLen) break;
        rec.default_tdol_len = buf[pos++];
        if (rec.default_tdol_len > 0) {
            if (pos + rec.default_tdol_len > bufLen ||
                rec.default_tdol_len > sizeof(rec.default_tdol)) break;
            memcpy(rec.default_tdol, &buf[pos], rec.default_tdol_len);
            pos += rec.default_tdol_len;
        }

        (*count)++;
    }

    LOG_PP_INFO("table_parse_aids: %d AIDs lidos", *count);
    return pos;
}

// ─── Parse T3: CAPK Table ────────────────────────────────────────────────────

int table_parse_capks(const uint8_t* buf, int bufLen,
                      ABECS_CAPK_RECORD* out, int* count) {
    if (!buf || !out || !count || bufLen < 2) {
        LOG_PP_ERROR("table_parse_capks: parâmetros inválidos");
        return -1;
    }

    *count = 0;
    int pos = 0;

    while (pos < bufLen && *count < MAX_CAPK_RECORDS) {
        ABECS_CAPK_RECORD& rec = out[*count];
        memset(&rec, 0, sizeof(ABECS_CAPK_RECORD));

        // RID (5 bytes)
        if (pos + 5 > bufLen) break;
        memcpy(rec.rid, &buf[pos], 5);
        pos += 5;

        // Key index (1 byte)
        if (pos >= bufLen) break;
        rec.index = buf[pos++];

        // Algorithm (1 byte) - 0x01=RSA
        if (pos >= bufLen) break;
        rec.algorithm = buf[pos++];

        // Modulus: [len(1)] [data(N)]
        if (pos >= bufLen) break;
        rec.modulus_len = buf[pos++];
        if (rec.modulus_len == 0 || rec.modulus_len > MAX_CAPK_MODULUS_LEN) break;
        if (pos + rec.modulus_len > bufLen) break;
        memcpy(rec.modulus, &buf[pos], rec.modulus_len);
        pos += rec.modulus_len;

        // Exponent: [len(1)] [data(N)]
        if (pos >= bufLen) break;
        rec.exponent_len = buf[pos++];
        if (rec.exponent_len == 0 || rec.exponent_len > MAX_CAPK_EXP_LEN) break;
        if (pos + rec.exponent_len > bufLen) break;
        memcpy(rec.exponent, &buf[pos], rec.exponent_len);
        pos += rec.exponent_len;

        // Checksum SHA-1 (20 bytes)
        if (pos + 20 > bufLen) break;
        memcpy(rec.checksum, &buf[pos], 20);
        pos += 20;

        // CTLS supported (1 byte)
        if (pos >= bufLen) break;
        rec.ctls_supported = (buf[pos++] != 0);

        (*count)++;
    }

    LOG_PP_INFO("table_parse_capks: %d CAPKs lidos", *count);
    return pos;
}
