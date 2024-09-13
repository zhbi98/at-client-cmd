/**
  ******************************************************************************
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
  * 2021-07-20     Morro        增加锁,解决执行at_do_work时urc部分数据丢失问题
  * 2021-11-20     Morro        按单字符处理接收,避免出现在urc事件中读取数据时
  *                             导致后面数据丢失问题
  ******************************************************************************
  */
#include "at.h"
#include "comdef.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

#define MAX_AT_LOCK_TIME   60 * 1000     //AT命令锁定时间

static void urc_recv_process(at_obj_t *at, const char *buf, unsigned int size);

/** 
  * @brief    默认调试接口
  */
static void nop_dbg(const char *fmt, ...){}


/**
  * @brief    输出字符串
  */
static void put_string(at_obj_t *at, const char *s)
{
    while (*s != '\0')
        at->adap.write(s++, 1);
}

/**
  * @brief    输出字符串(带换行)
  */
static void put_line(at_obj_t *at, const char *s)
{
    put_string(at, s);
    put_string(at, "\r\n");    
    at->adap.debug("->\r\n%s\r\n", s);
}


/**
  * @brief    作业读操作
  */
static unsigned int at_work_read(struct at_work_ctx *e, void *buf, unsigned int len)
{ 
    at_obj_t *at = e->at;
    len = at->adap.read(buf, len);
    urc_recv_process(at, buf, len);             //递交到URC进行处理
    return len;
}

/**
  * @brief    作业写操作
  */
static unsigned int at_work_write(struct at_work_ctx *e, const void *buf, unsigned int len)
{
    return e->at->adap.write(buf, len);
}

//打印输出
static void at_work_print(struct at_work_ctx *e, const char *cmd, ...)
{
    va_list args;
    va_start(args, cmd);
    char buf[MAX_AT_CMD_LEN];
    vsnprintf(buf, sizeof(buf), cmd, args);
    put_line(e->at, buf);
    va_end(args);	
}

//等待AT命令响应
static at_return wait_resp(at_obj_t *at, at_respond_t *r)
{        
    at->ret   = AT_RET_TIMEOUT;
    at->resp_timer = at_get_ms();    
    at->rcv_cnt = 0;                   //清空接收缓存
    at->resp  = r;
    at_sem_wait(at->completed, r->timeout);
    at->adap.debug("<-\r\n%s\r\n", r->recvbuf);
    at->resp = NULL;
    return at->ret;
}

/**
  * @brief       等待接收到指定串
  * @param[in]   resp    - 期待待接收串(如"OK",">")
  * @param[in]   timeout - 等待超时时间
  */
at_return wait_recv(struct at_work_ctx *e, const char *resp, unsigned int timeout)
{
    char buf[64];
    int cnt = 0, len;
    at_obj_t *at = e->at;    
    at_return ret = AT_RET_TIMEOUT;
    unsigned int timer = at_get_ms();
    while (at_get_ms() - timer < timeout) {
        len = e->read(e, &buf[cnt], sizeof(buf) - cnt);
        if (len > 0) {
            cnt += len;
            buf[cnt] = '\0';
            if (strstr(buf, resp)) {
                ret =  AT_RET_OK;
                break;
            } else if (strstr(buf, "ERROR")) {
                ret =  AT_RET_ERROR;
                break;
            }
        } else
            at_delay(1);
    }
    at->adap.debug("%s\r\n", buf);
    return ret;
}

/**
  * @brief       创建AT控制器
  * @param[in]   adap - AT接口适配器
  */
void at_obj_init(at_obj_t *at, const at_adapter_t *adap)
{
    at->adap    = *adap;
    at->rcv_cnt = 0;    
    at->send_lock = at_sem_new(1);
    at->recv_lock = at_sem_new(1);
    at->completed = at_sem_new(0);
    at->urc_item  = NULL;
    if (at->adap.debug == NULL)
        at->adap.debug = nop_dbg;
    
}

/**
  * @brief       执行命令
  * @param[in]   fmt    - 格式化输出
  * @param[in]   r      - 响应参数,如果填NULL, 默认返回OK表示成功,等待5s
  * @param[in]   args   - 如变参数列表
  */
