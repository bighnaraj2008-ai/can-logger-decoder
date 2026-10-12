#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>

#include "../include/can_parser.h"
#include "../include/dbc_decoder.h"

static void print_banner(void) {
    printf("CAN BUS LOGGER & SIGNAL DECODER\n");
}

static void print_usage(const char *prog) {
    printf("Usage: %s [OPTIONS]\n", prog);
    printf("  -i, --interface <name>  CAN interface (default: vcan0)\n");
    printf("  -d, --dbc <path>        DBC file (default: sample.dbc)\n");
    printf("  -o, --output <path>     CSV log output (default: can_log.csv)\n");
    printf("  -m, --mock              Force mock generator\n");
    printf("  -c, --count <num>       Max frames to capture\n");
}

int main(int argc, char *argv[]) {
    char interface_name[64] = "vcan0";
    char dbc_file[256] = "sample.dbc";
    char csv_file[256] = "can_log.csv";
    bool force_mock = false;
    uint64_t max_frames = 0;

    static struct option long_options[] = {
        {"interface", required_argument, 0, 'i'},
        {"dbc",       required_argument, 0, 'd'},
        {"output",    required_argument, 0, 'o'},
        {"mock",      no_argument,       0, 'm'},
        {"count",     required_argument, 0, 'c'},
        {"help",      no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:d:o:mc:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'i': snprintf(interface_name, sizeof(interface_name), "%s", optarg); break;
            case 'd': snprintf(dbc_file, sizeof(dbc_file), "%s", optarg); break;
            case 'o': snprintf(csv_file, sizeof(csv_file), "%s", optarg); break;
            case 'm': force_mock = true; break;
            case 'c': max_frames = (uint64_t)strtoull(optarg, NULL, 10); break;
            case 'h': print_banner(); print_usage(argv[0]); return 0;
            default: return 1;
        }
    }
    print_banner();
    printf("Loading DBC database: %s...\n", dbc_file);
    dbc_database_t *dbc_db = dbc_load_file(dbc_file);
    if (!dbc_db) {
        fprintf(stderr, "Error loading DBC file %s\n", dbc_file);
        return 1;
    }
    printf("Loaded %zu messages from DBC\n", dbc_db->message_count);
    dbc_free(dbc_db);
    return 0;
}
