/******************************************************************************
 * @brief    AT命令模拟器
 * Change Logs: 
 * Date           Author       Notes 
 * 2022-04-01     Roger.luo    初版
 ******************************************************************************/
#ifndef __AT_DEVICE_H__
#define __AT_DEVICE_H__

void at_device_init(void);

void at_device_open(void);

void at_device_close(void);

unsigned int at_device_write(const void *buf, unsigned int size);

unsigned int at_device_read(void *buf, unsigned int size);

void at_device_emit_urc(const void *urc, int size);

#endif
