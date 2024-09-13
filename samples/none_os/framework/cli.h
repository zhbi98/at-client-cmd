/******************************************************************************
 * @brief    命令行处理
 *
 * Copyright (c) 2015-2020, <master_roger@sina.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs: 
 * Date           Author      Notes 
 * 2015-06-09     roger.luo   初版
 *                             
 * 2017-07-04     roger.luo   优化字段分割处理
 * 
 * 2020-07-05     roger.luo   使用cli_obj_t对象, 支持多个命令源处理
 * 2020-08-29     roger.luo   支持AT指令解析及回显控制
 * 2020-02-16     roger.luo   添加命令行守卫处理程序
 ******************************************************************************/
#ifndef _CMDLINE_H_
#define _CMDLINE_H_

#include "comdef.h"

#define CLI_MAX_CMD_LEN           256            /*命令行长度*/                 
#define CLI_MAX_ARGS              16             /*最大参数个数*/
#define CLI_MAX_CMDS              64             /*最大允许定义的命令个数*/

/** 
 * @brief     CLI作为AT解析器使用,在接收匹配时自动过滤"AT+",
 */
#define CLI_AT_ENABLE             1              

/*命令类型 */
#define CLI_CMD_TYPE_EXEC         0              /* 普通执行命令*/
#define CLI_CMD_TYPE_QUERY        1              /* 查询命令 (XXX?)*/
#define CLI_CMD_TYPE_SET          2              /* 设备命令 (XXX=YY)*/

struct cli_obj;

/*命令项定义*/
typedef struct {
	char	   *name;		                         /*命令名*/ 
    /**
     * @brief     命令处理程序,类型
     * @params    o      - cli 对象
     * @params    argc   - 命令参数个数
     * @params    argv   - 命令参数表
     * @return    命令执行结果, 对于AT指令, 返回true时会自动响应OK,返回false时则
     *            响应ERROR
     */       
	int        (*handler)(struct cli_obj *o, int argc, char *argv[]);   
    const char *brief;                               /*命令简介*/
}cmd_item_t;

#define __cmd_register(name,handler,brief)\
    USED ANONY_TYPE(const cmd_item_t,__cli_cmd_##handler)\
    SECTION("cli.cmd.1") = {name, handler, brief}
    
/*******************************************************************************
 * @brief     命令注册
 * @params    name      - 命令名
 * @params    handler   - 命令处理程序
 *            类型:int (*handler)(struct cli_obj *s, int argc, char *argv[]);   
 * @params    brief     - 使用说明
 */                 
#define cmd_register(name,handler,brief)\
    __cmd_register(name,handler,brief)

/*cli 接口定义 -------------------------------------------------------------*/
typedef struct {
    /**
     * @brief 通信数据(串口)写接口
     */        
    unsigned int (*write)(const void *buf, unsigned int len);
    /**
     * @brief 通信数据(串口)读接口
     */        
    unsigned int (*read) (void *buf, unsigned int len);
    
    /**
     * @brief 命令行守卫(不需要则填写NULL,允许所有命令执行)
     * @retval true - 允许执行, false - 忽略此命令
     */         
    int (*cmd_guard)(char *cmdline);
    
}cli_port_t;

/*命令行对象*/
typedef struct cli_obj {
    int          (*guard)(char *cmdline);
    unsigned int (*write)(const void *buf, unsigned int len);
    unsigned int (*read) (void *buf, unsigned int len);
    void         (*print)(struct cli_obj *this, const char *fmt, ...); 
    int          (*get_val)(struct cli_obj *this);
    char           recvbuf[CLI_MAX_CMD_LEN + 1];  /* 命令接收缓冲区*/
    unsigned short recvcnt;                       /* 最大接收长度*/    
    unsigned       type   : 3;                    /* 命令类型*/
    unsigned       enable : 1;                    /* CLI 开关控制*/ 
    unsigned       echo   : 1;                    /* 回显设置*/    
}cli_obj_t;

void cli_init(cli_obj_t *obj, const cli_port_t *p);

void cli_enable(cli_obj_t *obj);

void cli_disable (cli_obj_t *obj);

void cli_echo_ctrl (cli_obj_t *obj, int echo);

void cli_exec_cmd(cli_obj_t *obj, const char *cmd);

void cli_process(cli_obj_t *obj);


#endif	/* __CMDLINE_H */
