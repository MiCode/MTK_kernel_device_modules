#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/types.h>

//#include "hi5022q_mipi_raw_Sensor.h"

#define PFX "HI5022Q_pdafotp"
#define LOG_INF(format, args...) pr_err(PFX "[%s] " format, __FUNCTION__, ##args)

//#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_typedef.h"

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
extern int iMultiReadReg(u16 a_u2Addr, u8 *a_puBuff, u16 i2cId, u8 number);

#define USHORT unsigned short
#define BYTE unsigned char
#define Sleep(ms) mdelay(ms)

#define HI5022Q_EEPROM_READ_ID 0xA1
#define HI5022Q_EEPROM_WRITE_ID 0xA0
#define HI5022Q_I2C_SPEED 100
#define HI5022Q_MAX_OFFSET 0xFFFF

#define DATA_SIZE 2048

#define READ_EEPROM_ID 0xA0
#define I2C_SPEED 100

BYTE hi5022q_eeprom_data[DATA_SIZE] = {0};
static bool get_done = false;
static int last_size = 0;
static int last_offset = 0;

BYTE eeprom_data[DATA_SIZE] = {0};
#if 0
bool getPDAFCalDataFromFile(void)
{
	bool Flag = false;
	int fd;
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);

	//fd = sys_open("/data/pdaf.txt", O_RDONLY, 777);
	fd = ksys_open("/sdcard/DCIM/pdaf.txt", O_RDONLY, 777);

	if (fd < 0)
	{
		LOG_INF("KYM PDAF FILE READ FAIL\n");
		goto RESULT;
	}
	else
	{
		//	 if( sys_read(fd, (char *)&hi5022q_eeprom_data[0], 1372) )
		if (ksys_read(fd, (char *)&hi5022q_eeprom_data[0], 1404))
		{
			LOG_INF("KYM PDAF FILE READ PASS\n");
			Flag = true;
		}
	}

RESULT:
	ksys_close(fd);
	set_fs(old_fs);
	return Flag;
}

bool _read_hi5022q_eeprom(kal_uint16 addr, BYTE *data, kal_uint32 size)
{
	bool Flag;
	addr = 0x0800;
	size = 1404;

	Flag = getPDAFCalDataFromFile();
	if (Flag)
		memcpy(data, hi5022q_eeprom_data, size);

	return Flag;
}
#endif
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//		read EEPROM In VCM
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if 1
static bool selective_read_eeprom(kal_uint16 addr, BYTE *data)
{

	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF)};

	if (addr > HI5022Q_MAX_OFFSET)
		return false;

	//kdSetI2CSpeed(I2C_SPEED);

	if (iReadRegI2C(pu_send_cmd, 2, (u8 *)data, 1, READ_EEPROM_ID) < 0)
	{
		LOG_INF("VCM E2PROM READ fail\n");
		return false;
	}

	//LOG_INF( "read eeprom : 0x%d \n", (u8*)data );

	return true;
}

bool _read_eeprom(kal_uint16 addr, BYTE *data, kal_uint32 size)
{
	int i = 0;
	int index = 0;
	int offset = addr;

	for (i = 0; i < 496; i++)
	{
		if (!selective_read_eeprom(offset, &data[i]))
		{
			return false;
		}
		//LOG_INF("VCM1 read_eeprom 0x%0x %d\n",offset, data[i]);
		offset++;
		index++;
	}

	offset = addr + 1024;

	for (i = 0; i < 908; i++)
	{ //ok
		if (!selective_read_eeprom(offset, &data[index]))
		{
			return false;
		}

		//LOG_INF("VCM2 read_eeprom 0x%0x %d\n",offset, data[i]);
		offset++;
		index++;
	}

	get_done = true;
	last_size = size;
	last_offset = addr;
	return true;
}

