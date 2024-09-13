/******************************************************************************
 * @brief    命令行处理
 *
 * Copyright (c) 2015-2020, <morro_luo@163.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs: 
 * Date           Author     Notes 
* 2015-06-09      roger.luo  初版
*                             
* 2017-07-04      roger.luo  优化字段分割处理
* 
* 2020-07-05      roger.luo  使用cli_obj_t对象, 支持多个命令源处理
 ******************************************************************************/
#include "cli.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>

static const cmd_item_t cmd_tbl_start SECTION("cli.cmd.0") = {0};
static const cmd_item_t cmd_tbl_end SECTION("cli.cmd.4") = {0};    
/**
 * @brief       查找命令
 * @param[in]   keyword - 命令关键字
 * @return      命令项
 */ 
static const cmd_item_t *find_cmd(const char *keyword, int n)
{                   
	const cmd_item_t *it;
    for (it = &cmd_tbl_start + 1; it < &cmd_tbl_end; it++) {
        if (!strncasecmp(keyword, it->name, n))
            return it;
    }
	return NULL;
}

/*******************************************************************************
 * @brief      字符串分割  - 在源字符串查找出所有由separator指定的分隔符
 *                            (如',')并替换成字符串结束符'\0'形成子串，同时令list
 *                            指针列表中的每一个指针分别指向一个子串
 * @example 
 *             input=> s = "abc,123,456,,fb$"  
 *             separator = ",$"
 *            
 *             output=>s = abc'\0' 123'\0' 456'\0' '\0' fb'\0''\0'
 *             list[0] = "abc"
 *             list[1] = "123"
 *             list[2] = "456"
 *             list[3] = ""
 *             list[4] = "fb"
 *             list[5] = ""
 *
 * @param[in] str             - 源字符串
 * @param[in] separator       - 分隔字符串 
 * @param[in] list            - 字符串指针列表
 * @param[in] len             - 列表长度
 * @return    list指针列表项数，如上例所示则返回6
 ******************************************************************************/  
static size_t strsplit(char *s, const char *separator, char *list[],  size_t len)
{
    size_t count = 0;      
    if (s == NULL || list == NULL || len == 0) 
        return 0;     
        
    list[count++] = s;    
    while(*s && count < len) {       
        if (strchr(separator, *s) != NULL) {
            *s = '\0';                                       
            list[count++] = s + 1;                           /*指向下一个子串*/
        }
        s++;        
    }    
    return count;
}



/**
 * @brief 打印一个格式化字符串到串口控制台
 * @retval 
 */
static void cli_print(cli_obj_t *obj, const char *format, ...)
{
	va_list args;
    int len;
	char buf[CLI_MAX_CMD_LEN + CLI_MAX_CMD_LEN / 2];
	va_start (args, format);
	len = vsnprintf (buf, sizeof(buf), format, args);
	va_end (args);
    obj->write(buf, len);    
}


/**
 * @brief       处理行
 * @param[in]   line - 命令行
 * @return      none
 **/
static void process_line(cli_obj_t *obj)
{
    char *argv[CLI_MAX_ARGS];
    int   argc, ret, isat = 0;
    const cmd_item_t *it;
    
    //命令拦截
    if (obj->guard && !obj->guard(obj->recvbuf))
        return;
    
    if (obj->echo) {  //回显
        obj->print(obj,"%s\r\n",obj->recvbuf);
    }

    argc = strsplit(obj->recvbuf, ",",argv, CLI_MAX_ARGS);
    
    const char *start, *end;

    if (argv[0] == NULL)
        return;

    if (strcasecmp("AT", argv[0]) == 0) {
        obj->print(obj, "OK\r\n");
        return;
    } 
#if CLI_AT_ENABLE != 0
    isat = strncasecmp(argv[0], "AT+", 3) == 0;
#endif
    
    start= !isat ? argv[0] : argv[0] + 3;

    if ((end = strchr(start, '=')) != NULL) {
        obj->type = CLI_CMD_TYPE_SET;

    } else if ((end = strchr(start, '?')) != NULL) {
        obj->type = CLI_CMD_TYPE_QUERY;

    } else {
        obj->type = CLI_CMD_TYPE_EXEC;
        end = start + strlen(argv[0]);
    }
    if (start == end)
        return;

    if ((it = find_cmd(start, end - start)) == NULL) {
        obj->print(obj, "%s\r\n", isat ? "ERROR" : "");
        return;
    }
    ret = it->handler(obj, argc, argv);
    if (isat) {
        obj->print(obj, "%s\r\n" ,ret ? "OK":"ERROR");
    }
}

/**
 * @brief       获取设置值
 */
