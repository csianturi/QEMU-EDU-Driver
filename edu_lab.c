// #include <linux/pci.h>


// static int  {
//     struct pci_driver *myDriver = (struct pci_driver*)malloc(sizeof(struct pci_driver));

//     static const struct pci_device_id edu_ids[2] = {
//         PCI_DEVICE(0x1234,0x11e8),
//         { }
//     };

//     free(myDriver);
    
// }

#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include "edu_ioctl.h"

#define EDU_VENDOR_ID 0x1234
#define EDU_DEV_ID 0x11e8
#define EDU_REG_LIVENESS 0x04

MODULE_LICENSE("GPL");

/*
    -------------------------
    PCI DEVICE DRIVER STRUCT
    -------------------------
*/

static const struct pci_device_id id_table[] = {
    {PCI_DEVICE(EDU_VENDOR_ID, EDU_DEV_ID)},
    {}
};

static int edu_open (struct inode *inode, struct file *file) {
    pr_info("edu_lab: device opened\n");
    return 0;
}

static int edu_release (struct inode *inode, struct file *file){
    pr_info("edu_lab: device released\n");
    return 0;
}

static long edu_ioctl (struct file *file, unsigned int cmd, unsigned long arg) {
    pr_info("ioctl cmd: %#x\n", cmd);

    if(cmd == EDU_IOCTL_GET_INFO) {
        pr_info("GET_INFO recognized: %#x\n", cmd);

        struct edu_info response = {
            .abi_version = 1
        };

        // copy_to_user returns the number of bytes not copied
        unsigned long copy_result = copy_to_user((void __user*)arg, &response, sizeof(response));
        if(copy_result != 0) return -EFAULT;

        return 0;
    } else if(cmd == EDU_IOCTL_FACTORIAL) {
        struct edu_factorial factorial = {};

        // copy_from_user returns the number of bytes not copied
        unsigned long copy_result = copy_from_user(&factorial, (void __user*)arg, sizeof(factorial));
        if(copy_result != 0) return -EFAULT;

        if (factorial.input > 12 || factorial.flags != 0 || factorial.reserved != 0) {
            return -EINVAL;
        }

        return -EOPNOTSUPP;

    }

    return -ENOTTY;
}

static const struct file_operations edu_fops = {
    .owner = THIS_MODULE,
    .open = edu_open,
    .release = edu_release,
    .unlocked_ioctl = edu_ioctl
};

struct edu_device {
    void __iomem *bar0;
    struct miscdevice miscdev;
};