bool read_eeprom(kal_uint16 addr, BYTE *data, kal_uint32 size)
{
	BYTE af_pos[4];

	size = 0x057c; //0x5BC;//0x057c;
	addr = 0x0900;
	if (!get_done || last_size != size || last_offset != addr)
	{
		if (!_read_eeprom(addr, eeprom_data, size))
		{
			get_done = 0;
			last_size = 0;
			last_offset = 0;
			return false;
		}
	}
	memcpy(data, eeprom_data, size);

	selective_read_eeprom(0x0011, &af_pos[0]);
	selective_read_eeprom(0x0012, &af_pos[1]);
	selective_read_eeprom(0x0013, &af_pos[2]);
	selective_read_eeprom(0x0014, &af_pos[3]);
	LOG_INF("VCM read_eeprom Inf1 : 0x%0x%0x  , Macro : 0x%0x%0x \n", af_pos[0], af_pos[1], af_pos[2], af_pos[3]);

	return true;
}
/*
#define EEPROM_MODULEINFO_FLAG (0x0001)
#define EEPROM_MODULEINFO_VALUE (0x0002)
#define EEPROM_MODULEINFO_CHKSUM (0x0004)
#define EEPROM_AWB_FLAG (0x0008)
#define EEPROM_AWB_CHKSUM (0x0010)
#define EEPROM_LSC_FLAG (0x0020)
#define EEPROM_LSC_CHKSUM (0x0040)
#define EEPROM_AF_FLAG (0x0080)
#define EEPROM_AF_CHKSUM (0x0100)
#define EEPROM_PDAF_FLAG (0x0200)
#define EEPROM_PDAF_CHKSUM (0x0400)
#define EEPROM_AWB_GOLDEN_FLAG (0x0800)
#define EEPROM_AWB_GOLDEN_CHKSUM (0x1000)
*/
/*
#define EEPROM_XGC_FLAG (0x2000)
#define EEPROM_XGC_CHKSUM (0x4000)
#define EEPROM_QBGC_FLAG (0x8000)
#define EEPROM_QBGC_CHKSUM (0x10000)
#define EEPROM_PGC_FLAG (0x20000)
#define EEPROM_PGC_CHKSUM (0x40000)
*/

#define EEPROM_XTC_FLAG (0x2000)
#define EEPROM_XTC_CHKSUM (0x4000)


#define EEPROM_READ_ID 0xA0
#define EEPROM_WRITE_ID 0xA1

/*
#define MODULE_INFO_START_ADDR 0x2200
#define AWB_INFO_START_ADDR 0x2210
#define AF_INFO_START_ADDR 0x2220
#define LSC_INFO_START_ADDR 0x2230
#define PDAF_INFO_START_ADDR 0x297E
*/
/*
#define ISP_XGC_START_ADDR 0x0D4E
#define ISP_QBGC_START_ADDR 0x14D0
#define ISP_PGC_START_ADDR 0x19D2
*/

#define ISP_XTC_START_ADDR 0x0CD5
#if 0
typedef struct moduleInformation_struct
{
	BYTE moduleInoramtionFlag;
	BYTE moduleID;
	BYTE Year;
	BYTE Month;
	BYTE Day;
	BYTE Mirror_flip;
	BYTE SensorID;
	BYTE LENSID;
	BYTE LightSourceFlag;
	BYTE ColorTemperatureID;
	BYTE AF_FF_FLAG;
	BYTE reserved1;
	BYTE reserved2;
    BYTE reserved3;
	BYTE reserved4;
	BYTE checksum;
} moduleInformation_struct;

typedef struct awb_struct
{
	BYTE awbFlag;
	BYTE awbInformation[6];
	BYTE awbGoldenInformation[6];
	BYTE reserved1;
	BYTE reserved2;
	BYTE chksum;
} awb_struct;

typedef struct af_struct
{
	BYTE afFlag;
	BYTE afData[13];
	BYTE reserved1;
	BYTE chksum;
} af_struct;

typedef struct lsc_struct
{
	BYTE lscFlag;
	BYTE lscData[1868];
	BYTE chksum;
} lsc_struct;



typedef struct pdaf_struct
{
	BYTE pdafFlag;
	BYTE pdafData1[496];
	BYTE pdafData2[1004];
	BYTE chksum1;
	BYTE chksum2;
} pdaf_struct;
#endif
#if 0
typedef struct xgc_struct
{
	BYTE xgcData1[960];
	BYTE xgcData2[960];
} xgc_struct;

