#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>

#include "../include/can_parser.h"
#include "../include/dbc_decoder.h"

/* ANSI Color Escape Codes for Terminal UI */
#define ANSI_RESET       "\033[0m"
#define ANSI_BOLD        "\033[1m"
#define ANSI_DIM         "\033[2m"
#define ANSI_RED         "\033[31m"
#define ANSI_GREEN       "\033[32m"
#define ANSI_YELLOW      "\033[33m"
#define ANSI_BLUE        "\033[34m"
#define ANSI_MAGENTA     "\033[35m"
#define ANSI_CYAN        "\033[36m"
#define ANSI_WHITE       "\033[37m"
#define ANSI_BG_BLUE     "\033[44m"
#define ANSI_BG_DARK     "\033[48;5;236m"

static volatile sig_atomic_t g_running = 1;

static void sigint_handler(int signum) {
    (void)signum;
    g_running = 0;
}

static void print_banner(void) {
    printf("%s%s", ANSI_CYAN, ANSI_BOLD);
    printf("================================================================================\n");
    printf("        CAN BUS LOGGER & SIGNAL DECODER - AUTOMOTIVE TELEMETRY SUITE           \n");
    printf("================================================================================\n");
    printf("%s", ANSI_RESET);
}

static void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("Options:\n");
    printf("  -i, --interface <name>  SocketCAN interface (default: 'vcan0')\n");
    printf("  -d, --dbc <path>        Path to Vector DBC database file (default: 'sample.dbc')\n");
    printf("  -o, --output <path>     Path to output CSV log file (default: 'can_log.csv')\n");
    printf("  -m, --mock              Force mock frame generator (simulated automotive stream)\n");
    printf("  -r, --rate <hz>         Mock generator frequency in Hz (default: 10)\n");
    printf("  -c, --count <num>       Exit after receiving <num> frames (0 = run forever, default: 0)\n");
    printf("  -s, --summary           Display DBC database summary and exit\n");
    printf("  -q, --quiet             Quiet mode (suppress live console telemetry tables)\n");
    printf("  -h, --help              Display this help message\n\n");
    printf("Examples:\n");
    printf("  %s -i vcan0 -d sample.dbc\n", prog_name);
    printf("  %s --mock -c 50 -o can_telemetry.csv\n", prog_name);
    printf("  %s -d sample.dbc --summary\n\n", prog_name);
}