at_return at_do_cmd(at_obj_t *at, at_respond_t *r, const char *cmd)
{
    at_return ret;
    char      defbuf[64];
    at_respond_t  default_resp = {"OK", defbuf, sizeof(defbuf), 5000};
    if (r == NULL) {
        r = &default_resp;                 //默认响应      
    }
    if (!at_sem_wait(at->send_lock, r->timeout)) {
        return AT_RET_TIMEOUT;    
    }
    at->busy  = true;
    
    while (at->urc_cnt) {
        at_delay(10);
    }
    put_line(at, cmd);
    ret = wait_resp(at, r); 
    at_sem_post(at->send_lock);
    at->busy  = false;
    return ret;    
}

/**
  * @brief       执行AT作业
  * @param[in]   at    - AT控制器
  * @param[in]   work  - 作业入口函数(类型 - int (*)(at_work_ctx_t *))
  * @param[in]   params- 作业参数
  * @return      依赖于work的返回值
  */
int at_do_work(at_obj_t *at, at_work work, void *params)
{
    at_work_ctx_t ctx;
    int ret;
    if (!at_sem_wait(at->send_lock, MAX_AT_LOCK_TIME)) {
        return AT_RET_TIMEOUT;
    }
    if (!at_sem_wait(at->recv_lock, MAX_AT_LOCK_TIME))
        return AT_RET_TIMEOUT; 
    
    at->busy  = true;
    //构造at_work_ctx_t
    ctx.params    = params;
    ctx.printf    = at_work_print;
    ctx.read      = at_work_read;
    ctx.write     = at_work_write;
    ctx.wait_resp = wait_recv;  
    ctx.at        = at;
     
    at->rcv_cnt = 0;
    ret = work(&ctx);
    at_sem_post(at->recv_lock);
    at_sem_post(at->send_lock);
    at->busy  = false;
    return ret;
}

/**
  * @brief       分割响应行
  * @param[in]   recvbuf  - 接收缓冲区 
  * @param[out]  lines    - 响应行数组
  * @param[in]   separator- 分割符(, \n)
  * @return      行数
  */
int at_split_respond_lines(char *recvbuf, char *lines[], int count, char separator)
{
    char *s = recvbuf;
    size_t i = 0;      
    if (s == NULL || lines == NULL) 
        return 0;     
        
    lines[i++] = s;    
    while(*s && i < count) {       
        if (*s == ',') {
            *s = '\0';                                       
            lines[i++] = s + 1;                           /*指向下一个子串*/
        }
        s++;        
    }    
    return i;
}
/**
  * @brief       从URC缓冲区中查询URC项
  * @param[in]   urcline - URC行
  * @return      true - 正常识别并处理, false - 未识别URC
  */
const urc_item_t *find_urc_item(at_obj_t *at, char *urc_buf, unsigned int size)
{
    const urc_item_t *tbl = at->adap.utc_tbl;
    int i;
    if (size < 2)
        return NULL;
    for (i = 0; i < at->adap.urc_tbl_count; i++) {
        if (strstr(urc_buf, tbl->prefix))
            return tbl;  
        tbl++;
    }
    return NULL;
}

/**
  * @brief       urc 处理总入口
  * @param[in]   urcline - URC行
  */
static void urc_handler_entry(at_obj_t *at,  char *urcline, unsigned int size)
{
    at_urc_ctx_t context = {at->adap.read, urcline, at->adap.urc_bufsize, size};
    at->adap.debug("<=\r\n%s\r\n", urcline);            
    at->urc_item->handler(&context);                       /* 递交到上层处理 */      
}

/**
  * @brief       urc 接收处理
  * @param[in]   ch  - 接收字符
  * @return      none
  */
