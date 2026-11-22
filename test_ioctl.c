#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include "edu_ioctl.h"

static void report_ioctl_failure(const char *test, int result, int expected_errno)
{
    int actual_errno = result == -1 ? errno : 0;
    printf("FAIL: %s: expected return=-1, errno=%d; actual return=%d, errno=%d\n",
           test, expected_errno, result, actual_errno);
}

static void report_info_failure(const char *test, const struct edu_info *info)
{
    printf("FAIL: %s: expected version=1, factorial_limit=12, "
           "roundtrip_limit=0, reserved=0; actual version=%u, "
           "factorial_limit=%u, roundtrip_limit=%u, reserved=%u\n",
           test, info->abi_version, info->max_factorial_input,
           info->max_roundtrip_len, info->reserved);
}

static void report_factorial_failure(const char *test, int result,
                                     const struct edu_factorial *request,
                                     unsigned int expected)
{
    int actual_errno = result == -1 ? errno : 0;
    printf("FAIL: %s: input=%u, expected=%u, actual=%u, "
           "ioctl return=%d, errno=%d\n",
           test, request->input, expected, request->result,
           result, actual_errno);
}

int main(){
    int f_open = open("/dev/edu_lab0", O_RDWR);
    
    if (f_open == -1) {
        perror("open");
        return 1;
    }

    bool test_passed = true;

    // -----------------------------------
    // TEST 1: Valid cmd & arg
    // -----------------------------------
    struct edu_info info = {};
    int ioctl_result = ioctl(f_open, EDU_IOCTL_GET_INFO, &info);

    if(ioctl_result == -1) {
        perror("FAIL: Test 1 GET_INFO");
        test_passed = false;
    } else if (
        info.abi_version != 1           
        || info.max_factorial_input != 12
        || info.max_roundtrip_len != 0
        || info.reserved != 0) {

        test_passed = false;
        report_info_failure("Test 1 GET_INFO", &info);
    } else {
        printf("abi_version: %u\n", info.abi_version);
        printf("max_factorial_input: %u\n", info.max_factorial_input);
        printf("max_roundtrip_len: %u\n", info.max_roundtrip_len);
        printf("reserved: %u\n", info.reserved);
    }

    // cleanup
    info = (struct edu_info) {
        .abi_version = 99,
        .max_factorial_input = 99,
        .max_roundtrip_len = 99,
        .reserved = 99
    };

    // -----------------------------------
    // TEST 2: NULL arg
    // -----------------------------------
    ioctl_result = ioctl(f_open, EDU_IOCTL_GET_INFO, NULL);

    if (ioctl_result == -1 && errno == EFAULT) {
        printf("PASS: NULL arg rejected\n");
    } else {
        report_ioctl_failure("Test 2 GET_INFO null pointer", ioctl_result, EFAULT);
        test_passed = false;
    }

    if(ioctl_result == -1) {
        perror("ioctl");
    }


    // -----------------------------------
    // TEST 3: Invalid CMD and NULL arg
    // -----------------------------------
    ioctl_result = ioctl(f_open, 0, NULL);

    if (ioctl_result == -1 && errno == ENOTTY) {
        printf("PASS: invalid cmd and NULL arg rejected\n");
    } else {
        report_ioctl_failure("Test 3 unknown command", ioctl_result, ENOTTY);
        test_passed = false;
    }

    if(ioctl_result == -1) {
        perror("ioctl");
    }

    // -----------------------------------
    // TEST 4: Valid cmd & arg
    // -----------------------------------
    ioctl_result = ioctl(f_open, EDU_IOCTL_GET_INFO, &info);

    if(ioctl_result == -1) {
        perror("FAIL: Test 4 GET_INFO after rejected requests");
        test_passed = false;
    } else if (
        info.abi_version != 1           
        || info.max_factorial_input != 12
        || info.max_roundtrip_len != 0
        || info.reserved != 0) {

        test_passed = false;
        report_info_failure("Test 4 GET_INFO after rejected requests", &info);
    } else {
        printf("abi_version: %u\n", info.abi_version);
        printf("max_factorial_input: %u\n", info.max_factorial_input);
        printf("max_roundtrip_len: %u\n", info.max_roundtrip_len);
        printf("reserved: %u\n", info.reserved);
    }

    // -----------------------------------
    // TEST 5: factorial with null pointer
    // -----------------------------------
    struct edu_factorial factorial = {};
    ioctl_result = ioctl(f_open, EDU_IOCTL_FACTORIAL, NULL);

    if (ioctl_result == -1 && errno == EFAULT) {
        printf("PASS: NULL arg rejected\n");
    } else {
        report_ioctl_failure("Test 5 FACTORIAL null pointer", ioctl_result, EFAULT);
        test_passed = false;
    }

    if(ioctl_result == -1) {
        perror("ioctl");
    }

    // -----------------------------------
    // TEST 6: factorial with invalid input
    // -----------------------------------
    factorial.input = 13;
    ioctl_result = ioctl(f_open, EDU_IOCTL_FACTORIAL, &factorial);

    if (ioctl_result == -1 && errno == EINVAL) {
        printf("PASS: invalid input rejected\n");
    } else {
        report_ioctl_failure("Test 6 FACTORIAL input 13", ioctl_result, EINVAL);
        test_passed = false;
    }

    if(ioctl_result == -1) {
        perror("ioctl");
    }

    // -----------------------------------
    // TEST 7: factorial nonzero flags & valid input
    // -----------------------------------
    factorial.input = 5;
    factorial.flags = 5;
    ioctl_result = ioctl(f_open, EDU_IOCTL_FACTORIAL, &factorial);

    if (ioctl_result == -1 && errno == EINVAL) {
        printf("PASS: valid input and invalid flags rejected\n");
    } else {
        report_ioctl_failure("Test 7 FACTORIAL nonzero flags", ioctl_result, EINVAL);
        test_passed = false;
    }

    if(ioctl_result == -1) {
        perror("ioctl");
    }

    // -----------------------------------
    // TEST 8: factorial nonzero reserved & valid input
    // -----------------------------------
    factorial.input = 5;
    factorial.flags = 0;
    factorial.reserved = 5;
    
    ioctl_result = ioctl(f_open, EDU_IOCTL_FACTORIAL, &factorial);

    if (ioctl_result == -1 && errno == EINVAL) {
        printf("PASS: valid input and invalid reserved rejected\n");
    } else {
        report_ioctl_failure("Test 8 FACTORIAL nonzero reserved", ioctl_result, EINVAL);
        test_passed = false;
    }

    if(ioctl_result == -1) {
        perror("ioctl");
    }

    // // -----------------------------------
    // // TEMPORARY: run with test_suppress_completion=1.
    // // Remove this block and its early return to restore Tests 9-13.
    // // -----------------------------------
    // factorial = (struct edu_factorial){
    //     .input = 5,
    //     .result = 99,
    // };

    // ioctl_result = ioctl(f_open, EDU_IOCTL_FACTORIAL, &factorial);
    // int saved_errno = errno;

    // if (ioctl_result == -1 && saved_errno == ETIMEDOUT) {
    //     printf("PASS: suppressed completion returned ETIMEDOUT\n");
    // } else {
    //     test_passed = false;
    //     printf("FAIL: expected ETIMEDOUT, ioctl return=%d, errno=%d\n",
    //            ioctl_result, ioctl_result == -1 ? saved_errno : 0);
    // }

    // factorial = (struct edu_factorial){
    //     .input = 5,
    //     .result = 99,
    // };

    // ioctl_result = ioctl(f_open, EDU_IOCTL_FACTORIAL, &factorial);
    // saved_errno = errno;

    // if (ioctl_result == -1 && saved_errno == EIO) {
    //     printf("PASS: subsequent request returned EIO\n");
    // } else {
    //     test_passed = false;
    //     printf("FAIL: expected EIO, ioctl return=%d, errno=%d\n",
    //            ioctl_result, ioctl_result == -1 ? saved_errno : 0);
    // }

    // close(f_open);
    // return test_passed ? 0 : 1;

    // -----------------------------------
    // TEST 9: valid factorial 5!
    // -----------------------------------
    factorial.input = 5;
    factorial.flags = 0;
    factorial.reserved = 0;
    factorial.result = 99; // sentinel

    ioctl_result = ioctl(f_open, EDU_IOCTL_FACTORIAL, &factorial);

    if (ioctl_result == 0 && factorial.result == 120) {
        printf("PASS: %u! = %u\n", factorial.input, factorial.result);
    } else {
        test_passed = false;
        report_factorial_failure("Test 9 factorial 5!", ioctl_result, &factorial, 120);
    }
    if(ioctl_result == -1) {
        perror("ioctl");
    }

    // -----------------------------------
    // TEST 10: valid factorial 0!
    // -----------------------------------
    factorial.input = 0;
    factorial.flags = 0;
    factorial.reserved = 0;
    factorial.result = 99; // sentinel

    ioctl_result = ioctl(f_open, EDU_IOCTL_FACTORIAL, &factorial);

    if (ioctl_result == 0 && factorial.result == 1) {
        printf("PASS: %u! = %u\n", factorial.input, factorial.result);
    } else {
        test_passed = false;
        report_factorial_failure("Test 10 factorial 0!", ioctl_result, &factorial, 1);
    }

    if(ioctl_result == -1) {
        perror("ioctl");
    }

    // -----------------------------------
    // TEST 11: valid factorial 1!
    // -----------------------------------
    factorial.input = 1;
    factorial.flags = 0;
    factorial.reserved = 0;
    factorial.result = 99; // sentinel

    ioctl_result = ioctl(f_open, EDU_IOCTL_FACTORIAL, &factorial);

    if (ioctl_result == 0 && factorial.result == 1) {
        printf("PASS: %u! = %u\n", factorial.input, factorial.result);
    } else {
        test_passed = false;
        report_factorial_failure("Test 11 factorial 1!", ioctl_result, &factorial, 1);
    }

    if(ioctl_result == -1) {
        perror("ioctl");
    }

    // -----------------------------------
    // TEST 12: valid factorial 12!
    // -----------------------------------
    factorial.input = 12;
    factorial.flags = 0;
    factorial.reserved = 0;
    factorial.result = 99; // sentinel

    ioctl_result = ioctl(f_open, EDU_IOCTL_FACTORIAL, &factorial);

    if (ioctl_result == 0 && factorial.result == 479001600) {
        printf("PASS: %u! = %u\n", factorial.input, factorial.result);
    } else {
        test_passed = false;
        report_factorial_failure("Test 12 factorial 12!", ioctl_result, &factorial, 479001600);
    }

    if(ioctl_result == -1) {
        perror("ioctl");
    }


    // -----------------------------------
    // TEST 13: loop test 0!, 1!, 5!, 12!
    // -----------------------------------

    struct factorial_case {
        unsigned int input;
        unsigned int expected;
    };

    static const struct factorial_case cases[] = {
        { .input = 0,  .expected = 1 },
        { .input = 1,  .expected = 1 },
        { .input = 5,  .expected = 120 },
        { .input = 12, .expected = 479001600 },
    };

    bool loop_result = true;
    size_t case_count = sizeof(cases) / sizeof(cases[0]); //whole array size / element size: 32/4

    for (int i = 0; i < 100 && loop_result; i++) {
        for (size_t j = 0; j < case_count; j++) {
            factorial = (struct edu_factorial){
                .input = cases[j].input,
                .result = 99,
            };

            ioctl_result = ioctl(f_open, EDU_IOCTL_FACTORIAL, &factorial);

            if (ioctl_result != 0) {
                perror("factorial ioctl");
                printf("FAIL: iteration=%d, input=%u\n",
                        i + 1, cases[j].input);
                loop_result = false;
                break;
            }

            if (factorial.result != cases[j].expected) {
                printf("FAIL: iteration=%d, input=%u, expected=%u, actual=%u\n",
                        i + 1, cases[j].input,
                        cases[j].expected, factorial.result);
                loop_result = false;
                break;
            }
    
        }
    
    }

    if(loop_result) printf("100 Loop test PASS\n");
    else test_passed = false;
    
    // -----------------------------------
    // TEST 13: Roundtrip with NULL userspace pointer
    // -----------------------------------

    ioctl_result = ioctl(f_open, EDU_IOCTL_ROUNDTRIP, NULL);

    if (ioctl_result == -1 && errno == EFAULT) {
        perror("ioctl");
        printf("PASS: Roundtrip test with NULL userspace pointer rejected\n");
    } else {
        report_ioctl_failure("ROUNDTRIP null pointer", ioctl_result, EFAULT);
        test_passed = false;
    }

    // -----------------------------------
    // TEST 14: Roundtrip with length 0 and flag 0
    // -----------------------------------

    struct edu_roundtrip roundtrip_state = {};

    roundtrip_state.length = 0;
    roundtrip_state.flags = 0;

    ioctl_result = ioctl(f_open, EDU_IOCTL_ROUNDTRIP, &roundtrip_state);

    if (ioctl_result == -1 && errno == EINVAL) {
        perror("ioctl");
        printf("PASS: Roundtrip test with length: %u and flags: %u rejected\n", roundtrip_state.length, roundtrip_state.flags);
    } else {
        report_ioctl_failure("ROUNDTRIP length 0, flags 0", ioctl_result, EINVAL);
        test_passed = false;
    }

    // -----------------------------------
    // TEST 15: Roundtrip with length 4097 and flag 0
    // -----------------------------------

    roundtrip_state.length = 4097;
    roundtrip_state.flags = 0;

    ioctl_result = ioctl(f_open, EDU_IOCTL_ROUNDTRIP, &roundtrip_state);

    if (ioctl_result == -1 && errno == EINVAL) {
        perror("ioctl");
        printf("PASS: Roundtrip test with length: %u and flags: %u rejected\n", roundtrip_state.length, roundtrip_state.flags);
    } else {
        report_ioctl_failure("ROUNDTRIP length 4097, flags 0", ioctl_result, EINVAL);
        test_passed = false;
    }

    // -----------------------------------
    // TEST 16: Roundtrip with length 1 and flag 5
    // -----------------------------------

    roundtrip_state.length = 1;
    roundtrip_state.flags = 5;

    ioctl_result = ioctl(f_open, EDU_IOCTL_ROUNDTRIP, &roundtrip_state);

    if (ioctl_result == -1 && errno == EINVAL) {
        perror("ioctl");
        printf("PASS: Roundtrip test with length: %u and flags: %u rejected\n", roundtrip_state.length, roundtrip_state.flags);
    } else {
        report_ioctl_failure("ROUNDTRIP length 1, flags 5", ioctl_result, EINVAL);
        test_passed = false;
    }

    // -----------------------------------
    // TEST 17: Roundtrip with length 1 and flag 0
    // -----------------------------------

    roundtrip_state.length = 1;
    roundtrip_state.flags = 0;

    ioctl_result = ioctl(f_open, EDU_IOCTL_ROUNDTRIP, &roundtrip_state);

    if (ioctl_result == -1 && errno == EOPNOTSUPP) {
        perror("ioctl");
        printf("PASS: Roundtrip test with length: %u and flags: %u rejected, not supported\n", roundtrip_state.length, roundtrip_state.flags);
    } else {
        report_ioctl_failure("ROUNDTRIP length 1, flags 0", ioctl_result, EOPNOTSUPP);
        test_passed = false;
    }

    // -----------------------------------
    // TEST 18: Roundtrip with length 4096 and flag 0
    // -----------------------------------

    roundtrip_state.length = 4096;
    roundtrip_state.flags = 0;

    ioctl_result = ioctl(f_open, EDU_IOCTL_ROUNDTRIP, &roundtrip_state);

    if (ioctl_result == -1 && errno == EOPNOTSUPP) {
        perror("ioctl");
        printf("PASS: Roundtrip test with length: %u and flags: %u rejected, not supported\n", roundtrip_state.length, roundtrip_state.flags);
    } else {
        report_ioctl_failure("ROUNDTRIP length 4096, flags 0", ioctl_result, EOPNOTSUPP);
        test_passed = false;
    }

    close(f_open);
    

    if (test_passed) return 0;

    return 1;
}
