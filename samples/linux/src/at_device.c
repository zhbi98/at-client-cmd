/******************************************************************************
 * @brief    AT命令模拟器
 * Change Logs: 
 * Date           Author       Notes 
 * 2022-04-01     Roger.luo    初版
 ******************************************************************************/
#include "cli.h"
#include "at_device.h"
#include "ringbuffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <pthread.h>

/**
 * @brief AT设备
 */
typedef struct {
    cli_obj_t cli;
    ring_buf_t    rb_tx; 
    ring_buf_t    rb_rx; 
    unsigned char txbuf[1024];
    unsigned char rxbuf[512];
} at_device_t;

static at_device_t at_device;

/**
 * @brief 数据写操作
 */
static unsigned int cli_write(const void *buf, unsigned int size)
{
    return ring_buf_put(&at_device.rb_tx, (unsigned char *)buf, size);
}
/**
 * @brief 数据读操作
 */
static unsigned int cli_read(void *buf, unsigned int size)
{
    return ring_buf_get(&at_device.rb_rx, buf, size);
}

/** 
 * @brief 命令处理任务
 */
static void *at_device_thread(void *args)
{
    pthread_detach(pthread_self());
    printf("at devicce running...\r\n");    
    while(1) {
        usleep(1000);
        cli_process(&at_device.cli);
    }
    return NULL;
}

/**
 * @brief AT模拟器初始化()
 */
void at_device_init(void)
{

    cli_port_t p = {cli_write, cli_read, NULL};     /*读写接口 */
    /*初始化环形缓冲区 */
    ring_buf_init(&at_device.rb_rx, at_device.rxbuf, sizeof(at_device.rxbuf));
    ring_buf_init(&at_device.rb_tx, at_device.txbuf, sizeof(at_device.txbuf));

    cli_init(&at_device.cli, &p);                   /*初始化命令行对象 */
    cli_enable(&at_device.cli);
    pthread_t tid;    
    pthread_create(&tid, NULL, at_device_thread, NULL); 
}


/**
 * @brief 数据写操作
 * @param buf  数据缓冲区
 * @param size 缓冲区长度
 * @retval 实际写入数据
 */
unsigned int at_device_write(const void *buf, unsigned int size)
{
    return ring_buf_put(&at_device.rb_rx, (unsigned char *)buf, size);
}
/**
 * @brief 数据读操作
 * @param buf  数据缓冲区
 * @param size 缓冲区长度
 * @retval 实际读到的数据
 */
unsigned int at_device_read(void *buf, unsigned int size)
{
    return ring_buf_get(&at_device.rb_tx, buf, size);
}

/**
 * @brief 触发生成URC消息
 */
void at_device_emit_urc(const void *urc, int size)
{
    cli_write(urc, size);
}

/**
 * @brief 打开AT设备(将触发 "+POWER:1" URC消息)
 */
void at_device_open(void)
{
    char *s = "+POWER:1\r\n";
    at_device_emit_urc(s, strlen(s));
}

/**
 * @brief 关闭AT设备(将触发 "+POWER:0" URC消息)
 */
void at_device_close(void)
{
    char *s = "+POWER:0\r\n";
    at_device_emit_urc(s, strlen(s));
}