typedef struct qbgc_struct
{
	BYTE qbgcData[1280];
} qbgc_struct;

typedef struct pgc_struct
{
	BYTE pgcData[2048];
} pgc_struct;

#endif

typedef struct xtc_struct
{
	BYTE xtcFlag;
	BYTE xgcData[1920];
	BYTE qbgcData[1280];
	BYTE opcData[2048];
	BYTE chksum;
} xtc_struct;


extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData,
					   u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);

static bool read_hi5022q_eeprom(kal_uint16 addr, BYTE *data, int size)
{
	int i = 0;
	int offset = addr;
	int ret;
	u8 pu_send_cmd[2];

#define MAX_READ_WRITE_SIZE 255

	for (i = 0; i < size; i += MAX_READ_WRITE_SIZE)
	{
		pu_send_cmd[0] = (u8)(offset >> 8);
		pu_send_cmd[1] = (u8)(offset & 0xFF);

		if (i + MAX_READ_WRITE_SIZE > size)
		{
			ret = iReadRegI2C(pu_send_cmd, 2,
							  (u8 *)(data + i),
							  (size - i),
							  EEPROM_READ_ID);
		}
		else
		{
			ret = iReadRegI2C(pu_send_cmd, 2,
							  (u8 *)(data + i),
							  MAX_READ_WRITE_SIZE,
							  EEPROM_READ_ID);
		}
		if (ret < 0)
		{
			LOG_INF("read spc failed!\n");
			return false;
		}

		offset += MAX_READ_WRITE_SIZE;
	}

	LOG_INF("exit _read_eeprom size = %d\n", size);
	return true;
}


static void dump_data(void)
{
#if 0
	int i;
	BYTE *buf=NULL;

	buf = kmalloc(DATA_SIZE, GFP_KERNEL);
	read_ov16e10_eeprom(0x0, buf, DATA_SIZE);

	for(i = 0 ; i < DATA_SIZE/0x10 ; i++)
		LOG_INF("addr 0x%8x:  0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x", \
		i*0x10,\
		buf[i*0x10+0],buf[i*0x10+1],buf[i*0x10+ 2],buf[i*0x10+ 3],buf[i*0x10+ 4],buf[i*0x10+ 5],buf[i*0x10+ 6],buf[i*0x10+ 7],\
		buf[i*0x10+8],buf[i*0x10+9],buf[i*0x10+10],buf[i*0x10+11],buf[i*0x10+12],buf[i*0x10+13],buf[i*0x10+14],buf[i*0x10+15]);

	kfree(buf);
    return;
#else
	return;
#endif
}
#if 0
static bool check_sum(BYTE *buf, unsigned int size)
{
	int i, sum = 0;

	for (i = 1; i <= size - 2; i++)
	{
		sum += buf[i];
		//LOG_INF("buf[%d] = 0x%x %d", i, buf[i], buf[i]);
	}

	if ((sum % 256) != buf[size - 1])
	{
		//LOG_INF("chksum fail size = %d sum=%d sum-in-eeprom=%d", size, (sum % 255 + 1), buf[size - 1]);
		return false;
	}
	return true;
}

static bool check_sum_awb(awb_struct *buf, unsigned int size)
{
	int i, sum = 0;

	for (i = 0; i < sizeof(buf->awbInformation); i++)
	{
		sum += buf->awbInformation[i];
		//LOG_INF("awbData[%d] = 0x%x %d", i, buf->awbInformation[i], buf->awbInformation[i]);
	}
       

	for (i = 0; i < sizeof(buf->awbGoldenInformation); i++)
	{
		sum += buf->awbGoldenInformation[i];
		//LOG_INF("awbGolden Data[%d] = 0x%x %d", i, buf->awbGoldenInformation[i], buf->awbGoldenInformation[i]);
	}

	sum += buf->reserved1;
	sum += buf->reserved2;

	if ( (sum % 256) != buf->chksum)
	{
		LOG_INF("chksum fail size = %d sum=%d sum-in-eeprom=%d", size, (sum % 256), buf->chksum);
		return false;
	}
	return true;
}

