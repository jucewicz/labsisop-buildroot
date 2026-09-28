#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/types.h>

#define DEVICE_NAME "xtea_driver"
#define CLASS_NAME  "xtea_class"

#define MAX_DATA_BYTES 256
#define MAX_COMMAND_SIZE 1024
#define MAX_MESSAGE_SIZE (MAX_DATA_BYTES * 2 + 1)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Grupo A");
MODULE_DESCRIPTION("XTEA character device driver");
MODULE_VERSION("1.0");

static int majorNumber;
static char message[MAX_MESSAGE_SIZE] = {0};
static size_t size_of_message;
static int numberOpens = 0;

static struct class *charClass = NULL;
static struct device *charDevice = NULL;

static char *key0 = NULL;
static char *key1 = NULL;
static char *key2 = NULL;
static char *key3 = NULL;

module_param(key0, charp, 0000);
module_param(key1, charp, 0000);
module_param(key2, charp, 0000);
module_param(key3, charp, 0000);

MODULE_PARM_DESC(key0, "XTEA key part 0");
MODULE_PARM_DESC(key1, "XTEA key part 1");
MODULE_PARM_DESC(key2, "XTEA key part 2");
MODULE_PARM_DESC(key3, "XTEA key part 3");

/* Prototypes */
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char __user *, size_t, loff_t *);

static u32 xtea_key[4];

/* XTEA */

static void encipher(u32 num_rounds, u32 v[2], const u32 key[4])
{
    u32 i;
    u32 v0 = v[0];
    u32 v1 = v[1];
    u32 sum = 0;
    u32 delta = 0x9E3779B9;

    for (i = 0; i < num_rounds; i++) {
        v0 += (((v1 << 4) ^ (v1 >> 5)) + v1)
              ^ (sum + key[sum & 3]);

        sum += delta;

        v1 += (((v0 << 4) ^ (v0 >> 5)) + v0)
              ^ (sum + key[(sum >> 11) & 3]);
    }

    v[0] = v0;
    v[1] = v1;
}


static void decipher(u32 num_rounds, u32 v[2], const u32 key[4])
{
    u32 i;
    u32 v0 = v[0];
    u32 v1 = v[1];
    u32 delta = 0x9E3779B9;
    u32 sum = delta * num_rounds;

    for (i = 0; i < num_rounds; i++) {
        v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0)
              ^ (sum + key[(sum >> 11) & 3]);

        sum -= delta;

        v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1)
              ^ (sum + key[sum & 3]);
    }

    v[0] = v0;
    v[1] = v1;
}


/*
 * Converte um caractere hexadecimal para seu valor numérico.
 */
static int hex_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';

    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return -1;
}


/*
 * Converte:
 *
 * "aabbcc"
 *
 * para:
 *
 * 0xaa 0xbb 0xcc
 */
static int hex_to_bytes(const char *hex, u8 *data, size_t bytes)
{
    size_t i;

    if (strlen(hex) != bytes * 2)
        return -EINVAL;

    for (i = 0; i < bytes; i++) {
        int high = hex_value(hex[i * 2]);
        int low  = hex_value(hex[i * 2 + 1]);

        if (high < 0 || low < 0)
            return -EINVAL;

        data[i] = (high << 4) | low;
    }

    return 0;
}


/*
 * Converte bytes novamente para texto hexadecimal.
 */
static void bytes_to_hex(const u8 *data, size_t bytes, char *out)
{
    static const char digits[] = "0123456789abcdef";
    size_t i;

    for (i = 0; i < bytes; i++) {
        out[i * 2]     = digits[data[i] >> 4];
        out[i * 2 + 1] = digits[data[i] & 0x0F];
    }

    out[bytes * 2] = '\0';
}


static struct file_operations fops =
{
    .owner = THIS_MODULE,
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};


static int __init xtea_init(void)
{
    if (!key0 || !key1 || !key2 || !key3) {
    printk(KERN_ERR "XTEA Driver: all four keys must be provided\n");
    return -EINVAL;
    }

    if (kstrtou32(key0, 16, &xtea_key[0]) ||
        kstrtou32(key1, 16, &xtea_key[1]) ||
        kstrtou32(key2, 16, &xtea_key[2]) ||
        kstrtou32(key3, 16, &xtea_key[3])) {

        printk(KERN_ERR "XTEA Driver: invalid key\n");
        return -EINVAL;
    }

    printk(KERN_INFO "XTEA Driver: key loaded successfully\n");

    printk(KERN_INFO "XTEA Driver: initializing\n");

    majorNumber = register_chrdev(0, DEVICE_NAME, &fops);

    if (majorNumber < 0) {
        printk(KERN_ALERT "XTEA Driver: failed to register major number\n");
        return majorNumber;
    }

    charClass = class_create(THIS_MODULE, CLASS_NAME);

    if (IS_ERR(charClass)) {
        unregister_chrdev(majorNumber, DEVICE_NAME);
        return PTR_ERR(charClass);
    }

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
        return PTR_ERR(charDevice);
    }

    printk(KERN_INFO "XTEA Driver: /dev/%s created\n", DEVICE_NAME);

    return 0;
}


