/******************************************************************************
 * @brief        AT指令通信管理
 *
 * Copyright (c) 2020~2021, <morro_luo@163.com>
 *
 * SPDX-License-Identifier: Apathe-2.0
 *
 * Change Logs: 
 * Date           Author       Notes 
 * 2020-01-02     Morro        初版
 * 2021-01-20     Morro        增加debug调试接口, 解决链表未初始化导至段错误的问题
 *                             调通单行命令、多行命令、自定义命令等接口
 * 2021-03-03     Morro        解决未清除URC计数器导致频繁打印接收超时的问题
 ******************************************************************************/

#ifndef _ATCHAT_H_
#define _ATCHAT_H_

#include <list.h>
#include <stdbool.h>


/*最大AT命令长度 --------------------------------------------------------------*/
#define MAX_AT_CMD_LEN          128

/* debug 打印接口 ------------------------------------------------------------*/
#include <stdio.h>
#define AT_DEBUG(...)          printf("[AT]:");printf(__VA_ARGS__) /*do{}while(0)*/

/* 获取系统滴答(ms) -----------------------------------------------------------*/
#include "platform.h"
#define AT_GET_TICK()           get_tick()

struct at_obj;

/*urc处理项 -----------------------------------------------------------------*/
typedef struct {
    const char *prefix;  //需要匹配的头部
    void (*handler)(char *recvbuf, int size); 
}utc_item_t;

/*AT接口适配器 ---------------------------------------------------------------*/
typedef struct {
    unsigned int (*write)(const void *buf, unsigned int len); /* 发送接口*/
    unsigned int (*read)(void *buf, unsigned int len);        /* 接收接口*/
    void         (*error)(void);                              /* AT执行异常事件*/
	utc_item_t    *utc_tbl;                                   /* urc 表*/
	unsigned char *urc_buf;                                   /* urc接收缓冲区*/
    unsigned char *recv_buf;                                  /* 数据缓冲区*/
	unsigned short urc_tbl_count;                             /* urc表项个数*/
	unsigned short urc_bufsize;                               /* urc缓冲区大小*/
    unsigned short recv_bufsize;                              /* 接收缓冲区大小*/
}at_adapter_t;

/*AT作业运行环境*/
typedef struct {
	int         i,j,state;                                    
	void        *params;
    void        (*reset_timer)(struct at_obj *at); 
	bool        (*is_timeout)(struct at_obj *at, unsigned int ms); /*时间跨度判断*/
	void        (*printf)(struct at_obj *at, const char *fmt, ...);
	char *      (*find)(struct at_obj *at, const char *expect);
    char *      (*recvbuf)(struct at_obj *at);                 /* 指向接收缓冲区*/
    unsigned int(*recvlen)(struct at_obj *at);                 /* 缓冲区总长度*/
    void        (*recvclr)(struct at_obj *at);                 /* 清空接收缓冲区*/
    bool        (*abort)(struct at_obj *at);                   /* 终止执行*/
}at_env_t;

/*AT命令响应码*/
typedef enum {
    AT_RET_OK = 0,                                             /* 执行成功*/
    AT_RET_ERROR,                                              /* 执行错误*/
    AT_RET_TIMEOUT,                                            /* 响应超时*/
	AT_RET_ABORT,                                              /* 强行中止*/
}at_return;

/*AT响应 */
typedef struct {
    void           *param;                                     
	char           *recvbuf;                                   /* 接收缓冲区*/
	unsigned short  recvcnt;                                   /* 接收数据长度*/
    at_return       ret;                                       /* AT执行结果*/
}at_response_t;

typedef void (*at_callbatk_t)(at_response_t *r);               /* AT 执行回调*/
 
/*AT状态 (当前版本未用) ------------------------------------------------------*/
typedef enum {
    AT_STATE_IDLE,                                             /* 空闲状态*/
    AT_STATE_WAIT,                                             /* 等待执行*/
    AT_STATE_EXEC,                                             /* 正在执行*/
}at_work_state;

/*AT作业项*/
typedef struct {
    unsigned int  state : 3;
    unsigned int  type  : 3;                                 /* 作业类型*/
    unsigned int  abort : 1; 
    void          *param;                                    /* 通用参数*/
	void          *info;                                     /* 通用信息指针*/
    struct list_head node;                                   /* 链表结点*/
}at_item_t;

/*AT管理器 ------------------------------------------------------------------*/
typedef struct at_obj{
	at_adapter_t            adap;
    at_env_t                env;                             /* 作业运行环境*/
	at_item_t               items[10];                       /* 最大容纳10个作业*/
    at_item_t               *cursor;                         /* 当前作业项*/
    struct list_head        ls_ready, ls_idle;               /* 就绪,空闲作业链*/
	unsigned int            timer;
	unsigned int            urc_timer;                       /* urc接收计时器*/
	at_return               ret; 
	//urc接收计数, 命令响应接收计数器
	unsigned short          urc_cnt, recv_cnt;
	unsigned char           suspend: 1;
}at_obj_t;

typedef struct {
    void (*sender)(at_env_t *e);                            /*自定义发送器 */
    const char *matcher;                                    /*接收匹配串 */
    at_callbatk_t  cb;                                      /*响应处理 */
    unsigned char  retry;                                   /*错误重试次数 */
    unsigned short timeout;                                 /*最大超时时间 */
}at_cmd_t;

void at_obj_init(at_obj_t *at, const at_adapter_t *);

bool at_send_singlline(at_obj_t *at, at_callbatk_t cb, const char *singlline);

bool at_send_multiline(at_obj_t *at, at_callbatk_t cb, const char **multiline);

bool at_do_cmd(at_obj_t *at, void *params, const at_cmd_t *cmd);

bool at_do_work(at_obj_t *at, int (*work)(at_env_t *e), void *params);

void at_item_abort(at_item_t *it);                          /*终止当前作业*/
         
bool at_obj_busy(at_obj_t *at);                              /*忙判断*/

void at_suspend(at_obj_t *at);

void at_resume(at_obj_t *at);

void at_poll_task(at_obj_t *at);


#endif
