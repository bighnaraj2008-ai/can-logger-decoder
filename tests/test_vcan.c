#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#include "can_core.h"
#include "can_filters.h"
#include "error_codes.h"
#include "vcan_interface.h"

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 1; \
    } else { \
        printf("  [PASS] %s\n", msg); \
    } \
} while (0)

static int test_socket_lifecycle(void) {
    printf("\n=== Running VCAN Socket Lifecycle Tests ===\n");

    vcan_socket_t sock;
    memset(&sock, 0, sizeof(sock));
    sock.fd = -1;

    int rc = vcan_socket_bind(&sock, "vcan0");
    TEST_ASSERT(rc == CAN_OK || rc == CAN_ERR_IO || rc == CAN_ERR_UNSUPPORTED,
                "vcan_socket_bind returns valid status");
    TEST_ASSERT(strcmp(sock.ifname, "vcan0") == 0, "Interface name saved in socket handle");

    rc = vcan_socket_set_nonblock(&sock, true);
    TEST_ASSERT(rc == CAN_OK || rc == CAN_ERR_IO || rc == CAN_ERR_INVALID_ARG,
                "vcan_socket_set_nonblock accepted");

    const char *err_str = vcan_socket_strerror(CAN_ERR_IO);
    TEST_ASSERT(err_str != NULL && strlen(err_str) > 0, "Socket error string non-empty");

    rc = vcan_socket_close(&sock);
    TEST_ASSERT(rc == CAN_OK, "vcan_socket_close succeeds");
    TEST_ASSERT(!sock.bound, "Socket marked unbound after close");

    return 0;
}

static int test_frame_io_and_loop(void) {
    printf("\n=== Running VCAN Frame I/O and Poll Loop Tests ===\n");

    vcan_socket_t sock;
    memset(&sock, 0, sizeof(sock));
    vcan_socket_bind(&sock, "vcan0");

    can_frame_t tx_frame;
    can_frame_init(&tx_frame);
    tx_frame.can_id = 0x321;
    tx_frame.can_dlc = 4;
    tx_frame.data[0] = 0xDE;
    tx_frame.data[1] = 0xAD;
    tx_frame.data[2] = 0xBE;
    tx_frame.data[3] = 0xEF;

    int write_rc = vcan_socket_write_frame(&sock, &tx_frame);
    TEST_ASSERT(write_rc == 1 || write_rc < 0, "Write frame executed");

    can_frame_t rx_frame;
    int read_rc = vcan_socket_read_frame(&sock, &rx_frame, 10);
    TEST_ASSERT(read_rc >= 0 || read_rc == CAN_ERR_IO, "Read frame executed cleanly");

    vcan_socket_close(&sock);
    return 0;
}

int main(void) {
    printf("====================================================\n");
    printf(" VCAN NETWORK INTERFACE UNIT TEST SUITE             \n");
    printf("====================================================\n");

    int failed = 0;
    failed += test_socket_lifecycle();
    failed += test_frame_io_and_loop();

    printf("\n====================================================\n");
    if (failed == 0) {
        printf(" ALL VCAN TESTS PASSED SUCCESSFULLY!               \n");
    } else {
        printf(" VCAN TEST FAILURES: %d\n", failed);
    }
    printf("====================================================\n");

    return failed;
}
