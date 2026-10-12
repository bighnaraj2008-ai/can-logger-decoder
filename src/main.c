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
    printf("Connecting to CAN interface: %s...\n", interface_name);
    can_bus_t *bus = can_bus_init(interface_name, force_mock, 10);
    if (!bus) {
        fprintf(stderr, "Failed to initialize CAN bus\n");
        dbc_free(dbc_db);
        return 1;
    }
    printf("CAN bus active (Mock: %s)\n", can_bus_is_mock(bus) ? "yes" : "no");
    printf("Initializing CSV log file: %s...\n", csv_file);
    FILE *csv_fp = fopen(csv_file, "w");
    if (!csv_fp) {
        fprintf(stderr, "Cannot open CSV file\n");
        can_bus_close(bus);
        dbc_free(dbc_db);
        return 1;
    }
    fprintf(csv_fp, "Timestamp_ISO,Timestamp_Sec,CAN_ID_Hex,CAN_ID_Dec,Message_Name,DLC,Payload_Hex,Signal_Name,Raw_Value,Physical_Value,Unit\n");
    fflush(csv_fp);
    can_msg_t msg;
    uint64_t total_frames = 0;
    while (max_frames == 0 || total_frames < max_frames) {
        int res = can_bus_read(bus, &msg, 100);
        if (res <= 0) continue;
        total_frames++;
        const dbc_message_t *msg_def = dbc_find_message(dbc_db, msg.can_id);
        if (msg_def) {
            dbc_decoded_message_t decoded;
            dbc_decode_message(msg_def, msg.data, msg.dlc, &decoded);
            printf("[0x%03X] %s (DLC: %u)\n", msg.can_id, decoded.message_name, msg.dlc);
            for (size_t s = 0; s < decoded.signal_count; s++) {
                printf("  -> %s: %.2f %s\n", decoded.signals[s].signal_name, decoded.signals[s].physical_value, decoded.signals[s].unit);
                fprintf(csv_fp, "%s,%.4f,0x%03X,%u,%s,%u,,%s,%ld,%.4f,%s\n",
                        msg.timestamp_iso, msg.timestamp_sec, msg.can_id, msg.can_id,
                        decoded.message_name, msg.dlc, decoded.signals[s].signal_name,
                        (long)decoded.signals[s].raw_value, decoded.signals[s].physical_value, decoded.signals[s].unit);
            }
            fflush(csv_fp);
        }
        }
    }
    fclose(csv_fp);
    can_bus_close(bus);
    dbc_free(dbc_db);
    return 0;
}
