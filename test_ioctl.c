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
        || info.max_factorial_input != 0
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
        || info.max_factorial_input != 0
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

    close(f_open);

    if (test_passed) return 0;

    return 1;
}