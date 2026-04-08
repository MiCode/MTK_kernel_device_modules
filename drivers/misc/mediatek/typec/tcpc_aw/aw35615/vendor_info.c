// SPDX-License-Identifier: GPL-2.0
/*******************************************************************************
 *** Copyright (C), 2020-2021, Awinic.All rights reserved. ************
 *******************************************************************************
 * Author        : awinic
 * Date          : 2021-12-17
 * Description   : .C file function description
 * Version       : 1.0
 * Function List :
 ******************************************************************************/
#include "vendor_info.h"
#include "PD_Types.h"
#include "aw35615_global.h"

void VIF_InitializeSrcCaps(doDataObject_t *src_caps)
{
	struct aw35615_chip *chip = aw35615_GetChip();
	AW_U8 i;

	doDataObject_t gSrc_caps[7] = {
		/* macro expects index starting at 1 and type */
		CREATE_SUPPLY_PDO_FIRST(1),
		//CREATE_SUPPLY_PDO(1, Src_PDO_Supply_Type1),
		CREATE_SUPPLY_PDO(2, Src_PDO_Supply_Type2),
		CREATE_SUPPLY_PDO(3, Src_PDO_Supply_Type3),
		CREATE_SUPPLY_PDO(4, Src_PDO_Supply_Type4),
		CREATE_SUPPLY_PDO(5, Src_PDO_Supply_Type5),
		CREATE_SUPPLY_PDO(6, Src_PDO_Supply_Type6),
		CREATE_SUPPLY_PDO(7, Src_PDO_Supply_Type7),
	};

	for (i = 0; i < chip->port.src_pdo_size; ++i) {
		gSrc_caps[i].FPDOSupply.Voltage = chip->port.src_pdo_vol[i] / 50;
		gSrc_caps[i].FPDOSupply.MaxCurrent = chip->port.src_pdo_cur[i] / 10;
		if (i > 0) {
			gSrc_caps[i].FPDOSupply.USBCommCapable = 0;
			gSrc_caps[i].FPDOSupply.DataRoleSwap = 0;
		}
	}

	if (chip->port.src_pps_size) {
		for (i = chip->port.src_pdo_size; i < chip->port.src_pdo_size + chip->port.src_pps_size; ++i) {
			gSrc_caps[i].PPSAPDO.MinVoltage = chip->port.src_pps_vol[(i - chip->port.src_pdo_size) * 2] / 100;
			gSrc_caps[i].PPSAPDO.MaxVoltage = chip->port.src_pps_vol[(i - chip->port.src_pdo_size) * 2 + 1] / 100;
			gSrc_caps[i].PPSAPDO.MaxCurrent = chip->port.src_pps_cur[i - chip->port.src_pdo_size] / 50;
			gSrc_caps[i].PPSAPDO.SupplyType = 3;
		}
	}

	for (i = 0; i < 7; ++i)
		src_caps[i].object = gSrc_caps[i].object;
}
void VIF_InitializeSnkCaps(doDataObject_t *snk_caps)
{
	struct aw35615_chip *chip = aw35615_GetChip();
	AW_U8 i;

	doDataObject_t gSnk_caps[7] = {
		/* macro expects index start at 1 and type */
		CREATE_SINK_PDO(1, Snk_PDO_Supply_Type1),
		CREATE_SINK_PDO(2, Snk_PDO_Supply_Type2),
		CREATE_SINK_PDO(3, Snk_PDO_Supply_Type3),
		CREATE_SINK_PDO(4, Snk_PDO_Supply_Type4),
		CREATE_SINK_PDO(5, Snk_PDO_Supply_Type5),
		CREATE_SINK_PDO(6, Snk_PDO_Supply_Type6),
		CREATE_SINK_PDO(7, Snk_PDO_Supply_Type7),
	};

	for (i = 0; i < chip->port.snk_pdo_size; ++i) {
		gSnk_caps[i].FPDOSink.Voltage = chip->port.snk_pdo_vol[i] / 50;
		gSnk_caps[i].FPDOSink.OperationalCurrent = chip->port.snk_pdo_cur[i] / 10;
		if (i > 0) {
			gSnk_caps[i].FPDOSink.DataRoleSwap = 0;
			gSnk_caps[i].FPDOSink.USBCommCapable = 0;
			gSnk_caps[i].FPDOSink.ExternallyPowered = 0;
			gSnk_caps[i].FPDOSink.HigherCapability = 0;
			gSnk_caps[i].FPDOSink.DualRolePower = 0;
		}
	}

	if (chip->port.snk_pps_size) {
		for (i = chip->port.snk_pdo_size; i < chip->port.snk_pdo_size + chip->port.snk_pps_size; ++i) {
			gSnk_caps[i].PPSAPDO.MinVoltage = chip->port.snk_pps_vol[(i - chip->port.snk_pdo_size) * 2] / 100;
			gSnk_caps[i].PPSAPDO.MaxVoltage = chip->port.snk_pps_vol[(i - chip->port.snk_pdo_size) * 2 + 1] / 100;
			gSnk_caps[i].PPSAPDO.MaxCurrent = chip->port.snk_pps_cur[i - chip->port.snk_pdo_size] / 50;
			gSnk_caps[i].PPSAPDO.SupplyType = 3;
		}
	}

	for (i = 0; i < 7; ++i)
		snk_caps[i].object = gSnk_caps[i].object;
}

