/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MVPU3_CMD_DATA_H__
#define __MVPU3_CMD_DATA_H__

#define MVPU_PE_NUM  64
#define MVPU_DUP_BUF_SIZE  (2 * MVPU_PE_NUM)

#define MVPU_REQUEST_NAME_SIZE 32
#define MVPU_MPU_SEGMENT_NUMS  39

#define MVPU_MIN_CMDBUF_NUM     2
#define MVPU_CMD_INFO_IDX       0
#define MVPU_CMD_KREG_BASE_IDX  1

#define MVPU_CMD_LITE_SIZE_0 0x12E
#define MVPU_CMD_LITE_SIZE_1 0x14A

#ifndef MVPU_SECURITY
#define MVPU_SECURITY
#endif

#ifdef MVPU_SECURITY
#define BUF_NUM_MASK     0x0000FFFF

#define KERARG_NUM_MASK  0x3FFF0000
#define KERARG_NUM_SHIFT 16

#define SEC_LEVEL_MASK   0xC0000000
#define SEC_LEVEL_SHIFT  30

enum MVPU_SEC_LEVEL {
	SEC_LVL_CHECK = 0,
	SEC_LVL_CHECK_ALL = 1,
	SEC_LVL_PROTECT = 2,
	SEC_LVL_END,
};
#endif

struct BundleHeader {
    union /* 0x0000,  */
    {
        unsigned int dwValue;     /* treg_bdh_w0 */
        struct
        {
            unsigned int treg_bdh_w0          : 32; /* (16-Byte Alignment)
                                                       bit[31:  0] bdh_bd_kreg_base_addr : bundle's 1st (kernel ctrlreg) base addr */
                                                    /* default: 32'h0 */
        } s;
    }reg_treg_bdh_w0;

    union /* 0x0004,  */
    {
        unsigned int dwValue;     /* treg_bdh_w1 */
        struct
        {
            unsigned int treg_bdh_w1          : 32; /* bit[15:  0] bdh_task_instance_id
                                                       bit[31:16] bdh_graph_instance_id */
                                                    /* default: 32'h0 */
        } s;
    }reg_treg_bdh_w1;

    union /* 0x0008,  */
    {
        unsigned int dwValue;     /* treg_bdh_w2 */
        struct
        {
            unsigned int treg_bdh_w2          : 32; /* bit[  1:  0] bdh_preempt_en_md :  preempt enable mode
                                                                         2'h0 : not preempted bundle, should wait it all done  reserved
                                                                         2'h1 : kerenl level preempted bundle
                                                                         2'h2 : instruction level preempted bundle
                                                                         2'h3 : reserved
                                                       bit[  3:  2] bdh_preempt_resume_md :  Indicate if this bundle is resumed bundle
                                                                         2'h0 : not resumed bundle
                                                                         2'h1 : resumed from kernel level preemption
                                                                         2'h2 :  resumed from instruction level preemption
                                                                         2'h3 :  reserved
                                                       bit[  6:  4] bdh_preempt_resume_bdc_id : resumed boundle's original bundle container id
                                                       bit[10:  7] bdh_preempt_resume_skip_dma_num :
                                                                         Skip dma kernel number, DPCH would skip any dma action(set event/wait event/enque dma)
                                                                         from (kernel_id=kreg_knl_start_cnt) to (kernel_id=kreg_knl_start_cnt+bdh_bd_skip_dma_num)
                                                                         [Note!!] Please note this is only for SW preemption usage, other usage should keep it to be zero.
                                                                         [Behavior] When bundle start 1st kernel, DPCH would latch the treg_skip_dma_num setting.
                                                                                             In dma stage, if skip_dma_num!=0, dpch would skip any dma action.
                                                                                             Once the dma stage returns to idle, the skip_dma_num=skip_dma_num-1 until it is zero
                                                       bit[14:11] bdh_preempt_resume_skip_instr_pld_num :   (i.e. bdh_preempt_resume_skip_hse_update_num")
                                                       bit[15] bdh_bd_done_intp_en :  1: enable output interrupt to uP when bundle done
                                                       bit[31:16] bdh_bd_knl_num : kernel number of bundle
                                                        */
                                                    /* default: 32'h0 */
        } s;
    }reg_treg_bdh_w2;

    union /* 0x000c,  */
    {
        unsigned int dwValue;     /* treg_bdh_w3 */
        struct
        {
            unsigned int treg_bdh_w3          : 32; /* bit[31:  0] bdh_glsu_desc_ctbl_idx_base :
                                                       GLSU Table Index Buffer Base (for descriptor index access usage) :   GLSU read addr value from it to alter the source/destination addr of GLSU's descriptors. */
                                                    /* default: 32'h0 */
        } s;
    }reg_treg_bdh_w3;

    union /* 0x0010,  */
    {
        unsigned int dwValue;     /* treg_bdh_w4 */
        struct
        {
            unsigned int treg_bdh_w4          : 32; /* bit[31:  0] bdh_kregpatch_sw_buf_base */
                                                    /* default: 32'h0 */
        } s;
    }reg_treg_bdh_w4;

