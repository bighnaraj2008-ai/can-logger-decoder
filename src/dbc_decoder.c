#define _GNU_SOURCE
#include "../include/dbc_decoder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* Helper to strip leading whitespace */
static char* ltrim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    return s;
}

/* Helper to strip trailing whitespace and newline characters */
static void rtrim(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (isspace((unsigned char)s[len - 1]) || s[len - 1] == '\r' || s[len - 1] == '\n')) {
        s[--len] = '\0';
    }
}

/* Parse a single signal definition line:
 * SG_ SignalName [M|m0] : start|len@endian_sign (scale,offset) [min|max] "unit" Receiver
 */
static bool parse_signal_line(char *line, dbc_signal_t *sig) {
    memset(sig, 0, sizeof(*sig));
    sig->scale = 1.0;
    sig->offset = 0.0;

    char *p = ltrim(line);
    if (strncmp(p, "SG_ ", 4) != 0) {
        return false;
    }
    p += 4;
    p = ltrim(p);

    /* Read signal name */
    char *name_start = p;
    while (*p && !isspace((unsigned char)*p) && *p != ':') {
        p++;
    }
    size_t name_len = (size_t)(p - name_start);
    if (name_len == 0 || name_len >= sizeof(sig->name)) {
        return false;
    }
    snprintf(sig->name, sizeof(sig->name), "%.*s", (int)name_len, name_start);

    /* Find colon ':' */
    p = strchr(p, ':');
    if (!p) return false;
    p++; /* Skip ':' */
    p = ltrim(p);

    /* Parse start|length@endianness sign */
    unsigned int start_bit = 0, bit_len = 0, endian_val = 1;
    char sign_char = '+';
    if (sscanf(p, "%u|%u@%1u%c", &start_bit, &bit_len, &endian_val, &sign_char) != 4) {
        return false;
    }

    sig->start_bit = (uint8_t)start_bit;
    sig->bit_len = (uint8_t)bit_len;
    sig->endian = (endian_val == 1) ? CAN_ENDIAN_LITTLE : CAN_ENDIAN_BIG;
    sig->sign = (sign_char == '-') ? CAN_SIGN_SIGNED : CAN_SIGN_UNSIGNED;

    /* Parse scale and offset in parentheses (scale,offset) */
    char *paren = strchr(p, '(');
    if (paren) {
        sscanf(paren, "(%lf,%lf)", &sig->scale, &sig->offset);
    }

    /* Parse [min|max] */
    char *bracket = strchr(p, '[');
    if (bracket) {
        sscanf(bracket, "[%lf|%lf]", &sig->min, &sig->max);
    }

    /* Parse "unit" in quotes */
    char *quote1 = strchr(p, '\"');
    if (quote1) {
        char *quote2 = strchr(quote1 + 1, '\"');
        if (quote2) {
            size_t u_len = (size_t)(quote2 - (quote1 + 1));
            snprintf(sig->unit, sizeof(sig->unit), "%.*s", (int)u_len, quote1 + 1);
            p = quote2 + 1;
            p = ltrim(p);
            /* Parse receiver nodes if present */
            if (*p) {
                snprintf(sig->receiver, sizeof(sig->receiver), "%s", p);
                rtrim(sig->receiver);
            }
        }
    }

    return true;
}

/* Parse a message definition line:
 * BO_ ID MessageName: DLC TransmitterNode
 */
