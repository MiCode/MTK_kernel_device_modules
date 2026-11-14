#include "../../include/mi_ufshcd.h"
#include "ufscld.h"
#include <linux/delay.h>
#include "hid.h"
#include "../../core/mi_ufsctx.h"

int hid_get_frag_level(struct ufscld_dev *cld, int *frag_level)
{
    struct ufs_hba *hba = cld->hba;
    int dHIDDefragOperation = 0;
    int bHIDState = 0;
    int ret = 0;
    *frag_level = CLD_LEV_NA;
    ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
                                (enum attr_idn)QUERY_ATTR_IDN_HID_STATE, 0, 0, &bHIDState);
    if (ret) {
        ERR_MSG("Get bHIDState fail, ret=%d\n", ret);
        return ret;
    }
    INFO_MSG("Get bHIDState=%d\n", bHIDState);
#if 1
    if ((bHIDState != ANALYSIS_PROGRESSING) && (bHIDState != DEFRAG_PROGRESSING)) {
        dHIDDefragOperation = ANALYSIS_ENABLE_DEFRAG_DISABLE;
        INFO_MSG("Set dHIDDefragOperation=%d\n", dHIDDefragOperation);
        ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
                                    (enum attr_idn)QUERY_ATTR_IDN_HID_DEFRAG_OPERATION, 0, 0,
                                    &dHIDDefragOperation);
        if (ret) {
            ERR_MSG("Set dHIDDefragOperation fail, ret=%d\n", ret);
            return ret;
        }
        msleep(200);
        bHIDState = -1;
        ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
                                    (enum attr_idn)QUERY_ATTR_IDN_HID_STATE, 0, 0, &bHIDState);
        if (ret) {
            ERR_MSG("Get bHIDState fail, ret=%d\n", ret);
            return ret;
        }
    }
    INFO_MSG("Get bHIDState=%d\n", bHIDState);
#endif
    if ((bHIDState == DEFRAG_REQUIRE) || (bHIDState == DEFRAG_PROGRESSING)) {
        *frag_level = CLD_LEV_CRITICAL;
    } else if (bHIDState == ANALYSIS_PROGRESSING) {
        *frag_level = CLD_LEV_CLEAN;
    } else {
        *frag_level = CLD_LEV_NA;
    }
    return ret;
}
int hid_set_trigger(struct ufscld_dev *cld, u32 trigger)
{
    struct ufs_hba *hba = cld->hba;
    int dHIDDefragOperation = 0;
    int dHIDAvailableSize = 0;
    int dHIDSize = 0;
    int bHIDState = 0;
    int ret = 0;
    ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
                                (enum attr_idn)QUERY_ATTR_IDN_HID_DEFRAG_OPERATION, 0, 0,
                                &dHIDDefragOperation);
    if (ret) {
        ERR_MSG("Set dHIDDefragOperation fail, ret=%d\n", ret);
        return ret;
    }
    INFO_MSG("Get dHIDDefragOperation=%d\n", dHIDDefragOperation);
    if (trigger) {
        if (dHIDDefragOperation != ANALYSIS_DEFRAG_ENABLE) {
            if (dHIDDefragOperation != ANALYSIS_ENABLE_DEFRAG_DISABLE) {
                dHIDDefragOperation = ANALYSIS_ENABLE_DEFRAG_DISABLE;
                ERR_MSG("Set dHIDDefragOperation=%d\n", dHIDDefragOperation);
                ret =
                    ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
                                            (enum attr_idn)QUERY_ATTR_IDN_HID_DEFRAG_OPERATION, 0,
                                            0, &dHIDDefragOperation);
                if (ret) {
                    ERR_MSG("Set HIDDefragOperation fail, ret=%d\n", ret);
                    return ret;
                }
            }
            msleep(200);
            bHIDState = -1;
            ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
                                        (enum attr_idn)QUERY_ATTR_IDN_HID_STATE, 0, 0, &bHIDState);
            if (ret) {
                ERR_MSG("Get bHIDState fail, ret=%d\n", ret);
                return ret;
            }
            INFO_MSG("Get bHIDState=%d\n", bHIDState);
            if (bHIDState == DEFRAG_REQUIRE) {
                dHIDAvailableSize = 0;
                ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
                                            (enum attr_idn)QUERY_ATTR_IDN_HID_AVAILABLE_SIZE, 0, 0,
                                            &dHIDAvailableSize);
                if (ret) {
                    ERR_MSG("Get dHIDAvailableSize failed ret=%d\n", ret);
                    return ret;
                }
