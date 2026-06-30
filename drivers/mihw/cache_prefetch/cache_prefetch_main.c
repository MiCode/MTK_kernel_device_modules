#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/smp.h>
#include <linux/string.h>
#include <asm/sysreg.h>
#include <linux/cpuidle.h>
#include <trace/hooks/cpuidle.h>
#include <asm/barrier.h>

// CACHE PREFETCH REGISTER
#define IMP_CPUECTLR_EL1    sys_reg(3, 0, 15, 1, 4)
#define IMP_CPUECTLR2_EL1    sys_reg(3, 0, 15, 1, 5)

// X4 IMP_CPUECTLR2_EL1 (for BIG and MIDDLE)
#define X4_MASK     0xF      // mask
#define X4_SHIFT    11       // Offset Address
// X4 PF_MODE (0-9)
#define X4_MODE_0     0x0
#define X4_MODE_1     0x1
#define X4_MODE_2     0x2
#define X4_MODE_3     0x3
#define X4_MODE_4     0x4
#define X4_MODE_5     0x5
#define X4_MODE_6     0x6
#define X4_MODE_7     0x7
#define X4_MODE_8     0x8
#define X4_MODE_9     0x9

// A725 IMP_CPUECTLR_EL1 (for SMALL)
#define A725_MASK_DS     0x1      // DS mask
#define A725_SHIFT_DS    5        // DS Offset Address
#define A725_MASK        0x7      // mask
#define A725_SHIFT       2        // Offset Address
// A725 PF_MODE
#define A725_MODE_Dynamic   0x0
#define A725_MODE_Static    0x1
#define A725_MODE_0     0x0
#define A725_MODE_1     0x1
#define A725_MODE_2     0x2
#define A725_MODE_3     0x3
#define A725_MODE_4     0x4
#define A725_MODE_5     0x5
// A725 disable
// #define A725_DISABLE_PREFETCH_SHIFT 21

// X4 mode macro
#define X4_MODE(reg_value, mode) \
    ((reg_value & ~(X4_MASK << X4_SHIFT)) | ((mode)  << X4_SHIFT))

// A725 DS
#define A725_DYNAMIC(reg_value) \
    ((reg_value & ~(A725_MASK_DS << A725_SHIFT_DS)) | (A725_MODE_Dynamic  << A725_SHIFT_DS))
#define A725_STATIC(reg_value) \
    ((reg_value & ~(A725_MASK_DS << A725_SHIFT_DS)) | (A725_MODE_Static  << A725_SHIFT_DS))

// A725 mode
#define A725_MODE(reg_value, mode) \
    ((reg_value & ~(A725_MASK << A725_SHIFT)) | ((mode)  << A725_SHIFT))

// A725 disable
// #define A725_MODE_DISABLE(reg_value, mode) \
//     ((reg_value & ~(A725_MASK_DS << A725_DISABLE_PREFETCH_SHIFT)) | ((mode)  << A725_DISABLE_PREFETCH_SHIFT))

// reg array
static const u64 x4_mode_arrays[] = {
    X4_MODE_0,
    X4_MODE_1,
    X4_MODE_2,
    X4_MODE_3,
    X4_MODE_4,
    X4_MODE_5,
    X4_MODE_6,
    X4_MODE_7,
    X4_MODE_8,
    X4_MODE_9,
};

static const u64 a725_mode_arrays[] = {
    A725_MODE_0,
    A725_MODE_1,
    A725_MODE_2,
    A725_MODE_3,
    A725_MODE_4,
    A725_MODE_5,
};

// sysfs: debug node
static unsigned int DEBUG_NODE = 0;
module_param(DEBUG_NODE, uint, 0644);
static unsigned int Prefetcher_enable = 0;
module_param(Prefetcher_enable, uint, 0644);

// work queue
static struct work_struct config_work;
// static struct work_struct config_work_disable;
// mode count
const int NUM_X4_MODE = 10;
const int NUM_A725_MODE = 6;

