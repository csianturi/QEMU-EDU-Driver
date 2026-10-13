#ifndef EDU_IOCTL_H
#define EDU_IOCTL_H

#include <linux/types.h>
#include <linux/ioctl.h>

struct edu_info {
    __u32 abi_version;
    __u32 max_factorial_input;
    __u32 max_roundtrip_len;
    __u32 reserved;
};

struct edu_factorial {
    __u32 input;
    __u32 result;
    __u32 flags;
    __u32 reserved;
};

#define EDU_IOCTL_GET_INFO _IOR('E', 0, struct edu_info)
#define EDU_IOCTL_FACTORIAL _IOWR('E', 1, struct edu_factorial)



#endif //EDU_IOCTL_H