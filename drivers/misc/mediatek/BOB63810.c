#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/io.h>
#include <linux/uaccess.h>

#define I2C_BUS_NUM     9       // 指定 I2C 总线号为 9
#define DEVICE_ADDR     0x75    // 设备地址
#define TARGET_REG      0x01    // 目标寄存器地址
#define DEVICE_TARGET_REG      0x03    // 厂商寄存器地址

struct bob_proc_data {
    struct i2c_client *client;
};
static struct bob_proc_data g_bob_proc_data;

struct bob_i2c_device {
    struct i2c_client *client;
};

// read register
static int read_reg(struct i2c_client *client, u8 reg, u8 *val)
{
    int ret = 0;
    ret = i2c_smbus_read_byte_data(client, reg);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to read reg 0x%02x: %d\n", reg, ret);
        return ret;
    }
    *val = (u8)ret;
    return 0;
}

// write register
static int write_reg(struct i2c_client *client, u8 reg, u8 val)
{
    int ret = 0;
    ret = i2c_smbus_write_byte_data(client, reg, val);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to write reg 0x%02x: %d\n", reg, ret);
    }
    return ret;
}

static void bob_i2c_remove(struct i2c_client *client)
{
    dev_info(&client->dev, "Device removed\n");
    remove_proc_entry("bob63810", NULL);
    g_bob_proc_data.client = NULL;
    return;
}
static const struct of_device_id bob_i2c_of_match[] = {
    { .compatible = "BOB63810" },
    { }
};
static const struct i2c_device_id bob_i2c_id[] = {
    { "BOB63810", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, bob_i2c_id);

static ssize_t bob_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    struct i2c_client *client = g_bob_proc_data.client;
    char user_input[2]={0};
    u8 reg_val;
    int ret = 0;

    if (!client) {
        pr_err("bob: device not probed yet\n");
        return -ENODEV;
    }

    if (count > sizeof(user_input)-1) {
        pr_err("bob: input too long (max 1 character)\n");
        return -EINVAL;
    }

    if (copy_from_user(user_input, buf, count)) {
        pr_err("bob: copy_from_user failed\n");
        return -EFAULT;
    }
    user_input[count] = '\0';

    if (strcmp(user_input, "1") == 0) {
        // 写入"1"：设置TARGET_REG的bit3为1
        ret = read_reg(client, TARGET_REG, &reg_val);
        if (ret) {
            pr_err("bob: read TARGET_REG failed (ret=%d)\n", ret);
            return ret;
        }

        reg_val |= (1 << 3);
        ret = write_reg(client, TARGET_REG, reg_val);
        if (ret) {
            pr_err("bob: write TARGET_REG failed (ret=%d)\n", ret);
            return ret;
        }
        pr_info("bob: set TARGET_REG bit3, new value=0x%02x\n", reg_val);

    } else if (strcmp(user_input, "0") == 0) {
        // 写入"0"：设置TARGET_REG的bit3为0
        ret = read_reg(client, TARGET_REG, &reg_val);
        if (ret) {
            pr_err("bob: read TARGET_REG failed (ret=%d)\n", ret);
            return ret;
        }
        pr_info("bob: input is 0, do not write register\n");

        reg_val &= 0xF7;
        ret = write_reg(client, TARGET_REG, reg_val);
        if (ret) {
            pr_err("bob: write TARGET_REG failed (ret=%d)\n", ret);
            return ret;
        }
        pr_info("bob: set TARGET_REG bit3, new value=0x%02x\n", reg_val);
    } else {
        // 无效输入
        pr_err("bob: invalid input (only '0' or '1' allowed)\n");
        return -EINVAL;
    }

    return count;
}

