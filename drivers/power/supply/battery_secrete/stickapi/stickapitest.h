#ifndef __STICKAPITEST_H
#define __STICKAPITEST_H

void stickapitest_doTest_sequence(void);

stick_ReturnCode_t stickapitest_set_ui_soh(u8 *ui_soh_data, int len, int raw_soh);
stick_ReturnCode_t stickapitest_get_ui_soh(u8 *ui_soh_data, int len);

#endif
