#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include "edu_ioctl.h"

int main(){
    int f_open = open("/dev/edu_lab0", O_RDWR);
    
    if (f_open == -1) {
        perror("open");
        return 1;
    }

    struct edu_info info = {};
    int ioctl_result = ioctl(f_open, EDU_IOCTL_GET_INFO, &info);

    if(ioctl_result == -1) {
        perror("ioctl");
        close(f_open);
        return -1;
    }

    printf("abi_version: %u\n", info.abi_version);
    printf("max_factorial_input: %u\n", info.max_factorial_input);
    printf("max_roundtrip_len: %u\n", info.max_roundtrip_len);
    printf("reserved: %u\n", info.reserved);

    close(f_open);
    return 0;
}