#if 1
                dHIDSize = dHIDAvailableSize;
                INFO_MSG("Set dHIDSize=0x%x\n", dHIDSize);
                ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
                                            (enum attr_idn)QUERY_ATTR_IDN_HID_SIZE, 0, 0,
                                            &dHIDAvailableSize);
                if (ret) {
                    ERR_MSG("Set dHIDSize fail, ret=%d\n", ret);
                    return ret;
                }
#endif
                dHIDDefragOperation = ANALYSIS_DEFRAG_ENABLE;
                INFO_MSG("Set dHIDDefragOperation=%d\n", dHIDDefragOperation);
                ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
                                            (enum attr_idn)QUERY_ATTR_IDN_HID_DEFRAG_OPERATION, 0,
                                            0, &dHIDDefragOperation);
                if (ret) {
                    ERR_MSG("Set HIDDefragOperation fail, ret=%d\n", ret);
                    return ret;
                }
            } else {
                ERR_MSG("Not need set HIDDefragOperation, there is not frag!\n");
            }
        }
    } else {
        dHIDDefragOperation = ANALYSIS_DEFRAG_DISABLE;
        INFO_MSG("Set dHIDDefragOperation=%d\n", dHIDDefragOperation);
        ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
                                    (enum attr_idn)QUERY_ATTR_IDN_HID_DEFRAG_OPERATION, 0, 0,
                                    &dHIDDefragOperation);
        if (ret) {
            ERR_MSG("Set HIDDefragOperation fail, ret=%d\n", ret);
            return ret;
        }
    }
    return ret;
}
int hid_get_trigger(struct ufscld_dev *cld, u32 * trigger)
{
    struct ufs_hba *hba = cld->hba;
    int dHIDDefragOperation = 0;
    int ret = 0;
    ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
                                (enum attr_idn)QUERY_ATTR_IDN_HID_DEFRAG_OPERATION, 0, 0,
                                &dHIDDefragOperation);
    if (ret) {
        ERR_MSG("Set HIDDefragOperation fail, ret=%d\n", ret);
        return ret;
    }
    ERR_MSG("Get dHIDDefragOperation is 0x%x\n", dHIDDefragOperation);
    *trigger = dHIDDefragOperation;
    return ret;
}
int hid_operation_status(struct ufscld_dev *cld, int *op_status)
{
    int ret = 0;
    //int dHIDDefragOperation = 0;
    //int dHIDAvailableSize = 0;
    int dHIDProgressRatio = -1;
    int bHIDState = 0;
    struct ufs_hba *hba = cld->hba;
    *op_status = CLD_STATUS_NA;
#if 1
    if (cld->cld_trigger) {
        dHIDProgressRatio = -1;
        ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
                                    (enum attr_idn)QUERY_ATTR_IDN_HID_PROGRESS_RATIO, 0, 0,
                                    &dHIDProgressRatio);
        if (ret) {
            ERR_MSG("Get dHIDProgressRatio failed ret=%d\n", ret);
            return ret;
        }
        INFO_MSG("Get dHIDProgressRatio=%d\n", dHIDProgressRatio);
        bHIDState = -1;
        ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
                                    (enum attr_idn)QUERY_ATTR_IDN_HID_STATE, 0, 0, &bHIDState);
        if (ret) {
            ERR_MSG("Get bHIDState failed ret=%d\n", ret);
        }
        INFO_MSG("Get bHIDState=%d\n", bHIDState);
        if (dHIDProgressRatio == 100 || dHIDProgressRatio == 0) {
            if (bHIDState == DEFRAG_COMPLETED || bHIDState == DEFRAG_NOT_REQUIRE || bHIDState == ANALYSIS_DEFRAG_IDLE)
                *op_status = CLD_STATUS_IDLE;
            else
                *op_status = CLD_STATUS_PROGRESSING;
        } else if (dHIDProgressRatio > 0) {
            if (bHIDState == ANALYSIS_PROGRESSING || bHIDState == DEFRAG_PROGRESSING)
                *op_status = CLD_STATUS_PROGRESSING;
            else
                *op_status = CLD_STATUS_INTERRUPT_BY_HOST;
        } else {
            *op_status = CLD_STATUS_IDLE;
        }
    } else {
        *op_status = CLD_STATUS_IDLE;
    }
