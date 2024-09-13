/******************************************************************************
 * @brief        AT命令通信管理(OS版本)
 *
 * Copyright (c) 2020, <morro_luo@163.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs: 
 * Date           Author       Notes 
 * 2020-01-02     Morro        Initial version. 
 * 2021-02-01     Morro        支持URC回调中接收数据.
 * 2021-02-05     Morro        1.修改struct at_obj,去除链表管理机制
 *                             2.删除 at_obj_destroy接口
 * 2021-03-21     Morro        删除at_obj中的at_work_ctx_t域,减少内存使用
 * 2021-04-08     Morro        解决重复释放信号量导致命令出现等待超时的问题
 * 2021-05-15     Morro        优化URC匹配程序
 * 2021-07-20     Morro        增加接收锁,解决执行at_do_work时urc部分数据丢失问题
 ******************************************************************************/

#ifndef _AT_H_
#define _AT_H_

#include "at_util.h"
#include <stdbool.h>

/* 单行最大命令长度 */ 
#define MAX_AT_CMD_LEN        128

/* 单行urc接收超时时间*/  
#define MAX_URC_RECV_TIMEOUT  300      

/* 指定的URC 结束标记列表 */
#define SPEC_URC_END_MARKS     ":,\r\n"
     
struct at_obj;                                                /* AT对象*/

/*AT命令响应码 ---------------------------------------------------------------*/
typedef enum {
    AT_RET_OK = 0,                                            /* 执行成功*/
    AT_RET_ERROR,                                             /* 执行错误*/
    AT_RET_TIMEOUT,                                           /* 响应超时*/
	AT_RET_ABORT,                                             /* 未知错误*/
}at_return;

/**
 * @brief URC 上下文(Context) 定义
 */
typedef struct {
    /**
     * @brief   数据读取接口
     * @params  buf   - 缓冲区
     * @params  len   - 缓冲区大小    
     */
    unsigned int (*read)(void *buf, unsigned int len);       
    char *buf;                                                /* 数据缓冲区 */
    int bufsize;                                              /* 缓冲区大小 */
    int recvlen;                                              /* 已接收数据长度*/
} at_urc_ctx_t;

/*(Unsolicited Result Codes (URCs))处理项 ------------------------------------*/
typedef struct {
    /**
     * @brief urc 前缀(如"+CSQ: ")
     */    
    const char *prefix;
    /**
     * @brief urc 指定结束字符标记(参考DEF_URC_END_MARKS列表，如果不指定则默认"\n")
     * @note  
     */     
    const char *end_mark;
    /**
     * @brief       urc处理程序
     * @params      ctx - URC 上下文
     */
    void (*handler)(at_urc_ctx_t *ctx);
}urc_item_t;
    
/**
 * @brief AT接口适配器(用于初始化AT控制器)
 */
typedef struct {
    
    /**
     * @brief 数据写接口
     */    
    unsigned int (*write)(const void *buf, unsigned int len);
    
    /**
     * @brief 数据读接口
     */    
    unsigned int (*read)(void *buf, unsigned int len);         
    /**
     * @brief 调试打印输出接口,如果不需要则填NULL
     */      
    void         (*debug)(const char *fmt, ...);  
    
    /**
     * @brief urc 处理函数表,如果不需要则填NULL
     */     
	urc_item_t    *utc_tbl;
    /**
     * @brief urc接收缓冲区,如果不需要则填NULL
     */
	char          *urc_buf;
    /**
     * @brief utc_tbl项个数,如果不需要则填0
     */    
	unsigned short urc_tbl_count;  
    /**
     * @brief urc缓冲区大小,如果不需要则填0
     */      
	unsigned short urc_bufsize;
}at_adapter_t;

/**
 * @brief AT命令响应参数
 */
typedef struct {    
    const char    *matcher;                                   /* 接收匹配串 */
    char          *recvbuf;                                   /* 接收缓冲区 */
    unsigned short bufsize;                                   /* 缓冲区长度 */
    unsigned int   timeout;                                   /* 最大超时时间 */
} at_respond_t;

/** work context  ------------------------------------------------------------*/
/**
 * @brief AT作业上下文(Work Context) 定义
 */
typedef struct at_work_ctx {
    struct at_obj *at;
    
    //作业参数,由at_do_work接口传入
	void          *params;
    /**
     * @brief   数据写操作
     */    
    unsigned int (*write)(struct at_work_ctx *self, const void *buf, unsigned int len);
    /**
     * @brief   数据读操作
     */     
    unsigned int (*read)(struct at_work_ctx *self, void *buf, unsigned int len);
    
    /**
     * @brief   格式化打印输出    
     */     
	void         (*printf)(struct at_work_ctx *self, const char *fmt, ...);
    /*
     * @brief       等待回复
     * @param[in]   prefix  - 期待待接收前缀(如"OK",">")
     * @param[in]   timeout - 等待超时时间
     */
	at_return    (*wait_resp)(struct at_work_ctx *self, const char *prefix, 
                              unsigned int timeout);
}at_work_ctx_t;


/*AT对象 ---------------------------------------------------------------------*/
typedef struct at_obj {
	at_adapter_t            adap;                             /* 接口适配器*/
	at_sem_t                send_lock;                        /* 发送锁*/
    at_sem_t                recv_lock;                        /* 接收锁*/
	at_sem_t                completed;                        /* 命令完成信号*/
    at_respond_t            *resp;                            /* AT应答信息*/
    const urc_item_t        *urc_item;                        /* 当前URC项*/
	unsigned int            resp_timer;                       /* 响应接收定时器*/
	unsigned int            urc_timer;                        /* URC定时器 */
	at_return               ret;                              /* 命令执行结果*/
	unsigned short          urc_cnt, rcv_cnt;                 /* 接收计数器*/
	unsigned char           busy   : 1;
	unsigned char           suspend: 1;
    unsigned char           dowork : 1;
}at_obj_t;

typedef int (*at_work)(at_work_ctx_t *);

void at_obj_init(at_obj_t *at, const at_adapter_t *adap);      /* AT初始化*/

bool at_obj_busy(at_obj_t *at);

void at_suspend(at_obj_t *at);                                 /* 挂起*/
 
void at_resume(at_obj_t *at);                                  /* 恢复*/

at_return at_do_cmd(at_obj_t *at, at_respond_t *r, const char *cmd);

int at_split_respond_lines(char *recvbuf, char *lines[], int count, char separator);

int at_do_work(at_obj_t *at, at_work work, void *params);      /* 自定义AT作业*/


void at_process(at_obj_t *at);                                 /* AT接收处理*/
        
#endif
