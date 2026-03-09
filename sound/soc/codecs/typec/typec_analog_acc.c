// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2024, Xiaomi Corporation, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/usb/typec.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/debugfs.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include "typec_analog_acc.h"
#include "../../../../drivers/misc/mediatek/typec/tcpc/inc/tcpci_core.h"
#include "../../../../drivers/misc/mediatek/typec/tcpc/inc/tcpm.h"

/**
 * need match kernel define
 * use SND_JACK_VIDEOOUT for unsupported bit
 */
#define JACK_TYPEC_ANALOG_MASK (SND_JACK_VIDEOOUT | SND_JACK_HEADSET)
#define JACK_TYPEC_ANALOG_TYPES 6

#define HEADSET_STATUS_RECORD_INDEX_PLUGIN (0)
#define HEADSET_STATUS_RECORD_INDEX_PLUGOUT (4)
#define HEADSET_EVENT_PLUGIN_MICROPHONE (4)
#define HEADSET_EVENT_PLUGOUT_MICROPHONE (4)
#define HEADSET_EVENT_MAX (5)
#define DEBUGFS_DIR_NAME "accdet"
#define DEBUGFS_HEADSET_STATUS_FILE_NAME "headset_status_acc"
static u16 headset_status[HEADSET_EVENT_MAX] = {0,0,0,0,0};
static struct dentry* accdet_debugfs_dir;

struct typec_analog_acc_priv {
    struct notifier_block psy_nb;
    struct snd_soc_jack typec_analog_jack;
    struct tcpc_device *tcpc_dev;
};

static const int jack_switch_types[JACK_TYPEC_ANALOG_TYPES] = {
    SW_HEADPHONE_INSERT,
    SW_MICROPHONE_INSERT,
    SW_LINEOUT_INSERT,
    SW_JACK_PHYSICAL_INSERT,
    SW_VIDEOOUT_INSERT,
    SW_LINEIN_INSERT,
};

static struct typec_analog_acc_priv *acc_priv = NULL;

static ssize_t headset_status_read(struct file *filp, char __user *buffer,
    size_t count, loff_t *ppos);
static ssize_t headset_status_write(struct file *filp, const char __user *buffer,
    size_t count, loff_t *ppos);
static void add_headset_event(u32 event_index, u32 event_offset);

static void add_headset_event(u32 event_index, u32 event_offset) {
    u16 status;
    if (event_index >= HEADSET_EVENT_MAX) {
        return;
    }
    status = (headset_status[event_index] & (0xF << event_offset));
    status += (0x1 << event_offset);
    if (status > (0xF << event_offset)) {
        status = (0xF << event_offset);
    }
    headset_status[event_index] = (headset_status[event_index] & (~(0xF << event_offset))) + status;
    return;
}
static ssize_t headset_status_read(struct file *filp, char __user *buffer,
    size_t count, loff_t *ppos) {
    char buf[64];
    sprintf(buf, "0x%04x 0x%04x 0x%04x 0x%04x 0x%04x\n",
            headset_status[0], headset_status[1],
            headset_status[2], headset_status[3],
            headset_status[4]);
    return simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
}
static ssize_t headset_status_write(struct file *filp, const char __user *buffer,
    size_t count, loff_t *ppos) {
    char buf[4];
    size_t buf_size = min(count, sizeof(buf) - 1);
    if (copy_from_user(buf, buffer, buf_size))
        return -EFAULT;
    if (strncmp(buf, "0", 1) == 0) {
        memset(headset_status, 0, sizeof(headset_status));
    }
    return count;
}
static const struct file_operations accdet_headset_status_fops = {
    .owner = THIS_MODULE,
    .read = headset_status_read,
    .write = headset_status_write,
};

static void typec_analog_acc_jack_report(struct snd_soc_jack *jack, int status, int mask)
{
    pr_info("%s: enter, jack->status: %d, status: %d, mask: %d\n", __func__, jack->status, status, mask);
    struct input_dev *idev;

    idev = input_get_device(jack->jack->input_dev);
    if (!idev) {
        pr_err("%s: get idev fail, return\n", __func__);
        return;
    }

    for (int i = 0; i < ARRAY_SIZE(jack_switch_types); i++) {
        unsigned int mask_bits = 0;
        int testbit = ((1 << i) & ~mask_bits);
        if (jack->jack->type & testbit) {
            input_report_switch(idev, jack_switch_types[i], status & testbit);
        }
    }
    input_sync(idev);
    input_put_device(idev);

    if (jack->status || status)
        add_headset_event(status ? HEADSET_STATUS_RECORD_INDEX_PLUGIN : HEADSET_STATUS_RECORD_INDEX_PLUGOUT,
                          status ? HEADSET_EVENT_PLUGIN_MICROPHONE : HEADSET_EVENT_PLUGOUT_MICROPHONE);
}

