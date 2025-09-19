/****************************************************************************************
 *
 * @File Name   : lct_tp_info.h
 * @Author      : wanghan
 * @E-mail      : <wanghan@longcheer.com>
 * @Create Time : 2018-08-17 17:34:43
 * @Description : Display touchpad information.
 *
 ****************************************************************************************/

#ifndef __LCT_TP_INFO_H__
#define __LCT_TP_INFO_H__

#define TP_CALLBACK_CMD_INFO "CMD_INFO"
#define TP_CALLBACK_CMD_LOCKDOWN "CMD_LOCKDOWN"

// init /proc/tp_info node
extern int init_lct_tp_info(char *tp_info_buf);
// uninit /proc/tp_info node
extern void uninit_lct_tp_info(void);
// Update /proc/tp_info node
extern void update_lct_tp_info(char *tp_info_buf);
// Set tp_info node callback funcation
extern void set_lct_tp_info_callback(int (*pfun)(const char *));

// Set tp_lockdown_info node callback funcation
extern void set_lct_tp_lockdown_info_callback(int (*pfun)(void));
// Update /proc/tp_lockdown_info node
extern void update_lct_tp_lockdown_info(char *tp_lockdown_info_buf);
// init /proc/tp_lockdown_info node
extern int init_lct_tp_lockdown_info(void);
// uninit /proc/tp_lockdown_info node
extern void uninit_lct_tp_lockdown_info(void);
extern char touch_lcm_name[64];
extern void lct_tp_get_panel_info(void);
#endif //__LCT_TP_INFO_H__
