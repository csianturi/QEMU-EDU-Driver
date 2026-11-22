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

#define DMA_MAX_PAYLOAD 4096

struct edu_roundtrip {
    __u32 length;
    __u32 flags;
    __u8 payload[DMA_MAX_PAYLOAD];
};

#define EDU_IOCTL_GET_INFO _IOR('E', 0, struct edu_info)
#define EDU_IOCTL_FACTORIAL _IOWR('E', 1, struct edu_factorial)
#define EDU_IOCTL_ROUNDTRIP _IOWR('E', 2, struct edu_roundtrip)




#endif //EDU_IOCTL_H