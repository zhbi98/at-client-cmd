/******************************************************************************
 * @brief    wifi串口通信驱动
 *
 * Copyright (c) 2021, <morro_luo@163.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs: 
 * Date           Author       Notes 
 * 2021-01-20     roger.luo  初始版本
 ******************************************************************************/

#ifndef	_WIFI_UART_H_
#define	_WIFI_UART_H_

#define WIFI_RXBUF_SIZE		 256
#define WIFI_TXBUF_SIZE		 1024

/*接口声明 --------------------------------------------------------------------*/
void wifi_uart_init(int baud_rate);

unsigned int wifi_uart_write(const void *buf, unsigned int len); 

unsigned int wifi_uart_read(void *buf, unsigned int len);   

bool wifi_uart_rx_empty(void);                                  

#endif
