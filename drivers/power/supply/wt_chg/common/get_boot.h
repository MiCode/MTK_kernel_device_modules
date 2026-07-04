#ifndef __LINUX_GET_BOOT_H__
#define __LINUX_GET_BOOT_H__

enum {
    BOOT_MODE_NORMOL = 0,
    BOOT_MODE_TEST,
};

uint8_t get_boot_mode(void);

#endif
