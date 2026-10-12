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
        if (strlen(line) == 0) continue;
    }

    fclose(fp);
    return db;
}
