#define _GNU_SOURCE
#include "../include/dbc_decoder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char* ltrim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    return s;
}

static void rtrim(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (isspace((unsigned char)s[len - 1]) || s[len - 1] == '\r' || s[len - 1] == '\n')) {
        s[--len] = '\0';
    }
}

static bool parse_message_line(char *line, dbc_message_t *msg) {
    memset(msg, 0, sizeof(*msg));
    char *p = ltrim(line);
    if (strncmp(p, "BO_ ", 4) != 0) return false;
    p += 4;
    p = ltrim(p);

    uint32_t raw_id = 0;
    char name_buf[DBC_MAX_NAME_LEN] = {0};
    unsigned int dlc = 0;
    char transmitter_buf[DBC_MAX_NAME_LEN] = {0};

    char *endptr = NULL;
    raw_id = (uint32_t)strtoul(p, &endptr, 10);
    if (endptr == p) return false;
    p = ltrim(endptr);

    char *colon = strchr(p, ':');
    if (!colon) return false;

    size_t n_len = (size_t)(colon - p);
    while (n_len > 0 && isspace((unsigned char)p[n_len - 1])) n_len--;
    snprintf(name_buf, sizeof(name_buf), "%.*s", (int)n_len, p);

    p = colon + 1;
    p = ltrim(p);
    if (sscanf(p, "%u %63s", &dlc, transmitter_buf) < 1) return false;

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
    if (!fp) return NULL;

    dbc_database_t *db = (dbc_database_t*)calloc(1, sizeof(dbc_database_t));
    if (!db) { fclose(fp); return NULL; }

    snprintf(db->filepath, sizeof(db->filepath), "%s", filepath);
    db->capacity = 16;
    db->message_count = 0;
    db->messages = (dbc_message_t*)calloc(db->capacity, sizeof(dbc_message_t));
    if (!db->messages) { free(db); fclose(fp); return NULL; }

    char line_buffer[1024];
    while (fgets(line_buffer, sizeof(line_buffer), fp)) {
        char *line = ltrim(line_buffer);
        rtrim(line);
        if (strncmp(line, "BO_ ", 4) == 0) {
            if (db->message_count >= db->capacity) {
                size_t new_cap = db->capacity * 2;
                dbc_message_t *new_arr = (dbc_message_t*)realloc(db->messages, new_cap * sizeof(dbc_message_t));
                if (!new_arr) break;
                db->messages = new_arr;
                db->capacity = new_cap;
            }
            dbc_message_t parsed_msg;
            if (parse_message_line(line, &parsed_msg)) {
                db->messages[db->message_count++] = parsed_msg;
            }
        }
    }
    fclose(fp);
    return db;
}
