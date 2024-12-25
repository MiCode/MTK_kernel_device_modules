#ifndef CHARGER_PARTITION_H
#define CHARGER_PARTITION_H

#define CHARGER_WORK_DELAY_MS               50000

#define PARTITION_NAME                "charger"
#define CHARGER_PARTITION_MAXSIZE     (0x10000)
#define CHARGER_PARTITION_RWSIZE            (0x1000)  /* read 1 block one time, a total of 256 blocks */
#define CHARGER_PARTITION_OFFSET      0

#define UFSHCD "ufshcd"

#define PART_SECTOR_SIZE                        (0x200)
#define PART_BLOCK_SIZE                         (0x1000)
#define CHARGER_PARTITION_MAX_BLOCK_TRANSFERS   (128)

#define CHARGER_PARTITION_MAGIC            0x20240725

typedef enum
{
  CHARGER_PARTITION_HOST_LK,
  CHARGER_PARTITION_HOST_UEFI,
  CHARGER_PARTITION_HOST_ABL,
  CHARGER_PARTITION_HOST_ADSP,
  CHARGER_PARTITION_HOST_KERNEL,
  CHARGER_PARTITION_HOST_HAL,
  CHARGER_PARTITION_HOST_FRAMEWORK,
  CHARGER_PARTITION_HOST_APPLICATION,
  CHARGER_PARTITION_HOST_LAST,
  CHARGER_PARTITION_HOST_INVALID
} charger_partition_host_type;

/* a total of 256 blocks */
typedef enum
{
  CHARGER_PARTITION_HEADER,
  CHARGER_PARTITION_INFO_1,
  CHARGER_PARTITION_INFO_2,
  CHARGER_PARTITION_INFO_3,
  CHARGER_PARTITION_INFO_4,
  CHARGER_PARTITION_INFO_5,
  CHARGER_PARTITION_INFO_6,
  CHARGER_PARTITION_INFO_7,
  CHARGER_PARTITION_INFO_8,
  CHARGER_PARTITION_INFO_9,
  CHARGER_PARTITION_INFO_10,
  CHARGER_PARTITION_INFO_LAST,
  CHARGER_PARTITION_INFO_INVALID
} charger_partition_info_type;

typedef struct
{
  u32   magic;     /* Check charger partition magic */
  u32   version;   /* Version 2 at 2024/07/25 */
  u32   initialized;
  u32   avaliable;
  u32   reserved;   /* reseved*/
}charger_partition_header;

typedef struct
{
  u32   power_off_mode;    /* If go into poweroff charging mode at this boot time */
  u32   zero_speed_mode;   /* If trigger zero speed mode at this boot time */
  u32   test;
  u32   reserved;           /* reseved*/
}charger_partition_info_1;

int charger_partition_init(void);
void charger_partition_exit(void);

int charger_partition_alloc(u8 charger_partition_host_type, u8 charger_partition_info_type, uint32_t size);
int charger_partition_dealloc(u8 charger_partition_host_type, u8 charger_partition_info_type, uint32_t size);
void *charger_partition_read(u8 charger_partition_host_type, u8 charger_partition_info_type, uint32_t size);
int charger_partition_write(u8 charger_partition_host_type, u8 charger_partition_info_type, void *buf, uint32_t size);

#endif