#else
    dHIDDefragOperation = 0;
    ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
                                (enum attr_idn)QUERY_ATTR_IDN_HID_DEFRAG_OPERATION, 0, 0,
                                &dHIDDefragOperation);
    if (ret) {
        ERR_MSG("Get dHIDAvailableSize failed ret=%d\n", ret);
        return ret;
    }
    INFO_MSG("Get dHIDDefragOperation=%d\n", dHIDDefragOperation);
    dHIDAvailableSize = 0;
    ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
                                (enum attr_idn)QUERY_ATTR_IDN_HID_AVAILABLE_SIZE, 0, 0,
                                &dHIDAvailableSize);
    if (ret) {
        ERR_MSG("Get dHIDAvailableSize failed ret=%d\n", ret);
        return ret;
    }
    INFO_MSG("Get dHIDAvailableSize=0x%x\n", dHIDAvailableSize);
    if (dHIDAvailableSize == ANALYSIS_REQUIRE && dHIDDefragOperation == ANALYSIS_DEFRAG_DISABLE) {
        dHIDDefragOperation = ANALYSIS_ENABLE_DEFRAG_DISABLE;
        ERR_MSG("Set dHIDDefragOperation=%d\n", dHIDDefragOperation);
        ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
                                    (enum attr_idn)QUERY_ATTR_IDN_HID_DEFRAG_OPERATION, 0, 0,
                                    &dHIDDefragOperation);
        if (ret) {
            ERR_MSG("Set HIDDefragOperation fail, ret=%d\n", ret);
            return ret;
        }
        msleep(200);
    }
    dHIDProgressRatio = -1;
    ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
                                (enum attr_idn)QUERY_ATTR_IDN_HID_PROGRESS_RATIO, 0, 0,
                                &dHIDProgressRatio);
    if (ret) {
        ERR_MSG("Get dHIDProgressRatio failed ret=%d\n", ret);
        return ret;
    }
    INFO_MSG("Get dHIDProgressRatio=%d\n", dHIDProgressRatio);
    ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
                                (enum attr_idn)QUERY_ATTR_IDN_HID_STATE, 0, 0, &bHIDState);
    if (ret) {
        ERR_MSG("Get bHIDState failed ret=%d\n", ret);
    }
    INFO_MSG("Get bHIDState=%d\n", bHIDState);
    if (cld->cld_trigger) {
        if (dHIDProgressRatio == 100) {
            if (bHIDState == DEFRAG_NOT_REQUIRE || bHIDState == DEFRAG_COMPLETED
                || bHIDState == ANALYSIS_DEFRAG_IDLE) {
                *op_status = CLD_STATUS_IDLE;
            } else if (bHIDState == ANALYSIS_PROGRESSING || bHIDState == DEFRAG_PROGRESSING
                       || bHIDState == DEFRAG_REQUIRE) {
                *op_status = CLD_STATUS_PROGRESSING;
            } else {
                *op_status = CLD_STATUS_NA;
            }
        } else {
            if (bHIDState == DEFRAG_NOT_REQUIRE || bHIDState == DEFRAG_COMPLETED
                || bHIDState == ANALYSIS_DEFRAG_IDLE) {
                *op_status = CLD_STATUS_IDLE;
            } else if (bHIDState == DEFRAG_REQUIRE) {
                *op_status = CLD_STATUS_INTERRUPT_BY_HOST;
            } else {
                *op_status = CLD_STATUS_PROGRESSING;
            }
        }
    } else {
        *op_status = CLD_STATUS_IDLE;
    }
#endif
    INFO_MSG("op_status=%d\n", *op_status);
    return ret;
}
struct ufscld_ops hid_ops = {
    .cld_get_frag_level = hid_get_frag_level,
    .cld_set_trigger = hid_set_trigger,
    .cld_get_trigger = hid_get_trigger,
    .cld_operation_status = hid_operation_status
};