int main(int argc, char *argv[]) {
    char interface_name[64] = "vcan0";
    char dbc_file[256] = "sample.dbc";
    char csv_file[256] = "can_log.csv";
    bool force_mock = false;
    int mock_rate_hz = 10;
    uint64_t max_frames = 0;
    bool quiet_mode = false;
    bool show_summary_only = false;

    static struct option long_options[] = {
        {"interface", required_argument, 0, 'i'},
        {"dbc",       required_argument, 0, 'd'},
        {"output",    required_argument, 0, 'o'},
        {"mock",      no_argument,       0, 'm'},
        {"rate",      required_argument, 0, 'r'},
        {"count",     required_argument, 0, 'c'},
        {"summary",   no_argument,       0, 's'},
        {"quiet",     no_argument,       0, 'q'},
        {"help",      no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:d:o:mr:c:sqh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'i':
                snprintf(interface_name, sizeof(interface_name), "%s", optarg);
                break;
            case 'd':
                snprintf(dbc_file, sizeof(dbc_file), "%s", optarg);
                break;
            case 'o':
                snprintf(csv_file, sizeof(csv_file), "%s", optarg);
                break;
            case 'm':
                force_mock = true;
                break;
            case 'r':
                mock_rate_hz = atoi(optarg);
                if (mock_rate_hz <= 0) mock_rate_hz = 10;
                break;
            case 'c':
                max_frames = (uint64_t)strtoull(optarg, NULL, 10);
                break;
            case 's':
                show_summary_only = true;
                break;
            case 'q':
                quiet_mode = true;
                break;
            case 'h':
                print_banner();
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    print_banner();

    /* 1. Load DBC database */
    printf("%s[INFO]%s Loading DBC database: %s%s%s... ", ANSI_BLUE, ANSI_RESET, ANSI_BOLD, dbc_file, ANSI_RESET);
    dbc_database_t *dbc_db = dbc_load_file(dbc_file);
    if (!dbc_db) {
        printf("%sFAILED%s\n", ANSI_RED, ANSI_RESET);
        fprintf(stderr, "%s[ERROR]%s Could not open or parse DBC file '%s'.\n", ANSI_RED, ANSI_RESET, dbc_file);
        return 1;
    }
    printf("%sSUCCESS%s (%zu messages defined)\n", ANSI_GREEN, ANSI_RESET, dbc_db->message_count);

    if (show_summary_only) {
        dbc_print_summary(dbc_db);
        dbc_free(dbc_db);
        return 0;
    }

    /* 2. Open CSV Log file */
    printf("%s[INFO]%s Initializing CSV logger: %s%s%s... ", ANSI_BLUE, ANSI_RESET, ANSI_BOLD, csv_file, ANSI_RESET);
    FILE *csv_fp = fopen(csv_file, "w");
    if (!csv_fp) {
        printf("%sFAILED%s\n", ANSI_RED, ANSI_RESET);
        fprintf(stderr, "%s[ERROR]%s Cannot open output file '%s' for writing.\n", ANSI_RED, ANSI_RESET, csv_file);
        dbc_free(dbc_db);
        return 1;
    }
    /* Write CSV Header */
    fprintf(csv_fp, "Timestamp_ISO,Timestamp_Sec,CAN_ID_Hex,CAN_ID_Dec,Message_Name,DLC,Payload_Hex,Signal_Name,Raw_Value,Physical_Value,Unit\n");
    fflush(csv_fp);
    printf("%sSUCCESS%s\n", ANSI_GREEN, ANSI_RESET);

    /* 3. Initialize CAN bus */
    printf("%s[INFO]%s Initializing CAN interface: %s%s%s... ", ANSI_BLUE, ANSI_RESET, ANSI_BOLD, interface_name, ANSI_RESET);
    can_bus_t *bus = can_bus_init(interface_name, force_mock, mock_rate_hz);
    if (!bus) {
        printf("%sFAILED%s\n", ANSI_RED, ANSI_RESET);
        fprintf(stderr, "%s[ERROR]%s Failed to initialize CAN bus system.\n", ANSI_RED, ANSI_RESET);
        fclose(csv_fp);
        dbc_free(dbc_db);
        return 1;
    }

    if (can_bus_is_mock(bus)) {
        printf("%s[MOCK GENERATOR ACTIVE]%s\n", ANSI_YELLOW, ANSI_RESET);
        printf("       %sNotice: Real SocketCAN not present or mock requested. Generating simulated automotive telemetry at %d Hz.%s\n",
               ANSI_DIM, mock_rate_hz, ANSI_RESET);
    } else {
        printf("%s[SOCKETCAN CONNECTED: %s]%s\n", ANSI_GREEN, can_bus_get_interface(bus), ANSI_RESET);
    }

    printf("\n%sListening for incoming CAN frames... (Press Ctrl+C to stop)%s\n\n", ANSI_CYAN, ANSI_RESET);

    /* Setup interrupt handler */
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    /* Runtime tracking metrics */
    uint64_t total_frames = 0;
    uint64_t total_decoded_signals = 0;
    uint64_t unknown_frames = 0;
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    can_msg_t msg;
    char hex_payload[64];
    dbc_decoded_message_t decoded;

    /* 4. Main real-time acquisition and decoding loop */
    while (g_running) {
        if (max_frames > 0 && total_frames >= max_frames) {
            break;
        }

        int res = can_bus_read(bus, &msg, 100);
        if (res <= 0) {
            if (res < 0 && g_running) {
                /* Transient read error */
                usleep(10000);
            }
            continue;
        }

        total_frames++;
        can_msg_to_hex_string(&msg, hex_payload, sizeof(hex_payload));

        /* Match frame CAN ID against DBC definitions */
        const dbc_message_t *msg_def = dbc_find_message(dbc_db, msg.can_id);

        if (msg_def) {
            int sig_count = dbc_decode_message(msg_def, msg.data, msg.dlc, &decoded);
            total_decoded_signals += (uint64_t)sig_count;

            /* Log each signal to CSV */
            for (size_t i = 0; i < decoded.signal_count; i++) {
                const dbc_decoded_signal_t *sig = &decoded.signals[i];
                fprintf(csv_fp, "%s,%.6f,0x%03X,%u,%s,%u,%s,%s,%ld,%.4f,%s\n",
                        msg.timestamp_iso,
                        msg.timestamp_sec,
                        msg.can_id,
                        msg.can_id,
                        decoded.message_name,
                        msg.dlc,
                        hex_payload,
                        sig->signal_name,
                        (long)sig->raw_value,
                        sig->physical_value,
                        sig->unit);
            }
            fflush(csv_fp);

            /* Console Real-time Telemetry Visualization */
            if (!quiet_mode) {
                printf("%s[%s]%s %s0x%03X%s (%u) %s%-18s%s DLC:%u [%s%s%s]\n",
                       ANSI_DIM, msg.timestamp_iso, ANSI_RESET,
                       ANSI_YELLOW, msg.can_id, ANSI_RESET,
                       msg.can_id,
                       ANSI_BOLD, decoded.message_name, ANSI_RESET,
                       msg.dlc,
                       ANSI_WHITE, hex_payload, ANSI_RESET);

                for (size_t i = 0; i < decoded.signal_count; i++) {
                    const dbc_decoded_signal_t *sig = &decoded.signals[i];
                    bool is_last = (i + 1 == decoded.signal_count);
                    const char *branch = is_last ? "  └── " : "  ├── ";

                    const char *status_str = sig->in_range ? "[OK]" : "[OUT-OF-BOUNDS]";
                    const char *status_color = sig->in_range ? ANSI_GREEN : ANSI_RED;

                    printf("%s%s%-22s%s : %s%10.2f%s %-6s (raw: %-6ld) %s%s%s\n",
                           ANSI_DIM, branch, sig->signal_name, ANSI_RESET,
                           ANSI_CYAN, sig->physical_value, ANSI_RESET,
                           sig->unit,
                           (long)sig->raw_value,
                           status_color, status_str, ANSI_RESET);
                }
                printf("\n");
            }

        } else {
            /* Unrecognized CAN frame not present in DBC */
            unknown_frames++;
            fprintf(csv_fp, "%s,%.6f,0x%03X,%u,UNKNOWN,%u,%s,RAW,0,0,\n",
                    msg.timestamp_iso,
                    msg.timestamp_sec,
                    msg.can_id,
                    msg.can_id,
                    msg.dlc,
                    hex_payload);
            fflush(csv_fp);

            if (!quiet_mode) {
                printf("%s[%s]%s %s0x%03X%s (%u) %s<UNREGISTERED>%s DLC:%u [%s]\n\n",
                       ANSI_DIM, msg.timestamp_iso, ANSI_RESET,
                       ANSI_RED, msg.can_id, ANSI_RESET,
                       msg.can_id,
                       ANSI_DIM, ANSI_RESET,
                       msg.dlc, hex_payload);
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double elapsed_sec = (double)(end_time.tv_sec - start_time.tv_sec) +
                         (double)(end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    if (elapsed_sec <= 0.0) elapsed_sec = 0.001;
    double fps = (double)total_frames / elapsed_sec;

    /* 5. Clean teardown and summary report */
    printf("\n%s================================================================================%s\n", ANSI_CYAN, ANSI_RESET);
    printf("%s                   SESSION ACQUISITION & LOGGING SUMMARY                       %s\n", ANSI_BOLD, ANSI_RESET);
    printf("%s================================================================================%s\n", ANSI_CYAN, ANSI_RESET);
    printf("  Duration                 : %.2f seconds\n", elapsed_sec);
    printf("  Total CAN Frames Read    : %lu\n", (unsigned long)total_frames);
    printf("  Decoded Signal Records   : %lu\n", (unsigned long)total_decoded_signals);
    printf("  Unknown / Raw Frames     : %lu\n", (unsigned long)unknown_frames);
    printf("  Average Throughput       : %.1f frames/sec\n", fps);
    printf("  Logged Telemetry File    : %s%s%s\n", ANSI_GREEN, csv_file, ANSI_RESET);
    printf("================================================================================\n\n");

    /* Release resources */
    if (csv_fp) fclose(csv_fp);
    if (bus) can_bus_close(bus);
    if (dbc_db) dbc_free(dbc_db);

    return 0;
}
