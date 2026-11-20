#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include "edu_ioctl.h"

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
        perror("ioctl");
        test_passed = false;
    } else if (
        info.abi_version != 1           
        || info.max_factorial_input != 12
        || info.max_roundtrip_len != 0
        || info.reserved != 0) {

        test_passed = false;
        
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
        perror("ioctl");
        test_passed = false;
    } else if (
        info.abi_version != 1           
        || info.max_factorial_input != 12
        || info.max_roundtrip_len != 0
        || info.reserved != 0) {

        test_passed = false;
        
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
        test_passed = false;
    }

    if(ioctl_result == -1) {
        perror("ioctl");
    }

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
        printf("FAIL: %u != %u, expected result: 120, ioctl return: %d", factorial.input, factorial.result, ioctl_result);
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
        printf("FAIL: %u != %u, expected result: 0, ioctl return: %d", factorial.input, factorial.result, ioctl_result);
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
        printf("FAIL: %u != %u, expected result: 1, ioctl return: %d", factorial.input, factorial.result, ioctl_result);
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
        printf("FAIL: %u != %u, expected result: 479001600, ioctl return: %d", factorial.input, factorial.result, ioctl_result);
    }

    if(ioctl_result == -1) {
        perror("ioctl");
    }


    // -----------------------------------
    // TEST 12: loop test 0!, 1!, 5!, 12!
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
    
    close(f_open);
    

    if (test_passed) return 0;

    return 1;
}