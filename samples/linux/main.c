/******************************************************************************
 * @brief        AT命令测试程序
 * Change Logs: 
 * Date           Author       Notes 
 * 2022-04-01     Roger.luo    初版
 ******************************************************************************/
#include "at_chat.h"
#include "at_port.h"
#include "at_device.h"
#include <sys/poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>


static at_obj_t       *at_obj;      //AT
static pthread_mutex_t at_lock;     //互斥锁

typedef struct {
    pthread_mutex_t completed;      //完成信号量
} at_watch_t;

/**
 * @brief 测试项
 */
typedef struct {
    void (*handler)(void);
    const char *brief;
} test_item_t;

/**
 * @brief 上锁
 */
static void at_mutex_lock(void)
{
    pthread_mutex_lock(&at_lock);
}

/**
 * @brief 解锁
 */
static void at_mutex_unlock(void)
{
    pthread_mutex_unlock(&at_lock);
}

/**
 * @brief 命令异常处理
 */
static void at_error_handler(at_response_t *r)
{
    printf("Error detected!\r\n");
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

#if AT_URC_WARCH_EN

/**
 * @brief 开/关机URC处理程序
 */
static int urc_power_handler(at_urc_info_t *ctx)
{
    int status;
    if (sscanf(ctx->urcbuf, "+POWER:%d",&status) == 1) {
        printf("Device power %s event detected!\r\n", status ? "on" : "off");
    }
    return 0;
}
/**
 * @brief socket数据包(+IPD,<socket id>,<data length>:.....)
 */
static int urc_socket_data_handler(at_urc_info_t *ctx)
{
    int  length, total_length, sockid, i;
    char *data;    
    unsigned char check = 0;
    if (sscanf(ctx->urcbuf, "+IPD,%d,%d:", &sockid, &length) == 2) {     //解析出总数据长度
        data = strchr(ctx->urcbuf, ':');
        if (data == NULL)
            return 0;
        data++;        
        total_length = (data - ctx->urcbuf) + length;        //计算头部长度
        if (ctx->urclen < total_length) {                    //未接收全,返回剩余待接收数据
            printf("Need to receive %d more bytes\r\n", total_length - ctx->urclen);
            return total_length - ctx->urclen;
        }
        for (i = 1, check = 0; i < length; i++) {
            check ^= data[i];
        }
        printf("%d bytes of data were received form socket %d, check %s!\r\n", 
               length, sockid, check == data[0] ? "ok": "error");
    }
    return 0;
}

/**
 * @brief urc订阅表
 */
static const urc_item_t urc_table[] = 
{
    {.prefix = "+POWER:",    .endmark = '\n', .handler = urc_power_handler},
    {.prefix = "+IPD,",      .endmark = ':',  .handler = urc_socket_data_handler}
};
#endif

/**
 * @brief 显示内存使用信息
 */
static void sample_show_memory(void)
{
#if AT_MEM_WATCH_EN    
    printf("max memory:%d, current memory:%d\r\n", at_max_used_memory(), at_cur_used_memory());
#else
    printf("Unknown memory usage, please enable 'AT_MEM_WATCH_EN' macro first.\r\n");
#endif    
}

/**
 * @brief 单行命令
 */
static void sample_singlline(void)
{
    at_attr_t attr;
    at_attr_deinit(&attr);    
    // => AT+CSQ
    // <= +CSQ:<rssi>,<ber>
    // <= OK    
    at_send_singlline(at_obj, &attr, "AT+CSQ");  
}

/**
 * @brief 多行命令
 */
static void sample_multiline(void)
{
    static const char *cmd_table[] = {
        "AT+CPIN?",
        "AT+CSQ",
        "AT+CREG",
        "AT+PARAM?",
        NULL
    };    
    at_send_multiline(at_obj, NULL, cmd_table);
}

/**
 * @brief 可变参数命令
 */
static void sample_variable_param_cmd(void)
{
    srand((unsigned)time(NULL));
    at_exec_cmd(at_obj, NULL, "AT+PARAM=%d,%d",rand() % 100,rand() % 200);
    at_exec_cmd(at_obj, NULL, "AT+PARAM?");
}

/**
 * @brief 响应超级返重试
 */
static void sample_timeout_retry(void)
{
    at_attr_t attr;  
    at_attr_deinit(&attr);
    attr.retry = 3;
    attr.timeout = 1000;
    at_exec_cmd(at_obj, &attr, "AAA");      
}

/**
 * @brief 命令错误重试
 */
static void sample_error_retry(void)
{
    at_attr_t attr;
    at_attr_deinit(&attr);
    attr.retry = 2;
    at_exec_cmd(at_obj, &attr, "AT+CMD"); 
}

/**
 * @brief 命令终止
 */
static void sample_abort_command(void)
{
    at_exec_cmd(at_obj, NULL, "AT+CMD"); 
    at_work_abort_all(at_obj);
}

/**
 * @brief 版本读取响应程序
 */
static void read_ver_cb(at_response_t *r)
{
    char verbuf[16];
    if (r->code == AT_RESP_OK) {
        sscanf(r->prefix, "+VER:%s\r\n", verbuf);
        printf("Version info:%s\r\n", verbuf);
    }
}   

/**
 * @brief 指定响应前缀
 *        => AT+VER
 *        <= +VER:Vxxxx
 *        <= OK
 */
static void sample_specific_prefix(void)
{
    at_attr_t attr;
    at_attr_deinit(&attr);
    //匹配介于[+VER -> \r\n]之间的内容
    attr.prefix = "+VER",
    attr.suffix = "\r";  
    attr.cb     =  read_ver_cb;
    at_exec_cmd(at_obj, &attr, "AT+VER"); 
}

/**
 * @brief 自定义命令发送器
 */
static void custom_sender(at_env_t *env)
{
    env->println(env, "AT+CSQ\r\n"); 
}
/**
 * @brief 自定义命令
 */
static void sample_custom_cmd(void)
{
    at_custom_cmd(at_obj, NULL, custom_sender);      
}

/**
 * @brief 发送缓冲区
 */
static void sample_send_buffer(void)
{
    char buf[] = "AT+CSQ\r\n";
    at_send_data(at_obj, NULL, buf, sizeof(buf));      
}

/**
 * @brief 查询CSQ
 */
static int at_work_query_csq(at_env_t *env)
{
    char *p;
    int rssi, ber;
    switch (env->state)
    {
    case 0:
        env->println(env, "AT+CSQ\r\n");
        env->state++;                    //转到接收状态
        env->recvclr(env);               //清空接口
        env->reset_timer(env);           //重置计时器
        break;
    case 1:
        if (env->contains(env, "OK")) {
            //+CSQ:<rssi>,<ber>
            if ((p = env->contains(env, "+CSQ")) != NULL && 
                sscanf(p, "+CSQ:%d,%d", &rssi, &ber) == 2) {
                printf("The CSQ read ok, rssi:%d, ber:%d\r\n", rssi, ber);
            }            
            return true;
        } else if (env->contains(env, "ERROR")) {  //错误处理
            printf("The CSQ read failed.\r\n");
            return true;
        } else if (env->is_timeout(env, 1000)) {   //错误处理
            printf("The CSQ read timeout.\r\n");
            return true;
        }
        break;    
    default:
        return true;
    }
    return false;
}

/**
 * @brief 自定义作业测试1
 */
static void sample_at_work_1(void)
{
    at_do_work(at_obj, NULL, at_work_query_csq);              
}


static unsigned char check_bin_data(unsigned char *buf, int len)
{
    unsigned char check = 0;    
    while (len--) {
        check ^= *buf++;
    }
    return check;
}

/**
 * @brief 读取socket数制
 */
static int at_work_read_bin(at_env_t *env)
{
#define READ_STAT_START 0
#define READ_STAT_HEAD  1
#define READ_STAT_DATA  2

    static char *start, *end;
    static int  total, sockid;
    unsigned char buf[256];
    //=> AT+BINDAT=<socket id>,<read size>
    // 
    //<= +BINDAT:<socket id>,<real size>\r\n<raw data......>
    //
    //<= OK
    switch (env->state)
    {
    case READ_STAT_START:
        env->println(env, "AT+BINDAT=%d,%d\r\n", 1, sizeof(buf));
        env->recvclr(env);
        env->reset_timer(env);
        env->state = READ_STAT_HEAD;
        break;
    case READ_STAT_HEAD:
        if ((start = env->contains(env, "+BINDAT:")) != NULL) {
            end = strchr(start, '\n');
            if (end == NULL)
                break;
            end++;
            if (sscanf(start, "+BINDAT:%d,%d" ,&sockid, &total) == 2) {                
                env->reset_timer(env);
                env->state++; 
                printf("Next receive %d bytes data from socket %d\r\n", total, sockid);
            } 
        } else if (env->contains(env, "ERROR")) {
            env->finish(env, AT_RESP_ERROR);
            return true;
        } else if (env->is_timeout(env, 1000)) {
            if (env->i++ > 3) {
                /*产生发送异常事件 */
                printf("recv error!!!\r\n");
                return true;
            }
            env->state = READ_STAT_START;                        /*重新发送*/                
        } 
        break;    
    case READ_STAT_DATA:
        //
        //recvbuf...start....end....data ....
        //                   |---- total ----|
        //|---------- recvlen ---------------|
        if (env->recvlen(env) >= total + (end - env->recvbuf(env))) { //数据接收完成
            memcpy(buf, end, total);
            printf("recv ok!!!\r\n");
            if (total > 0 && buf[0] == check_bin_data(&buf[1], total - 1)) 
                printf("check ok!!!\r\n");
            return true;
        } else if (env->is_timeout(env, 1000)){
            printf("recv error!!!\r\n");
            return true;
        }
        break;         
    default:
        break;
    }
    return false;
}

/**
 * @brief Read binary data via 'at work'
 */
static void sample_read_bin_data(void)
{
    at_do_work(at_obj, NULL, at_work_read_bin);   
}

/**
 * @brief Capture unsolicited binary data
 */
static void sample_urc_bin_data(void)
{ 
    int i;
    unsigned char check;            //数据校验码
    int           len;              
    char buf[256];
    char head[32];
    srand((unsigned)time(NULL));
    len = rand() % sizeof(buf);    
    for (i = 1, check = 0; i < len; i++) {
        
        buf[i] = rand() % 128;
        check ^= buf[i];          //计算校验码
    }
    buf[0] = check;
    //BCC+DATA
    //+IPD,<socket id>,<data length>:.....
    snprintf(head, sizeof(head),"+IPD,%d,%d:",rand() % 16, len);
    at_device_emit_urc(head, strlen(head));
    at_device_emit_urc(buf, len);    
}

#if AT_WORK_CONTEXT_EN
/**
 * @brief 发送命令(同步方式)
 * @param respbuf 响应缓冲区
 * @param bufsize 缓冲区大小
 * @param timeout 超时时间
 * @param cmd 命令
 * @retval 命令执行状态
 */
static at_resp_code at_send_cmd_sync(char *respbuf, int bufsize, int timeout, const char *cmd, ...)
{
    at_attr_t    attr;
    at_context_t ctx;
    va_list      args; 
    bool         ret;
    //属性初始化
    at_attr_deinit(&attr);
    attr.timeout = timeout;
    attr.retry   = 1;
    //初始化context
    at_context_init(&ctx, respbuf, bufsize); 
    //为工作项绑定context
    at_context_attach(&attr, &ctx);
    
    va_start(args, cmd);
    ret = at_exec_vcmd(at_obj, &attr, cmd, args);
    va_end(args);

    if (!ret) {
         return AT_RESP_ERROR;     
    }

    //等待命令执行完毕
    while (!at_work_is_finish(&ctx)) {  
        usleep(1000);
    }    
    return at_work_get_result(&ctx);
}

/**
 * @brief 基于上下文实现同步接口
 */
static void sample_at_sync(void)
{
    int rssi, error_rate;
    char csqbuf[64];
    at_resp_code code;
    code = at_send_cmd_sync(csqbuf, sizeof(csqbuf), 200, "AT+CSQ");
    if (code == AT_RESP_OK) {
        sscanf(csqbuf, "+CSQ:%d,%d", &rssi, &error_rate);
        printf("The CSQ value is read successfully\r\n=>rssi:%d, error_rate:%d\r\n", rssi, error_rate);
    } else if (code == AT_RESP_TIMEOUT) {
        printf("The CSQ read timeout.\r\n");
    } else {
        printf("The CSQ read failed.\r\n");
    }
}

#endif
/**
 * @brief 测试用例
 */
static const test_item_t test_cases[] = {
    {sample_show_memory,        "Display memory information."},
    {sample_singlline,          "Testing singlline command."},
    {sample_multiline,          "Testing multiline commands."},
    {sample_variable_param_cmd, "Testing variable parameter command."},
    {sample_timeout_retry,      "Testing response timeout retry."},
    {sample_error_retry,        "Testing response error retry."},    
    {sample_abort_command,      "Testing command abort."},
    {sample_specific_prefix,    "Testing specific response prefix."},
    {sample_custom_cmd,         "Testing custom command."},
    {sample_send_buffer,        "Testing buffer send."},
    {sample_at_work_1,          "Testing 'at_do_work' 1."},    
    {sample_read_bin_data,      "Testing read binary data via 'at work'."},
    {sample_urc_bin_data,       "Testing capture unsolicited binary data."},
#if AT_WORK_CONTEXT_EN    
    {sample_at_sync,            "Testing at context interface."},
#endif
};

/**
 * @brief 显示测试用例
 */
static void show_test_case(void)
{
    int i;
    printf("Please input test item:\r\n");
    for (i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
        printf("\t%d:%s\r\n", i + 1, test_cases[i].brief);
    printf("*******************************************************\r\n");   
}

/**
 * @brief 显示测试用例
 */
static void input_process(void)
{
    const test_item_t *item;
    char buf[128];
    int  num;        

    fgets(buf, sizeof(buf), stdin);
    num = atoi(buf);
    if (num > 0 && num <= sizeof(test_cases) / sizeof(test_cases[0])) {
        item = &test_cases[num - 1];
        printf("Start '%s'\r\n", item->brief); 
        item->handler();
    } else {
        show_test_case();
    }
}


/**
 * @brief at适配器
 */
static const at_adapter_t at_adapter = {
    .lock          = at_mutex_lock,
    .unlock        = at_mutex_unlock,
    .write         = at_device_write,
    .read          = at_device_read,    
    .error         = at_error_handler,
    .debug         = at_debug,
#if AT_URC_WARCH_EN    
    .urc_bufsize   = 300,
#endif
    .recv_bufsize  = 300
};

/** 
 * @brief at通信处理线程
 */
static void *at_thread(void *args)
{
    pthread_detach(pthread_self());
    printf("at thread running...\r\n");    
    while(1) {
        usleep(1000);
        at_obj_process(at_obj);
    }
    return NULL;
}

int main(int argc, char **argv)
{
    pthread_t     tid;
    at_obj = at_obj_create(&at_adapter);
    if (at_obj == NULL) {
        printf("at object create failed\r\n");
        _exit(0);
    }     
    at_obj_set_urc(at_obj, urc_table,  sizeof(urc_table) / sizeof(urc_table[0]) );
    at_device_init();
    pthread_mutex_init(&at_lock, NULL);
    pthread_create(&tid, NULL, at_thread, NULL); 
    printf("*******************************************************\r\n");
    printf("This is an asynchronous AT command framework.\r\n");
    printf("Author:roger.luo, Mail:morro_luo@163.com\r\n");
    printf("*******************************************************\r\n\r\n");
    //Open AT emulator device
    at_device_open();

    show_test_case();
    while(1){
        input_process();
        usleep(10 * 1000);
    }
}
