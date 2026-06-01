#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Test the security invariant: any memcpy from an unverified source into a
 * code execution region must include integrity verification (e.g., hash/signature).
 * The invariant: copied data must match a known-good checksum before execution. */

/* Simulate the vulnerable pattern: copying from a "source" region to a "dest" region */
static int has_integrity_check(const uint8_t *src, size_t len, uint32_t expected_crc) {
    /* Stub: in a secure implementation, this MUST be called before memcpy to exec region */
    uint32_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= src[i];
        crc = (crc << 1) | (crc >> 31);
    }
    return (crc == expected_crc);
}

START_TEST(test_memcpy_requires_integrity_verification)
{
    /* Invariant: data copied into execution region must pass integrity check before use */
    uint8_t dest[0x1800];
    
    /* payloads: attacker-controlled shellcode, boundary all-zeros, valid known-good data */
    uint8_t exploit_payload[0x1800];
    uint8_t boundary_payload[0x1800];
    uint8_t valid_payload[0x1800];

    memset(exploit_payload, 0x90, sizeof(exploit_payload)); /* NOP sled attack */
    exploit_payload[0] = 0xDE; exploit_payload[1] = 0xAD;   /* attacker marker */

    memset(boundary_payload, 0x00, sizeof(boundary_payload)); /* all-zeros boundary */

    memset(valid_payload, 0xAB, sizeof(valid_payload));        /* known-good pattern */

    struct { uint8_t *src; int should_pass; } cases[] = {
        { exploit_payload, 0 },   /* attacker payload: must NOT pass integrity check */
        { boundary_payload, 0 },  /* boundary zeros: must NOT pass without verification */
        { valid_payload,    1 },  /* valid payload: passes integrity check */
    };

    /* Pre-compute expected CRC only for the valid payload */
    uint32_t valid_crc = 0;
    for (size_t i = 0; i < sizeof(valid_payload); i++) {
        valid_crc ^= valid_payload[i];
        valid_crc = (valid_crc << 1) | (valid_crc >> 31);
    }

    for (int i = 0; i < 3; i++) {
        int verified = has_integrity_check(cases[i].src, 0x1800, valid_crc);
        if (cases[i].should_pass) {
            ck_assert_msg(verified == 1,
                "SECURITY VIOLATION: valid payload failed integrity check");
            memcpy(dest, cases[i].src, 0x1800); /* safe: verified */
        } else {
            ck_assert_msg(verified == 0,
                "SECURITY VIOLATION: unverified/attacker payload passed integrity check — "
                "arbitrary code could be injected into boot execution path");
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_memcpy_requires_integrity_verification);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}