    union /* 0x0014,  */
    {
        unsigned int dwValue;     /* treg_bdh_w5 */
        struct
        {
            unsigned int treg_bdh_w5          : 32; /* bit[1:0] bdh_resource_virtualization_mode :
                                                                        2'h0 : manual_ep (SW select target exepipe which is set in sw_specify_exepipe[1:0])
                                                                        2'h1 : manual_cr   (SW select target core        which is set in sw_specify_exepipe[1]), then HW select the core's "free and smallest_id" exepipe. reserved
                                                                        2'h2 : reserved
                                                                        2'h3 : auto (urgent_bundle : auto --> ONLY used for preemption_target_mode), (normal_bundle : reserved)
                                                       bit[3:2] bdh_sw_specify_exepipe :  (ONLY used in "resource_virtualization_mode : manual_ep, manual_cr" and "preemption target mode : manual")
                                                                                                                      (manual_cr ONLY use sw_specify_exepipe[1])
                                                                        2'h0 : Exepipe0 (Core 0)
                                                                        2'h1 : Exepipe1 (Core 0)
                                                                        2'h2 : Exepipe2 (Core 1)
                                                                        2'h3 : Exepipe3 (Core 1)
                                                       bit[5:4] bdh_used_l1m :
                                                                        2'h0 : 0% reserved
                                                                        2'h1 : 50%   (Exepipe0/2 : 0~50% in Core0/1 ), (Exepipe1/3 : 50~100% in Core 0/1);
                                                                        2'h2 : 100%
                                                                        2'h3 : reserved
                                                       bit[7:6] bdh_priority : 2'h0 < 1 < 2 < 3(highest level);
                                                       bit[11:8] bdh_bdc_sel : (4'h0~7,8~b, c~d) : bundle_container n0~n7, u0~u3, i0~i1 ;
                                                                         [Note]  (bdc n0~n7) : normal bdc
                                                                                      (bdc u0~u3) : urgent bdc
                                                                                      (bdc i0~i1) :   internal-used bdc, ONLY for (Preemption's resume bd / While-OP bd) dispatched by (Core0/RV55 or Core1/RV55 respectively) via OPC.
                                                       bit[12] reserved
                                                       bit[13] bdh_other_core_access_en, (1 : allow current core access the other core's memory (itcm,dtcm,l1m) in local-addr-view)
                                                                    [Note] to 2 destinations via DAG : (1) to GLSU for local-to-local path access; (2) to GLSU.AXI_S for global-to-local path access;
                                                       bit[15:14] : reserved
                                                       bit[31:16] bdh_knl_start_cnt : starting kernel count of this bundle
                                                        */
                                                    /* default: 32'h0 */
        } s;
    }reg_treg_bdh_w5;

    union /* 0x0018,  */
    {
        unsigned int dwValue;     /* treg_bdh_w6 */
        struct
        {
            unsigned int treg_bdh_w6          : 32; /* (16-Byte Alignment)
                                                       bit[31:  0] bdh_mpu_dram_info_base : [MPU] mpu dram info base address */
                                                    /* default: 32'h0 */
        } s;
    }reg_treg_bdh_w6;

    union /* 0x001c,  */
    {
        unsigned int dwValue;     /* treg_bdh_w7 */
        struct
        {
            unsigned int treg_bdh_w7          : 32; /* bit[ 7: 0] bdh_mpu_dram_info_seg_num : [MPU] DRAM info total segment number (0: do nothing) (1~255)
                                                       bit[ 9: 8] bdh_mpu_gen_mode : [MPU] 2'h0 : auto mode, MPU HW will fetch buffer information from DRAM, and then it will perform HW sort and merge.
                                                                                                                            2'h1 : half auto mode, MPU HW will fetch buffer information from DRAM (will not perform HW sort and merge).
                                                                                                                            others : reserved.
                                                       bit[17:10] reserved
                                                       bit[18] bdh_intp_rv55_before_exepipe :
                                                                        1'h1 : fire intp to rv55 before bundle start in exepipe. (rv55 superloop can do something before entering exepipe, e.g.:  modify  APB cmd table)
                                                       bit[19]      bdh_4g_sel_en :  (1: use "bdh_4g_sel"), (0 : use "cfg_4g_sel" from mvpu_top_config)
                                                       bit[23:20] bdh_4g_sel :  these 4 bits are the bit[35:32] of 36-bit address.
                                                       bit[27:24] bdh_bd_header_type,  (big_bd : 4'b0001)
                                                       bit[30:28] bdh_bd_context_id : valid value for bundle header : 3'h0~5;
                                                                                                              [Note] : 3'h6,7: reserved for core0/1 MPU, PMU used ONLY!

                                                       bit[31]      bundle fire  (HW take it & write_1T_pulse to use)
                                                       (reserved address space for bundle header : 0x0020 ~ 0x003c) */
                                                    /* default: 32'h0 */
        } s;
    }reg_treg_bdh_w7;
};

#endif /* __MVPU3_CMD_DATA_H__ */