/*
    insmod loads edu_lab.ko and initializes a pci_driver struct (holds id table and probe)
    Linux already found EDU device, it then gets matched against the ID table and probe is called
    Probe allocates a seperate state object that holds bar0 and miscdev
        Linux supplied a *dev, pci_set_drvdata binds the state object with dev
    Probe prepares hardware access by enabling EDU, claiming BAR0, and mapping it
        Mapping uses pci_iomap. Bar0 contains the mapping of EDU's MMIO registers
        linux provides virtualized addr to access
    Probe then establishes userspace interfaces by state->miscdev
        This includes the fops struct that holds all fop functions
        makes /dev/edu_lab0 available
    Userpace program will open /dev/edu_lab0 and linux will create a kernel struct file to represent the instance
        Misc subsystem finds registered descriptor using device number
        connects the open file to edu_fops
        sets file->private_date to miscdev descriptor
        calls edu_open
    The purpose of miscdev is to act like an userspace entry with a callback table when programs access it
        edu_device holds the state
        bar0 reaches the hardware
        miscdev lets programs reach the driver
    Userspace calls ioctl
        linux finds the corresponding struct file and calls unlocked_ioctl.
    file gets closed

    | Structure     | What it represents                        | Who creates it           |
    | pci_driver    | Your driver’s PCI callback table          | You define it |
    | pci_dev       | One discovered PCI device—EDU             | Linux |
    | edu_device    | Your driver’s state for that EDU device   | Your probe allocates it |
    | miscdevice    | A userspace doorway named /dev/edu_lab0   | You embed it in edu_device |

    pci_driver:       “EDU appeared or detached”
    file_operations:  “A program opened, closed, or requested something”
*/
static int edu_probe(struct pci_dev *dev, const struct pci_device_id *id) {
    pr_info("edu_lab: probe called\n");

    struct edu_device *state = kzalloc(sizeof(struct edu_device), GFP_KERNEL);

    if (state == NULL) return -ENOMEM;

    // enables dev
    int pci_enable_result = pci_enable_device_mem(dev);
    
    if(pci_enable_result != 0) {
        pr_info("PCI device Enable failed: %d\n", pci_enable_result);
        kfree(state);
        return pci_enable_result;
    }

    pr_info("PCI device Enabled: %d\n", pci_enable_result);

    // claim BAR0
    int pci_region_result = pci_request_region(dev, 0, "edu_lab");
    
    if(pci_region_result != 0) {
        pr_info("PCI BAR0 region request failed: %d\n", pci_region_result);
        pci_disable_device(dev);
        kfree(state);
        return pci_region_result;
    }

    // obtains a kernel I/O address to access MMIO registers
    state->bar0 = pci_iomap(dev, 0, 0);

    if(state->bar0 == NULL) {
        pr_info("PCI BAR0 mapping failed\n");
        pci_release_region(dev, 0);
        pci_disable_device(dev);
        kfree(state);
        return -ENOMEM;
    }
    
    u32 read_result = ioread32(state->bar0);
    pr_info("EDU ID: 0x%08x\n", read_result);

    u32 write_val = 0x55555555;
    iowrite32(write_val, state->bar0 + EDU_REG_LIVENESS);
    read_result = ioread32(state->bar0 + EDU_REG_LIVENESS);
    pr_info("Liveness Check: 0x%08x\n", read_result);

    if(read_result != ~write_val) {
        pr_info("Liveness check failed. Expected value: 0x%08x\n", ~write_val);
        pci_iounmap(dev, state->bar0);
        pci_release_region(dev, 0);
        pci_disable_device(dev);
        kfree(state);
        return -EIO;
    }

    // preps userspace interface
    state->miscdev.minor = MISC_DYNAMIC_MINOR;
    state->miscdev.name = "edu_lab0";
    state->miscdev.fops = &edu_fops;
    state->miscdev.parent = &dev->dev;

    pci_set_drvdata(dev, state);
    int misc_result = misc_register(&state->miscdev);

    if(misc_result != 0) {
        pr_info("Misc Registration Failed: %d\n", misc_result);
        pci_set_drvdata(dev, NULL);
        pci_iounmap(dev, state->bar0);
        pci_release_region(dev, 0);
        pci_disable_device(dev);
        kfree(state);
        return misc_result;
    }
    
    return 0;
}

static void edu_remove(struct pci_dev *dev) {
    pr_info("Driver detached from EDU\n");

    struct edu_device *state = pci_get_drvdata(dev);

    misc_deregister(&state->miscdev);
    pci_set_drvdata(dev, NULL);
    pci_iounmap(dev, state->bar0);
    pci_release_region(dev, 0);
    pci_disable_device(dev);
    kfree(state);
    return;
}


static struct pci_driver edu_driver = {
    .name = "edu_lab",
    .id_table = id_table,
    .probe = edu_probe,
    .remove = edu_remove
};

static int example_start(void) {
    pr_info("----------NEW ITERATION----------\n");
    pr_info("example: module loaded\n");
    pci_register_driver(&edu_driver);
    return 0;
}

static void example_stop(void) {
    pr_info("example: module unloaded\n");
    pci_unregister_driver(&edu_driver);
    pr_info("----------END OF ITERATION----------\n");
}


module_init(example_start);
module_exit(example_stop);



/*
    -------------------------
    FILE OPERATIONS
    -------------------------
*/

