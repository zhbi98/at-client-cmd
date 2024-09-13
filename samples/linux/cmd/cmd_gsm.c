/******************************************************************************
 * @brief        GSM 模块AT命令模拟器
 * Change Logs: 
 * Date           Author       Notes 
 * 2022-04-01     Roger.luo    初版
 ******************************************************************************/
#include "cli.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * @brief AT测试
 */
static int do_cmd_at(struct cli_obj *obj, int argc, char *argv[])
{
    return true;
}
cmd_register("AT", do_cmd_at, NULL);

/**
 * @brief 查询SIM卡状态
 */
static int do_cmd_cpin(struct cli_obj *obj, int argc, char *argv[])
{
    obj->print(obj, "+CPIN:READY\r\n");
    return true;
}
cmd_register("CPIN", do_cmd_cpin, NULL);

/**
 * @brief 查询信号质量
 */
static int do_cmd_csq(struct cli_obj *obj, int argc, char *argv[])
{
    obj->print(obj, "+CSQ:31,99\r\n");
    return true;
}
cmd_register("CSQ",do_cmd_csq, NULL);

/**
 * @brief 网络注册状态
 */
static int do_cmd_creg(struct cli_obj *obj, int argc, char *argv[])
{
    obj->print(obj, "+CREG: 2,1,\"1052\",\"D619\",0\r\n");
    return true;
}
cmd_register("CREG", do_cmd_creg, NULL);


/**
 * @brief 查询模块IMEI
 */
static int do_cmd_cgsn(struct cli_obj *obj, int argc, char *argv[])
{
    obj->print(obj, "123456789012345\r\n");
    return true;
}
cmd_register("CGSN", do_cmd_cgsn, NULL);


/**
 * @brief 查询模块版本号
 */
static int do_cmd_ver(struct cli_obj *obj, int argc, char *argv[])
{
    obj->print(obj, "+VER:V1.02\r\n");
    return true;
}
cmd_register("VER", do_cmd_ver, NULL);

/**
 * @brief 配置参数设置
 */
static int do_cmd_param(struct cli_obj *obj, int argc, char *argv[])
{
    static int a = 0, b = 0;
    char *p;
    if (obj->type == CLI_CMD_TYPE_QUERY) {   //参数查询
        obj->print(obj, "+PARAM=%d,%d\r\n" ,a, b);
        return true;
    } else {                                 //参数设置
        p = strchr(obj->recvbuf, '=');
        if (argc == 2 && p != NULL) {
            a = atoi(p + 1);
            b = atoi(argv[1]);
            obj->print(obj, "Parameter setting is successfully, a=%d, b=%d\r\n", a, b);
            return true;
        }
    }
    return false;
}
cmd_register("PARAM", do_cmd_param, NULL);


/**
 * @brief 读取二进制数据命令
 */
static int do_cmd_read_bin(struct cli_obj *obj, int argc, char *argv[])
{
    int i;
    unsigned char check;            //数据校验码
    int           len;              
    char buf[256];
    srand((unsigned)time(NULL));
    len = rand() % sizeof(buf);    
    for (i = 1, check = 0; i < len; i++) {
        
        buf[i] = rand() % 128;
        check ^= buf[i];          //计算校验码
    }
    buf[0] = check;
    //=> AT+RDBIN=<socket id>,<read size>
    // 
    //<= +BINDAT:<socket id>,<real size>\r\n<raw data......>
    //
    //<= OK

    obj->print(obj, "+BINDAT:%d,%d\r\n" ,1, len);
    obj->write(buf, len);
    
    return true;
}
cmd_register("BINDAT", do_cmd_read_bin, NULL);
