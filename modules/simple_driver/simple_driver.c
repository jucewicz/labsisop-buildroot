/**
 * @brief   An introductory character driver. This module maps to /dev/simple_driver and
 * comes with a helper C program that can be run in Linux user space to communicate with
 * this LKM.
 *
 * Modified from Derek Molloy (http://www.derekmolloy.ie/)
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#define DEVICE_NAME "simple_driver"
#define CLASS_NAME  "simple_class"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Author Name");
MODULE_DESCRIPTION("A generic Linux char driver.");
MODULE_VERSION("0.2");

static int majorNumber;
static char message[256] = {0};
static short size_of_message;
static int numberOpens = 0;

static struct class *charClass = NULL;
static struct device *charDevice = NULL;


/* Function prototypes */
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);


/* File operations */
static struct file_operations fops =
{
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};


/**
 * Module initialization
 */
static int __init simple_init(void)
{
    trace_printk("Simple Driver: Initializing the LKM\n");

    numberOpens = 0;
    size_of_message = 0;
    memset(message, 0, sizeof(message));

    /* Dynamically allocate a major number */
    majorNumber = register_chrdev(0, DEVICE_NAME, &fops);

    if (majorNumber < 0) {
        trace_printk(
            "Simple Driver: failed to register a major number\n"
        );

        return majorNumber;
    }

    trace_printk(
        "Simple Driver: registered correctly with major number %d\n",
        majorNumber
    );


    /* Register device class */
    charClass = class_create(THIS_MODULE, CLASS_NAME);

    if (IS_ERR(charClass)) {
        unregister_chrdev(majorNumber, DEVICE_NAME);

        trace_printk(
            "Simple Driver: failed to register device class\n"
        );

        return PTR_ERR(charClass);
    }

    trace_printk(
        "Simple Driver: device class registered correctly\n"
    );


    /* Create device */
    charDevice = device_create(
        charClass,
        NULL,
        MKDEV(majorNumber, 0),
        NULL,
        DEVICE_NAME
    );

    if (IS_ERR(charDevice)) {
        class_destroy(charClass);
        unregister_chrdev(majorNumber, DEVICE_NAME);

        trace_printk(
            "Simple Driver: failed to create the device\n"
        );

        return PTR_ERR(charDevice);
    }

    trace_printk(
        "Simple Driver: device class created correctly\n"
    );

    return 0;
}


/**
 * Module cleanup
 */
static void __exit simple_exit(void)
{
    trace_printk(
        "Simple Driver: goodbye from the LKM!\n"
    );

    device_destroy(
        charClass,
        MKDEV(majorNumber, 0)
    );

    class_unregister(charClass);
    class_destroy(charClass);

    unregister_chrdev(
        majorNumber,
        DEVICE_NAME
    );
}


/**
 * Device open
 */
static int dev_open(
    struct inode *inodep,
    struct file *filep
)
{
    numberOpens++;

    trace_printk(
        "Simple Driver: device has been opened %d time(s)\n",
        numberOpens
    );

    return 0;
}


/**
 * Device read
 */
static ssize_t dev_read(
    struct file *filep,
    char *buffer,
    size_t len,
    loff_t *offset
)
{
    size_t bytes_to_read;

    /*
     * If everything has already been read,
     * signal EOF to user space.
     */
    if (*offset >= size_of_message)
        return 0;

    /*
     * Do not send more bytes than the user requested.
     */
    bytes_to_read = size_of_message - *offset;

    if (bytes_to_read > len)
        bytes_to_read = len;


    if (copy_to_user(
            buffer,
            message + *offset,
            bytes_to_read)) {

        trace_printk(
            "Simple Driver: failed to send data to the user\n"
        );

        return -EFAULT;
    }


    *offset += bytes_to_read;

    trace_printk(
        "Simple Driver: sent %zu characters to the user\n",
        bytes_to_read
    );

    return bytes_to_read;
}


/**
 * Device write
 */
static ssize_t dev_write(
    struct file *filep,
    const char *buffer,
    size_t len,
    loff_t *offset
)
{
    int appended;


    /*
     * Reserve at least one byte for '\0'.
     */
    if (len >= sizeof(message)) {

        trace_printk(
            "Simple Driver: too many characters to deal with (%zu)\n",
            len
        );

        return -EINVAL;
    }


    /*
     * buffer belongs to USER SPACE.
     *
     * We must not directly use it with sprintf("%s", buffer).
     * Copy it safely into kernel memory first.
     */
    if (copy_from_user(message, buffer, len)) {

        trace_printk(
            "Simple Driver: failed to copy data from user space\n"
        );

        return -EFAULT;
    }


    /*
     * Convert the received data into a proper C string.
     */
    message[len] = '\0';


    /*
     * Append "(X letters)" without overflowing message[].
     */
    appended = scnprintf(
        message + len,
        sizeof(message) - len,
        "(%zu letters)",
        len
    );


    size_of_message = len + appended;


    trace_printk(
        "Simple Driver: received %zu characters from the user\n",
        len
    );

    return len;
}


/**
 * Device release
 */
static int dev_release(
    struct inode *inodep,
    struct file *filep
)
{
    trace_printk(
        "Simple Driver: device successfully closed\n"
    );

    return 0;
}


/* Register module entry/exit functions */
module_init(simple_init);
module_exit(simple_exit);