static bool check_sum_lsc(lsc_struct *buf, unsigned int size)
{
	int i, sum = 0;

	for (i = 0; i < sizeof(buf->lscData); i++)
	{
		sum += buf->lscData[i];
	}

	if ((sum % 256) != buf->chksum)
	{
		LOG_INF("chksum fail size = %d sum=%d sum-in-eeprom=%d", size, (sum % 256), buf->chksum);
		return false;
	}
	return true;
}

static bool check_sum_af(af_struct *buf, unsigned int size)
{
	int i, sum = 0;

	for (i = 0; i < sizeof(buf->afData); i++)
	{
		sum += buf->afData[i];
		//LOG_INF("afData[%d] = 0x%x %d", i, buf->afData[i], buf->afData[i]);
	}
       
	if ( (sum % 256) != buf->chksum)
	{
		//LOG_INF("chksum fail size = %d sum=%d sum-in-eeprom=%d", size, (sum % 256), buf->chksum);
		return false;
	}

	 sum += buf->reserved1;

	return true;
}

static bool check_sum_pdaf(pdaf_struct *buf, unsigned int size)
{
	int i, sum1 = 0, sum2 = 0;

	for (i = 0; i < 496; i++)
	{
		sum1 += buf->pdafData1[i];
	}

	for (i = 0; i < 1004; i++)
	{
		sum2 += buf->pdafData2[i];
	}

	if ((sum1 % 256) != buf->chksum1 || (sum2 % 256) != buf->chksum2)
	{
		LOG_INF("chksum fail size = %d sum1=%d sum1-in-eeprom=%d sum2=%d sum2-in-eeprom=%d", size, (sum1 % 256), buf->chksum1, (sum2 % 256), buf->chksum2);
		return false;
	}
	return true;
}
#endif
#if 0
static bool check_sum_xgc(xgc_struct *buf, unsigned int size)
{
	int i, sum = 0;

	for (i = 0; i < sizeof(buf->xgcData); i++)
	{
		sum += buf->xgcData[i];
		LOG_INF("xgcData[%d] = 0x%x %d", i, buf->xgcData[i], buf->xgcData[i]);
	}
        sum += buf->xgcFlag;
	if (((sum % 255) + 1) != buf->chksum)
	{
		LOG_INF("chksum fail size = %d sum=%d sum-in-eeprom=%d", size, (sum % 255 + 1), buf->chksum);
		return false;
	}
	return true;
}

static bool check_sum_qbgc(qbgc_struct *buf, unsigned int size)
{
	int i, sum = 0;

	for (i = 0; i < sizeof(buf->qbgcData); i++)
	{
		sum += buf->qbgcData[i];
		LOG_INF("qbgcData[%d] = 0x%x %d", i, buf->qbgcData[i], buf->qbgcData[i]);
	}
        sum += buf->qbgcFlag;
	if (((sum % 255) + 1) != buf->chksum)
	{
		LOG_INF("chksum fail size = %d sum=%d sum-in-eeprom=%d", size, (sum % 255 + 1), buf->chksum);
		return false;
	}
	return true;
}

static bool check_sum_pgc(pgc_struct *buf, unsigned int size)
{
	int i, sum = 0;

	for (i = 0; i < sizeof(buf->pgcData); i++)
	{
		sum += buf->pgcData[i];
		LOG_INF("pgcData[%d] = 0x%x %d", i, buf->pgcData[i], buf->pgcData[i]);
	}
        sum += buf->pgcFlag;
	if (((sum % 255) + 1) != buf->chksum)
	{
		LOG_INF("chksum fail size = %d sum=%d sum-in-eeprom=%d", size, (sum % 255 + 1), buf->chksum);
		return false;
	}
	return true;
}
#endif