static int typec_analog_acc_event_changed(struct notifier_block *nb, unsigned long event, void *ptr)
{
    struct typec_analog_acc_priv *priv = NULL;
    priv = container_of(nb, struct typec_analog_acc_priv, psy_nb);
    struct tcp_notify *noti = ptr;

    if (!priv) {
        pr_err("%s: priv is NULL\n", __func__);
    }

    switch (event) {
    case TCP_NOTIFY_TYPEC_STATE:
        if (noti->typec_state.new_state == TYPEC_ATTACHED_AUDIO) {
            typec_analog_acc_jack_report(&priv->typec_analog_jack,
                        (SND_JACK_HEADSET | SND_JACK_VIDEOOUT), JACK_TYPEC_ANALOG_MASK);
            priv->typec_analog_jack.status = SND_JACK_HEADSET | SND_JACK_VIDEOOUT;
            pr_info("%s: Audio Plug In\n", __func__);
        } else if (noti->typec_state.new_state == TYPEC_UNATTACHED) {
            typec_analog_acc_jack_report(&priv->typec_analog_jack, 0, JACK_TYPEC_ANALOG_MASK);
            priv->typec_analog_jack.status = 0;
            pr_info("%s: Audio Plug Out\n", __func__);
        }
        break;
    default:
        break;
    }

    return 0;
}

int typec_analog_acc_init(struct snd_soc_card *card)
{
    int ret = 0;

    acc_priv = kzalloc(sizeof(struct typec_analog_acc_priv), GFP_KERNEL);
    if (!acc_priv) {
        pr_err("%s: Failed to allocate memory for acc_priv\n", __func__);
        return -ENOMEM;
    }

    acc_priv->tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
    if (!acc_priv->tcpc_dev) {
        pr_err("%s: Get TCPC device failed\n", __func__);
        ret = -EPROBE_DEFER;
        goto err;
    }

    acc_priv->psy_nb.notifier_call = typec_analog_acc_event_changed;
    acc_priv->psy_nb.priority = 0;
    ret = register_tcp_dev_notifier(acc_priv->tcpc_dev, &acc_priv->psy_nb, TCP_NOTIFY_TYPE_USB);
    if (ret) {
        pr_err("%s: TCPC notifier reg failed: %d\n", __func__, ret);
        goto err;
    }

    struct snd_jack *jjack;
    jjack = kzalloc(sizeof(struct snd_jack), GFP_KERNEL);
    if (jjack == NULL)
        return -ENOMEM;
    jjack->id = kstrdup("Typec_analog Jack", GFP_KERNEL);
    if (jjack->id == NULL) {
        kfree(jjack);
        return -ENOMEM;
    }
    jjack->input_dev = input_allocate_device();
    if (jjack->input_dev == NULL) {
        ret = -ENOMEM;
        goto err;
    }
    jjack->input_dev->name = "Typec_analog Jack";
    jjack->type = JACK_TYPEC_ANALOG_MASK;

    for (int i = 0; i < JACK_TYPEC_ANALOG_TYPES; i++)
        if (JACK_TYPEC_ANALOG_MASK & (1 << i))
            input_set_capability(jjack->input_dev, EV_SW, jack_switch_types[i]);

    ret = input_register_device(jjack->input_dev);
    if (ret) {
        pr_err("%s: register input device failed (%d)\n", __func__, ret);
        goto err;
    }
    acc_priv->typec_analog_jack.jack = jjack;

    struct dentry *dentry;
    dentry = debugfs_lookup(DEBUGFS_DIR_NAME, NULL);
    if (dentry) {
        accdet_debugfs_dir = dentry;
        pr_info("accdet folder already exists in debugfs\n");
    } else 
        accdet_debugfs_dir = debugfs_create_dir(DEBUGFS_DIR_NAME, NULL);

    if (!IS_ERR(accdet_debugfs_dir)) {
        debugfs_create_file(DEBUGFS_HEADSET_STATUS_FILE_NAME, 0666,
            accdet_debugfs_dir, NULL, &accdet_headset_status_fops);
    } else {
        pr_err("%s: create debugfs dir failed\n", __func__);
    }

    pr_info("%s: exit\n", __func__);
err:
    return ret;
}
EXPORT_SYMBOL(typec_analog_acc_init);

static int __init analog_acc_init(void)
{
    return 0;
}

static void __exit analog_acc_exit(void)
{
    kfree(acc_priv);
}

module_init(analog_acc_init);
module_exit(analog_acc_exit);

MODULE_DESCRIPTION("type analog accessory module");
MODULE_LICENSE("GPL v2");