static int bob_proc_show(struct seq_file *m, void *v)
{
    struct i2c_client *client = m->private;
    u8 reg_val;
    u8 reg_val_device;
    int ret;

    if (!client) {
        seq_printf(m, "Error: Device not probed\n");
        return 0;
    }

    // 读取TARGET_REG
    ret = read_reg(client, TARGET_REG, &reg_val);
    if (ret) {
        seq_printf(m, "TARGET_REG (0x%02x): Read failed (ret=%d)\n", TARGET_REG, ret);
    } else {
        seq_printf(m, "TARGET_REG (0x%02x): 0x%02x (bit3: %s)\n",
                  TARGET_REG, reg_val, (reg_val & (1 << 3)) ? "SET (1)" : "CLEAR (0)");
    }

    // 读取DEVICE_TARGET_REG
    ret = read_reg(client, DEVICE_TARGET_REG, &reg_val_device);
    if (ret) {
        seq_printf(m, "DEVICE_TARGET_REG (0x%02x): Read failed (ret=%d)\n", DEVICE_TARGET_REG, ret);
    } else {
        seq_printf(m, "DEVICE_TARGET_REG (0x%02x): 0x%02x\n", DEVICE_TARGET_REG, reg_val_device);
    }

    return 0;
}

static int bob_proc_open(struct inode *inode, struct file *file)
{
    struct i2c_client *client = g_bob_proc_data.client;
    int ret;
    if (!client) {
        pr_err("bob: device not probed yet\n");
        return -ENODEV;
    }
    ret = single_open(file, bob_proc_show, client);
    if (ret) {
        pr_err("bob: single_open failed (err=%d)\n", ret);
        return ret; 
    }
    return 0;
}

static const struct proc_ops bob_proc_fops = {
    .proc_open = bob_proc_open,
    .proc_read = seq_read,
    .proc_write = bob_proc_write,
    .proc_release = single_release,
};



static int bob_i2c_probe(struct i2c_client *client,
                        const struct i2c_device_id *id)
{
    struct bob_i2c_device *dev;
    u8 reg_val = 0;
    u8 reg_val_device = 0;
    int ret = 0;
    pr_info("enter bob_i2c_probe");

    dev = devm_kzalloc(&client->dev, sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;
    proc_create("bob63810", 0666, NULL, &bob_proc_fops);

    dev->client = client;
    i2c_set_clientdata(client, dev);
    g_bob_proc_data.client = client;
    // 读取当前寄存器值
    ret = read_reg(client, TARGET_REG, &reg_val);
    if (ret){
        g_bob_proc_data.client = NULL;
        return ret;
    }

    dev_info(&client->dev, "TARGET_REG Original Reg 0x%02x: 0x%02x\n",
             TARGET_REG, reg_val);

    ret = read_reg(client, DEVICE_TARGET_REG, &reg_val_device);
    if (ret)
        dev_err(&client->dev, "DEVICE_TARGET_REG Original Reg read fail\n");

    dev_info(&client->dev, "DEVICE_TARGET_REG Original Reg 0x%02x: 0x%02x\n",
             DEVICE_TARGET_REG, reg_val_device);

    reg_val &= 0xF7;// 设置 bit3 为 0
    ret = write_reg(client, TARGET_REG, reg_val);
    if (ret) {
        pr_err("bob: write TARGET_REG failed (ret=%d)\n", ret);
        g_bob_proc_data.client = NULL;
        return ret;
    }
    dev_info(&client->dev, "Updated Reg 0x%02x: 0x%02x\n",TARGET_REG, reg_val);

    return 0;
}
static struct i2c_driver bob_i2c_driver = {
    .driver = {
        .name   = "BOB63810",
        .owner  = THIS_MODULE,
        .of_match_table = bob_i2c_of_match,
    },
    .probe      = bob_i2c_probe,
    .remove     = bob_i2c_remove,
    .id_table   = bob_i2c_id,
};
static int __init bob_i2c_init(void)
{
	int rc = 0;
	pr_info("enter bob_i2c_init,i2c_add_driver");

	rc = i2c_add_driver(&bob_i2c_driver);
	if (rc)
		pr_err("TS_INIT: Failed to register I2C driver: %d\n", rc);

	return rc;
}

static void __exit bob_i2c_exit(void)
{
    i2c_del_driver(&bob_i2c_driver);
}

module_init(bob_i2c_init);
module_exit(bob_i2c_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dongfeiju");
MODULE_DESCRIPTION("I2C Device Driver for bob63810 , Address 0x03 on I2C-9");
