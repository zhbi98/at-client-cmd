/******************************************************************************
 * @brief    wifi任务(AT-command演示, 使用的模组是M169WI-FI)
 *
 * Copyright (c) 2020, <morro_luo@163.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs: 
 * Date           Author       Notes 
 * 2021-01-20     roger.luo  初版
 * 2021-03-03     roger.luo  增加URC使用案例
 ******************************************************************************/
#include "at_chat.h"
#include "wifi_uart.h"
#include "public.h"
#include "module.h"
#include "cli.h"
#include <stdio.h>
#include <stdbool.h>

/* Private function prototypes -----------------------------------------------*/
void wifi_open(void);
void wifi_close(void);
static void at_error(at_response_t *);
static void at_debug(const char *fmt, ...);
void wifi_query_version(void);
int wifi_ready_handler(at_urc_info_t *info);
int wifi_connected_handler(at_urc_info_t *info);
int wifi_disconnected_handler(at_urc_info_t *info);

/* Private variables ---------------------------------------------------------*/
/**
 * @brief   定义AT控制器
 */
static at_obj_t *at_obj;

/**
 * @brief   wifi URC表
 */
static const urc_item_t urc_table[] = {
    "ready",'\n',             wifi_ready_handler,
    "WIFI CONNECTED:", '\n',   wifi_connected_handler,
    "WIFI DISCONNECTED", '\n', wifi_disconnected_handler,
};

/** 
 * @brief   AT适配器
 */
static const at_adapter_t  at_adapter = {
    .write         = wifi_uart_write,
    .read          = wifi_uart_read,
    .error         = at_error,
    .debug         = at_debug,
    .urc_bufsize   = 128,
    .recv_bufsize  = 256
};

/* Private functions ---------------------------------------------------------*/

/* 
 * @brief   wifi开机就绪事件
 */
static int wifi_ready_handler(at_urc_info_t *info)
{
    printf("WIFI ready...\r\n");
    return 0;
}

/**
 * @brief   wifi连接事件
 */
static int wifi_connected_handler(at_urc_info_t *info)
{
    printf("WIFI connection detected...\r\n");
    return 0;
}
/* 
 * @brief   wifi断开连接事件
 */
static int wifi_disconnected_handler(at_urc_info_t *info)
{
    printf("WIFI disconnect detected...\r\n");
    return 0;
}

/* 
 * @brief   打开wifi
 */
void wifi_open(void)
{
    GPIO_SetBits(GPIOA, GPIO_Pin_4);
    printf("wifi open\r\n");
}
/* 
 * @brief   关闭wifi
 */
void wifi_close(void)
{
    GPIO_ResetBits(GPIOA, GPIO_Pin_4);
    printf("wifi close\r\n");
}


/* 
 * @brief   WIFI重启任务状态机
 * @return  true - 退出状态机, false - 保持状态机,
 */
static int wifi_reset_work(at_env_t *e)
{
    switch (e->state) {
    case 0:                                //关闭WIFI电源
        wifi_close();
        e->reset_timer(e);
        e->state++;
        break;
    case 1:
        if (e->is_timeout(e, 2000))       //延时等待2s
            e->state++;
        break;
    case 2:
        wifi_open();                       //重启启动wifi
        e->state++;
        break;
    case 3:
        if (e->is_timeout(e, 3000))       //大约延时等待3s至wifi启动
            return true;  
        break;
    }
    return false;
}
/**
 * @brief 打印输出
 */
static void at_debug(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
/**
 * @brief   wifi 通信异常处理
 */
static void at_error(at_response_t *r)
{
    printf("wifi AT communication error\r\n");
    //执行重启作业
    at_do_work(at_obj, NULL, wifi_reset_work);        
}

/* 
 * @brief    初始化回调
 */
static void at_init_callbatk(at_response_t *r)
{    
    if (r->code == AT_RESP_OK ) {
        printf("wifi Initialization successfully...\r\n");
        
        /* 查询版本号*/
        wifi_query_version();
        
    } else 
        printf("wifi Initialization failure...\r\n");
}

/* 
 * @brief    wifi初始化命令表
 */
static const char *wifi_init_cmds[] = {
    "AT+GPIO_WR=1,1",
    "AT+GPIO_WR=2,0",
    "AT+GPIO_WR=3,1",
    NULL
};


/* 
 * @brief    wifi初始化
 */
void wifi_init(void)
{
    at_attr_t attr;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA , ENABLE);
    gpio_conf(GPIOA, GPIO_Mode_OUT, GPIO_PuPd_NOPULL, GPIO_Pin_4);
    
    wifi_uart_init(115200);
    //创建AT控制器
    at_obj = at_obj_create(&at_adapter);
    //设置URC表
    at_obj_set_urc(at_obj, urc_table, sizeof(urc_table) / sizeof(urc_table[0]));         
    //启动WIFI
    at_do_work(at_obj, NULL, wifi_reset_work);        
    
    //初始化wifi
    at_attr_deinit(&attr);
    attr.cb = at_init_callbatk;
    at_send_multiline(at_obj, &attr, wifi_init_cmds);  
    
    //GPIO测试
    at_send_singlline(at_obj, &attr, "AT+GPIO_TEST_EN=1\r\n");  
    
}driver_init("wifi", wifi_init); 



/* 
 * @brief    wifi任务(5ms 轮询1次)
 */
void wifi_task(void)
{
    at_obj_process(at_obj);
} task_register("wifi", wifi_task, 0);


/** 非标准AT例子----------------------------------------------------------------
 *  以查询版本号为例:
 * ->  AT+VER\r\n
 * <-  VERSION:M169-YH01
 *  
 */

//方式1, 使用at_do_cmd接口

/* 
 * @brief    自定义AT发送器
 */
static void at_ver_sender(at_env_t *e)
{
    e->println(e, "AT+VER");
}
/* 
 * @brief    版本查询回调
 */
static void query_version_callback(at_response_t *r)
{
    if (r->code == AT_RESP_OK ) {
        printf("wifi version info : %s\r\n", r->prefix);
    } else 
        printf("wifi version query failure...\r\n");
}

/* 
 * @brief    执行版本查询命令
 */
void wifi_query_version(void)
{
    at_attr_t attr;
    at_attr_deinit(&attr);
    attr.cb     = query_version_callback;    
    attr.prefix = "VERSION:",                //响应前缀
    attr.suffix = "\n";                      //响应后缀
    at_custom_cmd(at_obj, &attr, at_ver_sender);    
    //at_exec_cmd(at_obj, &attr, "AT+VER");
}