static void __exit xtea_exit(void)
{
    device_destroy(charClass, MKDEV(majorNumber, 0));
    class_unregister(charClass);
    class_destroy(charClass);
    unregister_chrdev(majorNumber, DEVICE_NAME);

    printk(KERN_INFO "XTEA Driver: unloaded\n");
}


static int dev_open(struct inode *inodep, struct file *filep)
{
    numberOpens++;

    printk(
        KERN_INFO
        "XTEA Driver: device opened %d time(s)\n",
        numberOpens
    );

    return 0;
}


static ssize_t dev_read(
    struct file *filep,
    char __user *buffer,
    size_t len,
    loff_t *offset)
{
    size_t remaining;
    size_t count;

    if (*offset >= size_of_message)
        return 0;

    remaining = size_of_message - *offset;

    count = len < remaining ? len : remaining;

    if (copy_to_user(buffer, message + *offset, count))
        return -EFAULT;

    *offset += count;

    printk(
        KERN_INFO
        "XTEA Driver: sent %zu characters to user\n",
        count
    );

    return count;
}


static ssize_t dev_write(
    struct file *filep,
    const char __user *buffer,
    size_t len,
    loff_t *offset)
{
    char input[MAX_COMMAND_SIZE];

    char command[4];

    unsigned int data_size;

    char hex_data[MAX_DATA_BYTES * 2 + 1];

    u8 data[MAX_DATA_BYTES];

    size_t i;

    int parsed;
    int ret;


    if (len >= sizeof(input))
        return -EINVAL;


    /*
     * Copia o comando do user space para kernel space.
     */
    if (copy_from_user(input, buffer, len))
        return -EFAULT;

    input[len] = '\0';


    /*
     * Formato esperado:
     *
     * enc key0 key1 key2 key3 tamanho dados
     */
    parsed = sscanf(
        input,
        "%3s %u %512s",
        command,
        &data_size,
        hex_data
    );

    if (parsed != 3) {
        printk(KERN_WARNING "XTEA Driver: invalid command\n");
        return -EINVAL;
    }


    if (data_size == 0 || data_size > MAX_DATA_BYTES) {
        printk(KERN_WARNING "XTEA Driver: invalid data size\n");
        return -EINVAL;
    }


    /*
     * XTEA trabalha com blocos de 64 bits = 8 bytes.
     */
    if (data_size % 8 != 0) {
        printk(
            KERN_WARNING
            "XTEA Driver: data size must be multiple of 8\n"
        );

        return -EINVAL;
    }


    ret = hex_to_bytes(hex_data, data, data_size);

    if (ret < 0) {
        printk(KERN_WARNING "XTEA Driver: invalid hexadecimal data\n");
        return ret;
    }

    /*
     * Processa 8 bytes por vez.
     */
    for (i = 0; i < data_size; i += 8) {

        u32 v[2];


        v[0] =
            ((u32)data[i] << 24) |
            ((u32)data[i + 1] << 16) |
            ((u32)data[i + 2] << 8) |
            ((u32)data[i + 3]);


        v[1] =
            ((u32)data[i + 4] << 24) |
            ((u32)data[i + 5] << 16) |
            ((u32)data[i + 6] << 8) |
            ((u32)data[i + 7]);


        if (strcmp(command, "enc") == 0) {

            encipher(32, v, xtea_key);

        } else if (strcmp(command, "dec") == 0) {

            decipher(32, v, xtea_key);

        } else {

            printk(KERN_WARNING "XTEA Driver: unknown command\n");
            return -EINVAL;
        }


        data[i]     = (v[0] >> 24) & 0xFF;
        data[i + 1] = (v[0] >> 16) & 0xFF;
        data[i + 2] = (v[0] >> 8)  & 0xFF;
        data[i + 3] = v[0] & 0xFF;

        data[i + 4] = (v[1] >> 24) & 0xFF;
        data[i + 5] = (v[1] >> 16) & 0xFF;
        data[i + 6] = (v[1] >> 8)  & 0xFF;
        data[i + 7] = v[1] & 0xFF;
    }


    /*
     * Guardamos o resultado em hexadecimal para que read()
     * possa devolvê-lo facilmente para a aplicação.
     */
    bytes_to_hex(data, data_size, message);

    size_of_message = strlen(message);


    printk(
        KERN_INFO
        "XTEA Driver: command %s processed %u bytes\n",
        command,
        data_size
    );


    return len;
}


static int dev_release(struct inode *inodep, struct file *filep)
{
    printk(KERN_INFO "XTEA Driver: device closed\n");

    return 0;
}


module_init(xtea_init);
module_exit(xtea_exit);