static bool check_sum_xtc(xtc_struct *buf, unsigned int size)
{
	int i, sum = 0;

	for (i = 0; i < sizeof(buf->xgcData); i++)
	{
		sum += buf->xgcData[i];
		//LOG_INF("xgcData[%d] = 0x%x %d", i, buf->xgcData[i], buf->xgcData[i]);
	}

	for (i = 0; i < sizeof(buf->qbgcData); i++)
	{
		sum += buf->qbgcData[i];
		//LOG_INF("qbcData[%d] = 0x%x %d", i, buf->qbgcData[i], buf->qbgcData[i]);
	}

	for (i = 0; i < sizeof(buf->opcData); i++)
	{
		sum += buf->opcData[i];
		//LOG_INF("opcData[%d] = 0x%x %d", i, buf->opcData[i], buf->opcData[i]);
	}

	if ((sum % 256) != buf->chksum)
	{
		LOG_INF("chksum xtc fail size = %d sum=%d sum-in-eeprom=%d", size, (sum % 256), buf->chksum);
		return false;
	}
	return true;
}



kal_uint8 xgc_buf[1920];
kal_uint8 qbgc_buf[1280];
kal_uint8 opc_buf[2048];


bool check_hi5022q_otp(void)
{
/*
	moduleInformation_struct *pModuleInfo;
	awb_struct *pAWBInfo;
	lsc_struct *pLSCInfo;
	af_struct *pAFInfo;
	pdaf_struct *pPDAFInfo;
*/	
/*
        xgc_struct *pXGCInfo;
        qbgc_struct *pQBGCInfo;
        pgc_struct *pPGCInfo;
*/  
   xtc_struct *pXTCInfo;

	unsigned int otpStatus = 0;
#if 0
	pModuleInfo = kmalloc(sizeof(moduleInformation_struct), GFP_KERNEL);
	read_hi5022q_eeprom(MODULE_INFO_START_ADDR, (BYTE *)pModuleInfo, sizeof(moduleInformation_struct));
	if (pModuleInfo->moduleInoramtionFlag == 0x01)
	{
		otpStatus |= EEPROM_MODULEINFO_FLAG;
		if (check_sum((BYTE *)pModuleInfo, sizeof(moduleInformation_struct)))
		{
			otpStatus |= EEPROM_MODULEINFO_CHKSUM;
			if (pModuleInfo->moduleID == 0x03 &&pModuleInfo->SensorID == 0xF9 && pModuleInfo->LENSID == 0x01,pModuleInfo->ColorTemperatureID == 0x01)
			{
				otpStatus |= EEPROM_MODULEINFO_VALUE;
				LOG_INF("module info flag chksum value pass");
			}
			else
			{
				LOG_INF("moduleID =0x%x,SensorID=0x%x,LENSID=0x%x,DriverICID=0x%x,ColorTemperatureID=0x%x",
					   pModuleInfo->moduleID, pModuleInfo->SensorID, pModuleInfo->LENSID,pModuleInfo->ColorTemperatureID);
			}
		}
	}
	else
	{
		LOG_INF("moduleInoramtionFlag=%d", pModuleInfo->moduleInoramtionFlag);
	}
	kfree(pModuleInfo);

	pAWBInfo = kmalloc(sizeof(awb_struct), GFP_KERNEL);
	read_hi5022q_eeprom(AWB_INFO_START_ADDR, (BYTE *)pAWBInfo, sizeof(awb_struct));
	if (pAWBInfo->awbFlag == 0x01)
	{
		otpStatus |= EEPROM_AWB_FLAG;
		if (check_sum_awb(pAWBInfo, sizeof(awb_struct)))
		{
			otpStatus |= EEPROM_AWB_CHKSUM;
			LOG_INF("awb flag chksum pass");
		}
		else
		{
			int i;
			for (i = 0; i < sizeof(pAWBInfo->awbInformation); i++)
				LOG_INF("awb[%d]=0x%x  %d\n", i, pAWBInfo->awbInformation[i], pAWBInfo->awbInformation[i]);

			for (i = 0; i < sizeof(pAWBInfo->awbGoldenInformation); i++)
				LOG_INF("awbgoldne[%d]=0x%x  %d\n", i, pAWBInfo->awbGoldenInformation[i], pAWBInfo->awbGoldenInformation[i]);
		}
	}
	else
	{
		LOG_INF("awbFlag=%d", pAWBInfo->awbFlag);
	}
	kfree(pAWBInfo);


	pAFInfo = kmalloc(sizeof(af_struct), GFP_KERNEL);
	read_hi5022q_eeprom(AF_INFO_START_ADDR, (BYTE *)pAFInfo, sizeof(af_struct));
	if (pAFInfo->afFlag == 0x01)
	{
		otpStatus |= EEPROM_AF_FLAG;
		if (check_sum_af(pAFInfo, sizeof(af_struct)))
		{
			otpStatus |= EEPROM_AF_CHKSUM;
			LOG_INF("af flag chksum pass");
		}
	}
	else
	{
		LOG_INF("afFlag=%d", pAFInfo->afFlag);
	}
	kfree(pAFInfo);

	pLSCInfo = kmalloc(sizeof(lsc_struct), GFP_KERNEL);
	read_hi5022q_eeprom(LSC_INFO_START_ADDR, (BYTE *)pLSCInfo, sizeof(lsc_struct));
	if (pLSCInfo->lscFlag == 0x01)
	{
		otpStatus |= EEPROM_LSC_FLAG;
		if (check_sum_lsc(pLSCInfo, sizeof(lsc_struct)))
		{
			otpStatus |= EEPROM_LSC_CHKSUM;
			LOG_INF("lsc flag chksum pass");
		}
	}
	else
	{
		LOG_INF("lscFlag=%d", pLSCInfo->lscFlag);
	}
	kfree(pLSCInfo);


	pPDAFInfo = kmalloc(sizeof(pdaf_struct), GFP_KERNEL);
	read_hi5022q_eeprom(PDAF_INFO_START_ADDR, (BYTE *)pPDAFInfo, sizeof(pdaf_struct));
	if (pPDAFInfo->pdafFlag == 0x01)
	{
		otpStatus |= EEPROM_PDAF_FLAG;
		if (check_sum_pdaf(pPDAFInfo, sizeof(pdaf_struct)))
		{
			otpStatus |= EEPROM_PDAF_CHKSUM;
			LOG_INF("pdaf flag chksum pass");
		}
		else
		{
			int i;
			for (i = 0; i < 5; i++)
				LOG_INF("pdaf[%d]=0x%x", i, pPDAFInfo->pdafData1[i]);
		}
	}
	else
	{
		int i;
		LOG_INF("pdafFlag=%d", pPDAFInfo->pdafFlag);

		for (i = 0; i < 5; i++)
			LOG_INF("pdaf[%d]=0x%x", i, pPDAFInfo->pdafData1[i]);
	}
	kfree(pPDAFInfo);
#endif

#if 0
    pXGCInfo = kmalloc(sizeof(xgc_struct), GFP_KERNEL);
	read_hi5022q_eeprom(ISP_XGC_START_ADDR, (BYTE *)pXGCInfo, sizeof(xgc_struct));
	if (pXGCInfo->xgcFlag == 0x55)
	{
		otpStatus |= EEPROM_XGC_FLAG;
		if (check_sum_xgc(pXGCInfo, sizeof(xgc_struct)))
		{
                        memcpy(xgc_buf, pXGCInfo->xgcData, 1920);
			otpStatus |= EEPROM_XGC_CHKSUM;
			LOG_INF("xgc flag chksum pass");
		}
	}
	else
	{
		LOG_INF("xgcFlag=%d", pXGCInfo->xgcFlag);
	}
	kfree(pXGCInfo);

        pQBGCInfo = kmalloc(sizeof(qbgc_struct), GFP_KERNEL);
	read_hi5022q_eeprom(ISP_QBGC_START_ADDR, (BYTE *)pQBGCInfo, sizeof(qbgc_struct));
	if (pQBGCInfo->qbgcFlag == 0x55)
	{
		otpStatus |= EEPROM_QBGC_FLAG;
		if (check_sum_qbgc(pQBGCInfo, sizeof(qbgc_struct)))
		{
                        memcpy(qbgc_buf, pQBGCInfo->qbgcData, 1280);
			otpStatus |= EEPROM_QBGC_CHKSUM;
			LOG_INF("qbgc flag chksum pass");
		}
	}
	else
	{
		LOG_INF("qbgcFlag=%d", pQBGCInfo->qbgcFlag);
	}
	kfree(pQBGCInfo);

        pPGCInfo = kmalloc(sizeof(pgc_struct), GFP_KERNEL);
	read_hi5022q_eeprom(ISP_PGC_START_ADDR, (BYTE *)pPGCInfo, sizeof(pgc_struct));
	if (pPGCInfo->pgcFlag == 0x55)
	{
		otpStatus |= EEPROM_PGC_FLAG;
		if (check_sum_pgc(pPGCInfo, sizeof(pgc_struct)))
		{
                        memcpy(pgc_buf, pPGCInfo->pgcData, 2048);
			otpStatus |= EEPROM_PGC_CHKSUM;
			LOG_INF("pgc flag chksum pass");
		}
	}
	else
	{
		LOG_INF("pgcFlag=%d", pPGCInfo->pgcFlag);
	}
	kfree(pPGCInfo);
#endif

	pXTCInfo = kmalloc(sizeof(xtc_struct), GFP_KERNEL);
	read_hi5022q_eeprom(ISP_XTC_START_ADDR, (BYTE *)pXTCInfo, sizeof(xtc_struct));
	if (pXTCInfo->xtcFlag == 0x01)
	{
		otpStatus |= EEPROM_XTC_FLAG;
		if (check_sum_xtc(pXTCInfo, sizeof(xtc_struct)))
		{
            memcpy(xgc_buf, pXTCInfo->xgcData, 1920);
			memcpy(qbgc_buf, pXTCInfo->qbgcData, 1280);
			memcpy(opc_buf, pXTCInfo->opcData, 2048);
			otpStatus |= EEPROM_XTC_CHKSUM;
			LOG_INF("xtc flag chksum pass");
		}
	}
	else
	{
		LOG_INF("xtcFlag=%d", pXTCInfo->xtcFlag);
	}
	kfree(pXTCInfo);

	dump_data();
/*
#define EEPROM_XGC_FLAG (0x2000)
#define EEPROM_XGC_CHKSUM (0x4000)
#define EEPROM_QBGC_FLAG (0x8000)
#define EEPROM_QBGC_CHKSUM (0x10000)
#define EEPROM_PGC_FLAG (0x20000)
#define EEPROM_PGC_CHKSUM (0x40000)
*/	
	if (otpStatus == (/*EEPROM_MODULEINFO_CHKSUM | EEPROM_MODULEINFO_FLAG | EEPROM_MODULEINFO_VALUE |
					  EEPROM_AWB_CHKSUM | EEPROM_AWB_FLAG |
					  EEPROM_AF_CHKSUM | EEPROM_AF_FLAG |
					  EEPROM_LSC_CHKSUM | EEPROM_LSC_FLAG |
					  EEPROM_PDAF_CHKSUM | EEPROM_PDAF_FLAG *//*|
                                          EEPROM_XGC_FLAG | EEPROM_XGC_CHKSUM |
                                          EEPROM_QBGC_FLAG | EEPROM_QBGC_CHKSUM | EEPROM_PGC_FLAG | EEPROM_PGC_CHKSUM|*/
										  EEPROM_XTC_FLAG| EEPROM_XTC_CHKSUM))
	{
                LOG_INF("hi5022q otp check pass\n");
		return true;
	}
	else
	{
		LOG_INF("otp check fail otpStatus=0x%x", otpStatus);
		return false;
	}
}


bool hi5022q_read_otp_xgc(BYTE *data,int size)
{
	memcpy(data, xgc_buf, 1920);
	LOG_INF("hi5022q_read_otp_xgc \n");
	return 0;
}

bool hi5022q_read_otp_qbgc(BYTE *data,int size)
{
	memcpy(data, qbgc_buf, 1280);
	LOG_INF("hi5022q_read_otp_qbgc \n");
	return 0;
}

bool hi5022q_read_otp_opc(BYTE *data)
{
	memcpy(data, opc_buf, 2048);
	LOG_INF("hi5022q_read_otp_opc \n");
	return 0;
}

#endif
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