static void urc_recv_process(at_obj_t *at, const char *buf, unsigned int size)
{
    register char *urc_buf;	
    int ch;
    urc_buf  = at->adap.urc_buf;
    
    //接收超时处理,默认MAX_URC_RECV_TIMEOUT
    if (at->urc_cnt > 0 && at_istimeout(at->urc_timer, MAX_URC_RECV_TIMEOUT)) {
        urc_buf[at->urc_cnt] = '\0';
        if (at->urc_cnt > 2)
            at->adap.debug("urc recv timeout=>%s\r\n", urc_buf);
        at->urc_cnt  = 0;
        at->urc_item = NULL;		
    }    
	
    while (size--) {
        at->urc_timer = at_get_ms(); 
        ch =  *buf++;
        urc_buf[at->urc_cnt++] = ch;
        
        if (strchr(SPEC_URC_END_MARKS, ch) || ch == '\0') {      //结束标记列表
            urc_buf[at->urc_cnt] = '\0';    
            if (at->urc_item == NULL)
                at->urc_item = find_urc_item(at, urc_buf, at->urc_cnt);       
            if (at->urc_item != NULL){
                if (strchr(at->urc_item->end_mark, ch)) {        //匹配结束标记  
                    urc_handler_entry(at, urc_buf, at->urc_cnt);
                    at->urc_cnt  = 0;
                    at->urc_item = NULL;
                }
            } else if (ch == '\r' || ch == '\n'|| ch == '\0') {
                if (at->urc_cnt > 2 && !at->busy) {
                    at->adap.debug("%s\r\n", urc_buf);          //未识别到的URC
                }
                at->urc_cnt = 0;                
            }
        } else if (at->urc_cnt >= at->adap.urc_bufsize) {         //溢出处理
            at->urc_cnt = 0;
            at->urc_item = NULL;
        }
    }
}

/**
  * @brief       命令响应通知
  * @return      none
  */
static void resp_notification(at_obj_t *at, at_return ret)
{
    at->ret = ret;    
    at->resp = NULL;

    at_sem_post(at->completed);
}

/**
  * @brief       指令响应接收处理
  * @param[in]   buf  - 接收缓冲区
  * @param[in]   size - 缓冲区数据长度
  * @return      none
  */
static void resp_recv_process(at_obj_t *at, const char *buf, unsigned int size)
{
    char *rcv_buf;
    unsigned short rcv_size;	
    at_respond_t *resp = at->resp;
    
    if (resp == NULL)                                    //无命令请求
        return;
    
    if (size) {
        rcv_buf  = (char *)resp->recvbuf;
        rcv_size = resp->bufsize;

        if (at->rcv_cnt + size >= rcv_size) {             //接收溢出
            at->rcv_cnt = 0;
            at->adap.debug("Receive overflow:%s", rcv_buf);
        }
        /*将接收到的数据放至rcv_buf中 ---------------------------------------------*/
        memcpy(rcv_buf + at->rcv_cnt, buf, size);
        at->rcv_cnt += size;
        rcv_buf[at->rcv_cnt] = '\0';
        
        if (strstr(rcv_buf, resp->matcher)) {            //接收匹配
            resp_notification(at, AT_RET_OK);
            return;
        } else if (strstr(rcv_buf, "ERROR")) {
            resp_notification(at, AT_RET_ERROR);
            return;
        }
    } 
    
    if (at_istimeout(at->resp_timer, resp->timeout))    //接收超时
        resp_notification(at, AT_RET_TIMEOUT);
    else if (at->suspend)                                //强制终止
        resp_notification(at, AT_RET_ABORT);

}

/**
  * @brief       AT忙判断
  * @return      true - 有AT指令或者任务正在执行中
  */
bool at_obj_busy(at_obj_t *at)
{
    return !at->busy && at_istimeout(at->urc_timer, 2000);
}

/**
  * @brief       挂起AT作业
  * @return      none
  */
void at_suspend(at_obj_t *at)
{
    at->suspend = 1;
}

/**
  * @brief       恢复AT作业
  * @return      none
  */
void at_resume(at_obj_t *at)
{
    at->suspend = 0;
}

/**
  * @brief       AT处理
  * @return      none
  */
void at_process(at_obj_t *at)
{
    char buf[1];
    unsigned int len;
    if (!at_sem_wait(at->recv_lock, MAX_AT_LOCK_TIME))
        return;
    do {
        len = at->adap.read(buf, sizeof(buf));
        urc_recv_process(at, buf,len);
        resp_recv_process(at, buf, len);
    } while (len);
    
    at_sem_post(at->recv_lock);
}