static bool parse_message_line(char *line, dbc_message_t *msg) {
    memset(msg, 0, sizeof(*msg));
    char *p = ltrim(line);
    if (strncmp(p, "BO_ ", 4) != 0) {
        return false;
    }
    p += 4;
    p = ltrim(p);

    uint32_t raw_id = 0;
    char name_buf[DBC_MAX_NAME_LEN] = {0};
    unsigned int dlc = 0;
    char transmitter_buf[DBC_MAX_NAME_LEN] = {0};

    /* Scan ID */
    char *endptr = NULL;
    raw_id = (uint32_t)strtoul(p, &endptr, 10);
    if (endptr == p) return false;
    p = ltrim(endptr);

    /* Scan message name up to ':' */
    char *colon = strchr(p, ':');
    if (!colon) return false;

    size_t n_len = (size_t)(colon - p);
    /* Trim trailing spaces from name before colon */
    while (n_len > 0 && isspace((unsigned char)p[n_len - 1])) {
        n_len--;
    }
    snprintf(name_buf, sizeof(name_buf), "%.*s", (int)n_len, p);

    p = colon + 1;
    p = ltrim(p);

    if (sscanf(p, "%u %63s", &dlc, transmitter_buf) < 1) {
        return false;
    }

    /* Mask out extended CAN bit if set in standard DBC format (bit 31) */
    msg->id = (raw_id & 0x80000000U) ? (raw_id & 0x1FFFFFFFU) : (raw_id & 0x7FFU);
    snprintf(msg->name, sizeof(msg->name), "%s", name_buf);
    msg->dlc = (uint8_t)dlc;
    snprintf(msg->transmitter, sizeof(msg->transmitter), "%s", transmitter_buf);
    msg->signal_count = 0;

    return true;
}