// X4 SET MODE FUNC (for BIG and MIDDLE)
static inline u64 SET_X4_MODE(u64 reg_value, int _mode) {
    if (_mode == -1) {
        return reg_value;
    }
    if (_mode < NUM_X4_MODE) {
        return X4_MODE(reg_value, x4_mode_arrays[_mode]);
    }
    return reg_value;
}

// A725 SET MODE FUNC (for SMALL)
static inline u64 SET_A725_MODE(u64 reg_value, int _mode) {
    if (_mode == -1) {
        return reg_value;
    }
    if (_mode == -2) {
        return A725_DYNAMIC(reg_value);
    }
    if (_mode < NUM_A725_MODE) {
        reg_value = A725_STATIC(reg_value);
        return A725_MODE(reg_value, a725_mode_arrays[_mode]);
    }
    return reg_value;
}

// static inline u64 SET_A725_DISABLE_MODE(u64 reg_value, int mode) {
//     reg_value = A725_MODE_DISABLE(reg_value, mode);
//     return reg_value;
// }

// X4/A725 MODE SET FUNC POINTER
typedef struct {
    u64 (*setX4)(u64, int);
    u64 (*setA725)(u64, int);
}RegiserSet;
RegiserSet regiserSet = {
    .setX4 = SET_X4_MODE,
    .setA725 = SET_A725_MODE,
};

// typedef struct {
//     u64 (*setA725Disable)(u64, int);
// }RegiserSetD;
// RegiserSetD regiserSetD = {
//     .setA725Disable = SET_A725_DISABLE_MODE,
// };

// sysfs: X4/A725 mode
enum {
    BIG,
    MIDDLE,
    SMALL,
    MAX,
};

// Global variable
struct cache_prefetch_mode
{
    int big_mode;
    int middle_mode;
    int small_mode;
    // int disable;
}cpm = {
    .big_mode = -1,
    .middle_mode = -1,
    .small_mode = -1,
    // .disable = -1
};

// register read/write
static void write_reg_smp_x4(void *data) {
    u64 *value = data;
    if (unlikely(DEBUG_NODE > 1)) {
        pr_info("write_reg_smp_x4[pre-write]%s, %s, [value] 0x%llx", __FILE__, __func__, *value);
    }
    write_sysreg_s(*value, IMP_CPUECTLR2_EL1);
    isb();
}

static void read_reg_smp_x4(void *data) {
    u64 *value = data;
    if (unlikely(DEBUG_NODE > 1)) {
        pr_info("read_reg_smp_x4[pre-read]%s, %s, [value] 0x%llx", __FILE__, __func__, *value);
    }
    *value = read_sysreg_s(IMP_CPUECTLR2_EL1);
    if (unlikely(DEBUG_NODE > 1)) {
        pr_info("read_reg_smp_x4[read]%s, %s, [value] 0x%llx", __FILE__, __func__, *value);
    }
}

static void write_reg_smp_a725(void *data) {
    u64 *value = data;
    if (unlikely(DEBUG_NODE > 1)) {
        pr_info("write_reg_smp_a725[pre-write]%s, %s, [value] 0x%llx", __FILE__, __func__, *value);
    }
    write_sysreg_s(*value, IMP_CPUECTLR_EL1);
    isb();
}

static void read_reg_smp_a725(void *data) {
    u64 *value = data;
    if (unlikely(DEBUG_NODE > 1)) {
        pr_info("read_reg_smp_a725[pre-read]%s, %s, [value] 0x%llx", __FILE__, __func__, *value);
    }
    *value = read_sysreg_s(IMP_CPUECTLR_EL1);
    if (unlikely(DEBUG_NODE > 1)) {
        pr_info("read_reg_smp_a725[read]%s, %s, [value] 0x%llx", __FILE__, __func__, *value);
    }
}

// static void write_reg_smp_a725_disable(void *data) {
//     u64 *value = data;
//     if (unlikely(DEBUG_NODE > 1)) {
//         pr_info("write_reg_smp_a725_disable[pre-write]%s, %s, [value] 0x%llx", __FILE__, __func__, *value);
//     }
//     write_sysreg_s(*value, IMP_CPUECTLR2_EL1);
// }

