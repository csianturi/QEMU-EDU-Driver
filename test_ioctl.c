#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>

int main(){
    int f_open = open("/dev/edu_lab0", O_RDWR);
    
    if (f_open == -1) {
        perror("open");
        return 1;
    }

    int ioctl_result = ioctl(f_open, 0, 0);

    if(ioctl_result == -1) {
        perror("ioctl");
        close(f_open);
        return -1;
    }

    close(f_open);
    return 0;
}