static int get_val(struct cli_obj *self)
{
    char *p;
    p = strchr(self->recvbuf, '=');
    if ( p == NULL)
        return 0;
    return atoi(p + 1);
  
}

/**
 * @brief       cli 初始化
 * @param[in]   p - cli驱动接口
 * @return      none
 */
void cli_init(cli_obj_t *obj, const cli_port_t *p)
{
    obj->read   = p->read;
    obj->write  = p->write;
    obj->print  = cli_print;
    obj->enable = true;
    obj->get_val = get_val;
    obj->guard   = p->cmd_guard;
}

/**
 * @brief       进入cli命令模式(cli此时自动处理用户输入的命令)
 * @param[in]   none
 * @return      none
 **/
void cli_enable(cli_obj_t *obj)
{
    char a;
    obj->enable = true;
    obj->recvcnt = 0;
    while (obj->read(&a, 1) > 0) {}
}

/**
 * @brief       退出cli命令模式(cli此时不再处理用户输入的命令)
 * @param[in]   none
 * @return      none
 **/
void cli_disable (cli_obj_t *obj)
{
    obj->enable = false;
}

/**
 * @brief       回显控制
 * @param[in]   echo - 回显开关控制(0/1)
 * @return      none
 **/
void cli_echo_ctrl (cli_obj_t *obj, int echo)
{
    obj->echo = echo;
}

/**
 * @brief       执行一行命令(无论cli是否运行,都会执行)
 * @param[in]   none
 * @return      none
 **/
void cli_exec_cmd(cli_obj_t *obj, const char *cmd)
{
    int len = strlen(cmd);
    if (len >= CLI_MAX_CMD_LEN - 1)
        return;
    strcpy(obj->recvbuf, cmd);
    process_line(obj);
}

/**
 * @brief       命令行处理程序
 * @param[in]   none
 * @return      none
 **/
void cli_process(cli_obj_t *obj)
{    
    char buf[32];
    int i, readcnt;
    if (!obj->read || !obj->enable)
        return;
    
    readcnt = obj->read(buf, sizeof(buf));
    
    if (readcnt) {
        for (i = 0; i < readcnt; i++) {
            if (buf[i] == '\r' || buf[i] == '\n' || buf[i] == '\0') {
                obj->recvbuf[obj->recvcnt] = '\0';
                if (obj->recvcnt > 1)
                    process_line(obj);
                obj->recvcnt = 0;
            } else {
                obj->recvbuf[obj->recvcnt++] = buf[i];
                
                if (obj->recvcnt >= CLI_MAX_CMD_LEN) /*缓冲区满之后强制清空*/
                    obj->recvcnt = 0;
            }
        }
    }
}

#if 1
/*******************************************************************************
* @brief	   命令比较器
* @param[in]   none
* @return 	   参考strcmp
*******************************************************************************/
static int cmd_item_comparer(const void *item1,const void *item2)
{
    cmd_item_t *it1 = *((cmd_item_t **)item1); 
    cmd_item_t *it2 = *((cmd_item_t **)item2);    
    return strcmp(it1->name, it2->name);
}
#endif

/*
 * @brief	   帮助命令
 */
static int do_help (struct cli_obj *s, int argc, char *argv[])
{
	int i,j, count;
    const cmd_item_t *item_start = &cmd_tbl_start + 1; 
    const cmd_item_t *item_end   = &cmd_tbl_end;	
	const cmd_item_t *cmdtbl[CLI_MAX_CMDS];
    
    if (argc == 2) {
        if ((item_start = find_cmd(argv[1], strlen(argv[1]))) != NULL) 
        {
            s->print(s, item_start->brief);                  /*命令使用信息----*/
            s->print(s, "\r\n");   
        }
        return 0;
    }
    for (i = 0; i < item_end - item_start && i < CLI_MAX_ARGS; i++)
        cmdtbl[i] = &item_start[i];
    count = i;
    /*对命令进行排序 ---------------------------------------------------------*/
    qsort(cmdtbl, i, sizeof(cmd_item_t*), cmd_item_comparer);    		        
    s->print(s, "\r\n");
    for (i = 0; i < count; i++) {
        s->print(s, cmdtbl[i]->name);                        /*打印命令名------*/
        /*对齐调整*/
        j = strlen(cmdtbl[i]->name);
        if (j < 10)
            j = 10 - j;
            
        while (j--)
            s->print(s, " ");
            
        s->print(s, "- "); 
		s->print(s, cmdtbl[i]->brief);                       /*命令使用信息----*/
        s->print(s, "\r\n");        
    }
	return 1;
}
 /*注册帮助命令 ---------------------------------------------------------------*/
cmd_register("help", do_help, "list all command.");   
cmd_register("?",    do_help, "alias for 'help'");