// static void read_reg_smp_a725_disable(void *data) {
//     u64 *value = data;
//     if (unlikely(DEBUG_NODE > 1)) {
//         pr_info("read_reg_smp_a725_disable[pre-read]%s, %s, [value] 0x%llx", __FILE__, __func__, *value);
//     }
//     *value = read_sysreg_s(IMP_CPUECTLR2_EL1);
//     if (unlikely(DEBUG_NODE > 1)) {
//         pr_info("read_reg_smread_reg_smp_a725_disable_a725[read]%s, %s, [value] 0x%llx", __FILE__, __func__, *value);
//     }
// }

// show
static void print_all_cpu(void) {
    u64 value[8];
    for (int i = 0; i < 4; i++) {
        smp_call_function_single(i, read_reg_smp_a725, &value[i], 1);
        if (unlikely(DEBUG_NODE == 1)) {
            pr_info("%s, %s, [cpu] %d, [value] 0x%llx", __FILE__, __func__, i, value[i]);
        }
    }
    for (int i = 4; i < 8; i++) {
        smp_call_function_single(i, read_reg_smp_x4, &value[i], 1);
        if (unlikely(DEBUG_NODE == 1)) {
            pr_info("%s, %s, [cpu] %d, [value] 0x%llx", __FILE__, __func__, i, value[i]);
        }
    }
}

static void analysis_and_write_x4_config_mode(int mode, int cpu0, int cpu1) {
    u64 x4_value[8];
    for (int i = cpu0; i < cpu1; i++) {
        smp_call_function_single(i, read_reg_smp_x4, &x4_value[i], 1);
        x4_value[i] = regiserSet.setX4(x4_value[i], mode);
        smp_call_function_single(i, write_reg_smp_x4, &x4_value[i], 1);
    }
}

static void analysis_and_write_a725_config_mode(int mode, int cpu0, int cpu1) {
    u64 a725_value[8];
    for (int i = cpu0; i < cpu1; i++) {
        smp_call_function_single(i, read_reg_smp_a725, &a725_value[i], 1);
        a725_value[i] = regiserSet.setA725(a725_value[i], mode);
        smp_call_function_single(i, write_reg_smp_a725, &a725_value[i], 1);
    }
}

static void analysis_and_write_config_mode(int mode, int cluster) {
    if (cluster == 0) {        // BIG - CPU 7 (X4)
        analysis_and_write_x4_config_mode(mode, 7, 8);
    } else if (cluster == 1) { // MIDDLE - CPUs 4-6 (X4)
        analysis_and_write_x4_config_mode(mode, 4, 7);
    } else {                   // SMALL - CPUs 0-3 (A725)
        analysis_and_write_a725_config_mode(mode, 0, 4);
    }

    if (unlikely(DEBUG_NODE > 1)) {
        print_all_cpu();
    }
}

// static void disable_prefetcher(int disable) {
//     u64 a725_value[8];
//     for (int i = 0; i < 8; i++) {
//         smp_call_function_single(i, read_reg_smp_a725_disable, &a725_value[i], 1);
//         a725_value[i] = regiserSetD.setA725Disable(a725_value[i], disable);
//         smp_call_function_single(i, write_reg_smp_a725_disable, &a725_value[i], 1);
//     }
// }

// work queue
static void config_work_handler(struct work_struct *work)
{
    analysis_and_write_config_mode(cpm.big_mode, BIG);
    analysis_and_write_config_mode(cpm.middle_mode, MIDDLE);
    analysis_and_write_config_mode(cpm.small_mode, SMALL);
}
// static void config_work_disable_handler(struct work_struct *work)
// {
//     disable_prefetcher(cpm.disable);
// }

static void cpu_idle_exit_set(void *unused, int state,
                struct cpuidle_device *dev)
{
    // WFI
    if ((state > 0) && (Prefetcher_enable == 1)) {
        schedule_work(&config_work);
    }
    // if ((state > 0) && (cpm.disable != -1)) {
    //     schedule_work(&config_work_disable);
    // }
}

