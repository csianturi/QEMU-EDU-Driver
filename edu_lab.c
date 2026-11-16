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
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/jiffies.h>
#include "edu_ioctl.h"


#define EDU_VENDOR_ID 0x1234
#define EDU_DEV_ID 0x11e8
#define EDU_REG_LIVENESS 0x04
#define EDU_REG_ISR 0x24
#define EDU_REG_IRQ_RAISE 0x60
#define EDU_REG_IRQ_ACK 0x64

MODULE_LICENSE("GPL");

enum EDU_state {
    EDU_READY,
    EDU_BUSY,
    EDU_FAILED
};

/*
    -------------------------
    PCI DEVICE DRIVER STRUCT
    -------------------------
*/



struct edu_device {
    void __iomem *bar0;
    struct miscdevice miscdev;
    struct mutex op_lock;
    struct completion op_done;
    int irq;
    enum EDU_state op_state; 
};

static irqreturn_t edu_irq_handler (int irq, void *dev_id) {
    struct edu_device *state = dev_id;

    u32 ISR_read = ioread32(state->bar0 + EDU_REG_ISR);
    pr_info("EDU interrupt status register: 0x%08x\n", ISR_read);
    if (ISR_read == 0) return IRQ_NONE;

    iowrite32(ISR_read, state->bar0 + EDU_REG_IRQ_ACK);

    complete(&state->op_done);

    return IRQ_HANDLED;
}

static const struct pci_device_id id_table[] = {
    {PCI_DEVICE(EDU_VENDOR_ID, EDU_DEV_ID)},
    {}
};

static int edu_open (struct inode *inode, struct file *file) {
    pr_info("edu_lab: device opened\n");
    
    // if a program opens the device, misc_open assigns file->private_data → state->miscdev
    // ioctl will need file->private_data → whole edu_device instead, so switch it back
    // container_of calculates where edu_device is by subtracting the offset of state->miscdev from curr addr
    struct miscdevice *miscdev = file->private_data;
    struct edu_device *state = container_of(miscdev, struct edu_device, miscdev);
    file->private_data = state;
    
    return 0;
}

static int edu_release (struct inode *inode, struct file *file){
    pr_info("edu_lab: device released\n");
    return 0;
}

static long edu_ioctl (struct file *file, unsigned int cmd, unsigned long arg) {
    pr_info("ioctl cmd: %#x\n", cmd);

    struct edu_device *state = file->private_data;
    

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

        mutex_lock(&state->op_lock);

        // liveness check
        u32 read_result = ioread32(state->bar0 + EDU_REG_LIVENESS);
        pr_info("Liveness Check: 0x%08x\n", read_result);

        mutex_unlock(&state->op_lock);

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

    init_completion(&state->op_done);

    mutex_init(&state->op_lock);
    if(state->op_state == EDU_FAILED) {
        mutex_unlock(&state->op_lock);
        return -EIO;
    }

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
    
    // id register checks
    u32 read_result = ioread32(state->bar0);
    pr_info("EDU ID: 0x%08x\n", read_result);

    // liveness register checks
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

    int vec_alloc_result = pci_alloc_irq_vectors(dev, 1, 1, PCI_IRQ_MSI);
    //pr_info("Vector allocation status: %d\n", vec_alloc_result);

    if(vec_alloc_result < 0) {
        pr_info("Vector allocation failed: %d\n", vec_alloc_result);
        pci_iounmap(dev, state->bar0);
        pci_release_region(dev, 0);
        pci_disable_device(dev);
        kfree(state);
        return vec_alloc_result;
    }

    state->irq = pci_irq_vector(dev, 0);

    if(state->irq < 0) {
        pr_info("Linux IRQ vector failed: %d\n", state->irq);
        int irq_state = state->irq;
        pci_free_irq_vectors(dev);
        pci_iounmap(dev, state->bar0);
        pci_release_region(dev, 0);
        pci_disable_device(dev);
        kfree(state);
        return irq_state;
    }

    pr_info("Linux IRQ vector: %d\n", state->irq);

    int irq_reg = request_irq(state->irq, edu_irq_handler, 0, "edu_lab", state);

    if(irq_reg < 0) {
        pr_info("IRQ Registration Failed: %d\n", irq_reg);
        pci_free_irq_vectors(dev);
        pci_set_drvdata(dev, NULL);
        pci_iounmap(dev, state->bar0);
        pci_release_region(dev, 0);
        pci_disable_device(dev);
        kfree(state);
        return irq_reg;
    }

    //trigger an INT
    // this permits EDU to initiate mem transactions, instead of only respond to cpu access
    // CPU writes 0x1 into EDU IRQ register, EDU raises an INT because of this and sends MSI message
    // through the bus, int controller delivers int to CPU, then ISR runs.
    pci_set_master(dev);
    iowrite32(0x1, state->bar0 + EDU_REG_IRQ_RAISE);
    
    // waiting for completion will put the calling user space thread to sleep so CPU can do other work
    // the write tells EDU to raise the interrupt
    // it is not guaranteed that the CPU receives and executes that int immediately
    // this thread may execute waiting before int is started
    unsigned long completion_timeout = wait_for_completion_timeout(&state->op_done, msecs_to_jiffies(1000));
    if(completion_timeout == 0) {
        pr_info("IRQ Completion Timeout\n");
        pci_clear_master(dev);
        free_irq(state->irq, state);
        pci_free_irq_vectors(dev);
        pci_set_drvdata(dev, NULL);
        pci_iounmap(dev, state->bar0);
        pci_release_region(dev, 0);
        pci_disable_device(dev);
        kfree(state);
        return -ETIMEDOUT;
    }

    pr_info("IRQ Completed Succesfully. %lu jiffies remaining\n", completion_timeout);
    
    state->op_state = EDU_READY;

    // preps userspace interface/misc registration
    state->miscdev.minor = MISC_DYNAMIC_MINOR;
    state->miscdev.name = "edu_lab0";
    state->miscdev.fops = &edu_fops;
    state->miscdev.parent = &dev->dev;

    pci_set_drvdata(dev, state);
    int misc_result = misc_register(&state->miscdev);

    if(misc_result != 0) {
        pr_info("Misc Registration Failed: %d\n", misc_result);
        pci_clear_master(dev);
        free_irq(state->irq, state);
        pci_free_irq_vectors(dev);
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
    pci_clear_master(dev);
    free_irq(state->irq, state);
    pci_free_irq_vectors(dev);
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
    return pci_register_driver(&edu_driver);
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

