# Copyright (C) 2024 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/performance/game/include \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/performance/game/engine_cooler

mtk_game_objs += game.o \
	game_base.o \
	engine_cooler/game_ec.o

obj-$(CONFIG_MTK_GAME) += mtk_game.o
mtk_game-objs += $(mtk_game_objs)