static int cluster_cache_prefetch_mode_set(const char *val, const struct kernel_param *kp) {
    int cluster, mode;
    const char *param_name = kp->name;

    if (Prefetcher_enable != 1) {
        if (unlikely(DEBUG_NODE == 1)) {
            pr_info("%s, %s, Prefetcher_disable", __FILE__, __func__);
        }
        return -EINVAL;
    }

    if (sscanf(val, "%d", &mode) != 1)
        return -EINVAL;

    if (strcmp(param_name, "BIG_MODE") == 0) {
        cluster = BIG;
        if (mode < -1 || mode >= NUM_X4_MODE)
            return -EINVAL;
        cpm.big_mode = mode;
    } else if (strcmp(param_name, "MIDDLE_MODE") == 0) {
        cluster = MIDDLE;
        if (mode < -1 || mode >= NUM_X4_MODE)
            return -EINVAL;
        cpm.middle_mode = mode;
    } else if (strcmp(param_name, "SMALL_MODE") == 0) {
        cluster = SMALL;
        if (mode < -2 || mode >= NUM_A725_MODE)
            return -EINVAL;
        cpm.small_mode = mode;
    } else {
        return -EINVAL;
    }

    if (unlikely(DEBUG_NODE == 1)) {
        pr_info("%s, %s, [CONFIG_MODE] %d", __FILE__, __func__, mode);
    }

    // analysis config mode and write config mode
    analysis_and_write_config_mode(mode, cluster);

    // print config mode
    if (unlikely(DEBUG_NODE == 1)) {
        print_all_cpu();
    }

    return 0;
}

// static int disable_all_prefetch_set(const char *val, const struct kernel_param *kp) {
//     int disable;
//
//     if (sscanf(val, "%d", &disable) != 1)
//         return -EINVAL;
//
//     if ((disable == 0) || (disable == 1)) {
//         cpm.disable = disable;
//         disable_prefetcher(cpm.disable);
//     } else {
//         return -EINVAL;
//     }
//
//     return 0;
// }

// register fileoperations function
// 4. 定义参数操作结构体
static const struct kernel_param_ops cluster_cache_prefetch_mode = {
    .set = cluster_cache_prefetch_mode_set,
    .get = param_get_int,
};

// static const struct kernel_param_ops disable_all_prefetch = {
//     .set = disable_all_prefetch_set,
//     .get = param_get_int,
// };

// 5. 使用回调方式注册参数
module_param_cb(BIG_MODE, &cluster_cache_prefetch_mode, &cpm.big_mode, 0644);
MODULE_PARM_DESC(BIG_MODE, "BIG Cache Prefetch mode (0-9,-1). Default: -1");
module_param_cb(MIDDLE_MODE, &cluster_cache_prefetch_mode, &cpm.middle_mode, 0644);
MODULE_PARM_DESC(MIDDLE_MODE, "MIDDLE Cache Prefetch mode (0-9,-1). Default: -1");
module_param_cb(SMALL_MODE, &cluster_cache_prefetch_mode, &cpm.small_mode, 0644);
MODULE_PARM_DESC(SMALL_MODE, "SMALL Cache Prefetch mode (0,1,2,3,4,5,-2,-1). Default: -1");
// module_param_cb(DISABLE_PREFETCH, &disable_all_prefetch, &cpm.disable, 0644);
// MODULE_PARM_DESC(DISABLE_PREFETCH, "DISABLE PREFETCH mode (0,1). Default: 0");

static int __init sysreg_debugfs_init(void) {
    int ret;
    // to be used;
    ret = 0;
    INIT_WORK(&config_work, config_work_handler);
    // INIT_WORK(&config_work_disable, config_work_disable_handler);
    register_trace_android_vh_cpu_idle_exit(cpu_idle_exit_set, NULL);
    return ret;
}

static void __exit sysreg_debugfs_exit(void) {
    unregister_trace_android_vh_cpu_idle_exit(cpu_idle_exit_set, NULL);
}

module_init(sysreg_debugfs_init);
module_exit(sysreg_debugfs_exit);
MODULE_LICENSE("GPL");