dbc_database_t* dbc_load_file(const char *filepath) {
    if (!filepath) return NULL;

    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        return NULL;
    }

    dbc_database_t *db = (dbc_database_t*)calloc(1, sizeof(dbc_database_t));
    if (!db) {
        fclose(fp);
        return NULL;
    }

    snprintf(db->filepath, sizeof(db->filepath), "%s", filepath);
    db->capacity = 16;
    db->message_count = 0;
    db->messages = (dbc_message_t*)calloc(db->capacity, sizeof(dbc_message_t));
    if (!db->messages) {
        free(db);
        fclose(fp);
        return NULL;
    }

    char line_buffer[1024];
    dbc_message_t *current_msg = NULL;

    while (fgets(line_buffer, sizeof(line_buffer), fp)) {
        char *line = ltrim(line_buffer);
        rtrim(line);

        if (strlen(line) == 0) continue;

        if (strncmp(line, "BO_ ", 4) == 0) {
            /* If capacity full, reallocate */
            if (db->message_count >= db->capacity) {
                size_t new_cap = db->capacity * 2;
                dbc_message_t *new_arr = (dbc_message_t*)realloc(db->messages, new_cap * sizeof(dbc_message_t));
                if (!new_arr) {
                    break;
                }
                db->messages = new_arr;
                db->capacity = new_cap;
            }

            dbc_message_t parsed_msg;
            if (parse_message_line(line, &parsed_msg)) {
                db->messages[db->message_count] = parsed_msg;
                current_msg = &db->messages[db->message_count];
                db->message_count++;
            } else {
                current_msg = NULL;
            }
        } else if (strncmp(line, "SG_ ", 4) == 0 && current_msg != NULL) {
            if (current_msg->signal_count < DBC_MAX_SIGNALS_PER_MSG) {
                dbc_signal_t sig;
                if (parse_signal_line(line, &sig)) {
                    current_msg->signals[current_msg->signal_count++] = sig;
                }
            }
        } else if (strncmp(line, "CM_ BO_ ", 8) == 0) {
            uint32_t cm_id = 0;
            char *p = line + 8;
            cm_id = (uint32_t)strtoul(p, &p, 10);
            char *q1 = strchr(p, '\"');
            if (q1) {
                char *q2 = strchr(q1 + 1, '\"');
                if (q2) {
                    const dbc_message_t *m = dbc_find_message(db, cm_id);
                    if (m) {
                        dbc_message_t *mut_m = (dbc_message_t*)m;
                        size_t c_len = (size_t)(q2 - (q1 + 1));
                        snprintf(mut_m->comment, sizeof(mut_m->comment), "%.*s", (int)c_len, q1 + 1);
                    }
                }
            }
        } else if (strncmp(line, "CM_ SG_ ", 8) == 0) {
            uint32_t cm_id = 0;
            char *p = line + 8;
            cm_id = (uint32_t)strtoul(p, &p, 10);
            p = ltrim(p);
            char sig_name[DBC_MAX_NAME_LEN] = {0};
            if (sscanf(p, "%63s", sig_name) == 1) {
                char *q1 = strchr(p, '\"');
                if (q1) {
                    char *q2 = strchr(q1 + 1, '\"');
                    if (q2) {
                        const dbc_message_t *m = dbc_find_message(db, cm_id);
                        if (m) {
                            dbc_message_t *mut_m = (dbc_message_t*)m;
                            for (size_t i = 0; i < mut_m->signal_count; i++) {
                                if (strcmp(mut_m->signals[i].name, sig_name) == 0) {
                                    size_t c_len = (size_t)(q2 - (q1 + 1));
                                    snprintf(mut_m->signals[i].comment, sizeof(mut_m->signals[i].comment), "%.*s", (int)c_len, q1 + 1);
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    fclose(fp);
    return db;
}

void dbc_free(dbc_database_t *db) {
    if (!db) return;
    if (db->messages) {
        free(db->messages);
        db->messages = NULL;
    }
    free(db);
}

const dbc_message_t* dbc_find_message(const dbc_database_t *db, uint32_t can_id) {
    if (!db) return NULL;

    for (size_t i = 0; i < db->message_count; i++) {
        if (db->messages[i].id == can_id) {
            return &db->messages[i];
        }
    }
    return NULL;
}

double dbc_decode_signal(const dbc_signal_t *sig, const uint8_t *data, uint8_t dlc, int64_t *raw_out) {
    if (!sig || !data) {
        if (raw_out) *raw_out = 0;
        return 0.0;
    }

    uint64_t raw_uint = 0;

    if (sig->endian == CAN_ENDIAN_LITTLE) {
        /* Standard Intel (Little-Endian) Bit Unpacking */
        for (uint8_t i = 0; i < sig->bit_len; i++) {
            uint8_t bit_pos = sig->start_bit + i;
            uint8_t byte_idx = bit_pos / 8;
            uint8_t bit_in_byte = bit_pos % 8;

            if (byte_idx < dlc) {
                uint64_t bit_val = (data[byte_idx] >> bit_in_byte) & 0x01;
                raw_uint |= (bit_val << i);
            }
        }
    } else {
        /* Motorola (Big-Endian) Vector Format Bit Unpacking */
        uint8_t current_bit = sig->start_bit;
        for (uint8_t i = 0; i < sig->bit_len; i++) {
            uint8_t byte_idx = current_bit / 8;
            uint8_t bit_in_byte = current_bit % 8;

            if (byte_idx < dlc) {
                uint64_t bit_val = (data[byte_idx] >> bit_in_byte) & 0x01;
                raw_uint = (raw_uint << 1) | bit_val;
            }

            if (bit_in_byte == 0) {
                current_bit += 15;
            } else {
                current_bit -= 1;
            }
        }
    }

    /* Handle Two's Complement Signed Integers */
    int64_t raw_int;
    if (sig->sign == CAN_SIGN_SIGNED && sig->bit_len > 0 && sig->bit_len < 64) {
        uint64_t sign_bit = ((uint64_t)1) << (sig->bit_len - 1);
        if (raw_uint & sign_bit) {
            uint64_t mask = ~(((uint64_t)1 << sig->bit_len) - 1);
            raw_int = (int64_t)(raw_uint | mask);
        } else {
            raw_int = (int64_t)raw_uint;
        }
    } else {
        raw_int = (int64_t)raw_uint;
    }

    if (raw_out) {
        *raw_out = raw_int;
    }

    /* Scaling and offset formula: physical = raw * scale + offset */
    double physical_value = ((double)raw_int) * sig->scale + sig->offset;
    return physical_value;
}

int dbc_decode_message(const dbc_message_t *msg_def, const uint8_t *payload, uint8_t dlc, dbc_decoded_message_t *result) {
    if (!msg_def || !payload || !result) return 0;

    result->message_id = msg_def->id;
    snprintf(result->message_name, sizeof(result->message_name), "%s", msg_def->name);
    result->dlc = dlc;
    result->signal_count = 0;

    for (size_t i = 0; i < msg_def->signal_count && i < DBC_MAX_SIGNALS_PER_MSG; i++) {
        const dbc_signal_t *sig = &msg_def->signals[i];
        dbc_decoded_signal_t *out_sig = &result->signals[result->signal_count++];

        snprintf(out_sig->signal_name, sizeof(out_sig->signal_name), "%s", sig->name);
        snprintf(out_sig->unit, sizeof(out_sig->unit), "%s", sig->unit);

        out_sig->min = sig->min;
        out_sig->max = sig->max;
        out_sig->has_range = (sig->min != 0.0 || sig->max != 0.0);

        out_sig->physical_value = dbc_decode_signal(sig, payload, dlc, &out_sig->raw_value);

        if (out_sig->has_range) {
            out_sig->in_range = (out_sig->physical_value >= sig->min && out_sig->physical_value <= sig->max);
        } else {
            out_sig->in_range = true;
        }
    }

    return (int)result->signal_count;
}

int dbc_encode_signal(const dbc_signal_t *sig, double physical_value, uint8_t *data, uint8_t dlc) {
    if (!sig || !data) return -1;

    int64_t raw_val = (int64_t)round((physical_value - sig->offset) / sig->scale);

    if (sig->endian == CAN_ENDIAN_LITTLE) {
        uint64_t raw_bits = (uint64_t)raw_val;
        for (uint8_t i = 0; i < sig->bit_len; i++) {
            uint8_t bit_pos = sig->start_bit + i;
            uint8_t byte_idx = bit_pos / 8;
            uint8_t bit_in_byte = bit_pos % 8;

            if (byte_idx < dlc) {
                uint8_t bit_val = (uint8_t)((raw_bits >> i) & 0x01);
                data[byte_idx] &= (uint8_t)(~(1 << bit_in_byte));
                data[byte_idx] |= (uint8_t)(bit_val << bit_in_byte);
            }
        }
        return 0;
    }

    /* Big endian encoding */
    uint64_t raw_bits = (uint64_t)raw_val;
    uint8_t current_bit = sig->start_bit;
    for (uint8_t i = 0; i < sig->bit_len; i++) {
        uint8_t byte_idx = current_bit / 8;
        uint8_t bit_in_byte = current_bit % 8;

        if (byte_idx < dlc) {
            uint8_t bit_val = (uint8_t)((raw_bits >> (sig->bit_len - 1 - i)) & 0x01);
            data[byte_idx] &= (uint8_t)(~(1 << bit_in_byte));
            data[byte_idx] |= (uint8_t)(bit_val << bit_in_byte);
        }

        if (bit_in_byte == 0) {
            current_bit += 15;
        } else {
            current_bit -= 1;
        }
    }

    return 0;
}

void dbc_print_summary(const dbc_database_t *db) {
    if (!db) {
        printf("DBC database is NULL.\n");
        return;
    }

    printf("\n================================================================================\n");
    printf(" CAN DBC Database Summary: %s\n", db->filepath);
    printf(" Total Defined Messages: %zu\n", db->message_count);
    printf("================================================================================\n");

    for (size_t i = 0; i < db->message_count; i++) {
        const dbc_message_t *m = &db->messages[i];
        printf("\n [Message %zu] 0x%03X (%u) : %s  [DLC: %u, Node: %s]\n",
               i + 1, m->id, m->id, m->name, m->dlc, m->transmitter);
        if (strlen(m->comment) > 0) {
            printf("   Note: %s\n", m->comment);
        }
        printf("   %-24s %-10s %-8s %-12s %-12s %-16s %-8s\n",
               "Signal Name", "Start|Len", "Endian", "Scale", "Offset", "Range [Min|Max]", "Unit");
        printf("   ------------------------------------------------------------------------------------\n");

        for (size_t s = 0; s < m->signal_count; s++) {
            const dbc_signal_t *sig = &m->signals[s];
            char bit_spec[16];
            snprintf(bit_spec, sizeof(bit_spec), "%u|%u", sig->start_bit, sig->bit_len);

            char range_spec[20];
            snprintf(range_spec, sizeof(range_spec), "[%.1f|%.1f]", sig->min, sig->max);

            printf("   %-24s %-10s %-8s %-12g %-12g %-16s %-8s\n",
                   sig->name,
                   bit_spec,
                   (sig->endian == CAN_ENDIAN_LITTLE) ? "Intel" : "Motorola",
                   sig->scale,
                   sig->offset,
                   range_spec,
                   sig->unit);
        }
    }
    printf("================================================================================\n\n");
}
