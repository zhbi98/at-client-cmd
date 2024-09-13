# 高级教程

了解完一般命令处理之后，下面我们来讲一下一些特殊场景下的AT命令请求及其处理方式，以及了解处理这些请求涉及到的相关概念和接口，同时我也会详细说明有关URC消息处理办法和在OS上的应用。

本节导读如下：
- 组合命令处理
- 自定义AT作业
- URC消息处理
- 多实例并存
- AT作业上下文
- OS应用之异步转同步
- 内存监视器

## 组合命令处理

在一些场景下，主机与从机之前间需要通过组合命令来交换信息，即主机与从机间完成一次业务需要进行多次命令交互，例如sms收发，socket数据收发，不同模组产商命令可能是不一样的，下面是几个组合命令的例子。

发送短信流程(参考SIM900A模组)：

```shell
主机  =>
      AT+CMGS=<Phone numer> + \x1A     //发送目标手机号+ctrl+z
从机  <=
      '<'                              //从机回复提示符'<'
主机  =>   
      <sms message>                    //发送短信内容
从机  <=
      OK                               //从机回复OK
```

发送Socket数据流程(参考移远EC21模组)：
```shell
主机  =>
      AT+QISEND=<connectID>，<send_length> 
从机  <=
      '<'                              //从机回复提示符'<'
主机  =>   
      <data>                           //发送二进制数据内容
      \x1A                             //发送CTRL+z启动发送
从机  <=
      OK                               //从机回复OK
```

发送TCP Socket数据流程(参考Sierra 模组)：
```shell
主机  =>
      AT+KTCPSND=<session>,<send_length> 
从机  <=
      'CONNECT'                        //从机回复提示符'CONNECT'
主机  =>   
      <data>                           //发送二进制数据内容
      "--EOF--Pattern--""              //发送结束符
从机  <=
      OK                               //从机回复OK
```

接收TCP Socket数据(参考Sierra 模组)：
```shell
主机  =>
      AT+KTCPRCV=<session_id>,<recv_length> 
从机  <=
      'CONNECT'                        //从机回复提示符'CONNECT'
      <data>                           //二进制数据内容
      "--EOF--Pattern--""              //结束符      
      OK                               //状态码
```

组合命令与一般AT命令最大的不同体现在命令交互的流程结构上，它们进行一次通信(数据)业务时需要交互2次或多次以上，所以软件设计需要考虑并发冲突的可能，因为在进行某项业务交互时如果需要进行两次AT命令请求才能完成，有可能在进行第一次时过程中就被其它命令穿插打断了，从而造成该项业务执行失败。


对于这个问题，除了利用OS的锁解决之外，还可以利用自定义作业来处理，它允许你在一个AT作业中进行多次命令交互，同时能够让应用层自行控制命令收发流程。

## 自定义AT作业

自定义AT作业使用`at_work_t`表示，它实际是一个状态机轮询程序，通过'at_env_t'参数提供了基本的状态变量，数据收发，超时管理等接口。除了能用来收发命令之外，你甚至可以用它来控制硬件IO,例如有时序控制要求开关机操作，这样做也有利于处理设备状态同步的问题。

**原型定义如下:**
```c
/**
 *@brief   AT作业轮询处理程序
 *@param   env  AT作业的公共运行环境，包括一些通用的变量和进行AT命令通信所需的相关接口。
 *@return  作业处理状态, 它决定了是否在下一个循环中是否继续运行该作业。
 *     @arg true  指示当前作业已经处理完成，可以被中止，同时作业的状态码会被设置为AT_RESP_OK。
 *     @arg false 指示当前作业未处理完成，继续运行。
 *@note   需要注意的是，如果在当前作业中执行了env->finish()操作，则作业立即终止运行。
 */
typedef int  (*at_work_t)(at_env_t *env);

```

其中:
`at_env_t` 定义了一些通信上下文环境相关接口与公共状态变量，通过它你可以实现自己的通信交互逻辑。

```c
/**
 * @brief AT作业公共运行环境
 */
typedef struct at_env {
    struct at_obj *obj;      
    //公共状态(根据需要添加),每次新作业启动时，这些值会被重置
    int i, j, state;     
    //附属参数(引用自->at_attr_t)  
    void        *params;
    //设置下一个轮询等待间隔(只生效1次)
    void        (*next_wait)(struct at_env *self, unsigned int ms);
    //复位计时器
    void        (*reset_timer)(struct at_env *self);               
    //作业超时判断
    bool        (*is_timeout)(struct at_env *self, unsigned int ms);
    //带换行的格式化打印输出  
    void        (*println)(struct at_env *self, const char *fmt, ...);
    //接收内容包含判断      
    char *      (*contains)(struct at_env *self, const char *str); 
    //获取接收缓冲区       
    char *      (*recvbuf)(struct at_env *self);
    //获取接收缓冲区长度  
    unsigned int(*recvlen)(struct at_env *self);
    //清空接收缓冲区    
    void        (*recvclr)(struct at_env *self);
    //指示当前作业是否已被强行终止
    bool        (*disposing)(struct at_env *self);
    //结束作业,并设置响应码
    void        (*finish)(struct at_env *self, at_resp_code code);
} at_env_t;
```

**示例1:(Sierra 模组发送socket数据)**

命令格式:
```shell
主机  =>
      AT+KTCPSND=<session>,<send_length> 
从机  <=
      'CONNECT'                        //从机回复提示符'CONNECT'
主机  =>   
      <data>                           //发送二进制数据内容
      "--EOF--Pattern--""              //发送结束符
从机  <=
      OK                               //从机回复OK
```
**代码实现:**
```c

//socket定义
typedef struct {
    //....
    int id;
    unsigned char *sendptr;
    int sendcnt;
    //...
}socket_t;

/*
 * @brief       socket 数据发送处理
 * @return      true - 结束运行 false - 保持运行
 */
static int socket_send_handler(at_env_t *env)
{
    socket_t *sk = (socket_t *)env->params;
    switch (env->state) {
        case 0:
            env->println(env, "AT+KTCPSND=%d,%d", sk->id, sk->sendcnt);                       
            env->reset_timer(env);                               /*重置定时器*/
            env->state++; 
        break;
        case 1:
            if (env->contains(env, "CONNECT")) {
                env->obj->adap->write(sk->sendptr, sk->sendcnt); /*发送数据*/    
                env->println(env, "--EOF--Pattern--");            /*发送结束符*/                   
                env->reset_timer(env);
                env->recvclr(env);
                env->state++;                
            } else if (env->contains(env, "ERROR")) {            /*匹配到错误，结束作业*/
                env->finish(env, AT_RESP_ERROR);
            } else if (env->is_timeout(env, 1000)) {
                if (++env->i > 3) {
                    env->finish(env, AT_RESP_ERROR);           
                }
                env->state--;                                    /*重新发送*/                
            }            
        break;   
        case 2:
            if (env->contains(env, "OK"))                        
                env->finish(env, AT_RESP_OK);                    /*发送成功，设置状态为OK后退出*/
            else if (env->contains(env, "ERROR") ||
                     env->is_timeout(env, 1000)) {
                env->finish(env, AT_RESP_ERROR);   
            }          
        break;          
    }
    return 0;    
}


/**
 * @brief socket数据发送请求
 */
static void sock_send_data(socket_t *sock)
{
    at_do_work(at_obj, sock, socket_send_handler);              
}
```

## URC消息处理

未经请求主动上报的消息，又称URC(Unsolicited Result Code)，在主机方未下发送命令请求的情况下，设备会根据自身的运行状态或者事件主动上报消息给主机。

根据URC消息格式的不同,可以分为以下几类:
1. 对于大多数URC消息，通常是单行输出的，一般是以“+”为前缀，回车换行结束,例如:


```c
+SIM: 1 \r\n                          //SIM卡状态

+CREG: 1,"24A4","000012CF",1\r\n      //网络注册状态更新

```

2. 也有不带前缀'+'的URC：

```c
RDY \r\n                              //开机就绪
```

```c
WIFI DISCONNECTED                     //WIFI断开
```

3. 非回车换行结束的URC

socket数据接收(参考sim800 模组)。

```c
+IPD,<socket id>,<data length>:<bin data>

```

可以看到，每个URC消息都有其特定的前缀信息，前两类是以'\n'作为结束符的，最后一种只有前缀，而整个URC消息帧是可变长度的，没有特定的结束符。那么该如何将这几类消息识别并提取出来的呢？实际上URC处理程序也是通过匹配"前缀"+"结束符"的方式来提取URC消息的，对于前两种消息，它们都有特定的结束符('\n')，只要匹配到消息"前缀"+"后缀"就可以将整条消息完整的提取出来；而对于最后一种消息，由于没有固定的结束符，而且消息长度是可变的，所以URC处理程序无法一次性将整条消息匹配并提取出来，往往需要分成多次进行，第一次是匹配它的头部信息，然后再由订阅者(通过回调)告诉它剩余待接收的数据长度，当剩余数据长度为0时，则该则URC消息接收完毕。这种处理方法其实同样也适用于前两类URC，不过这些消息的剩余数据长度为0罢了。

如下所示，我们可以把第3类URC消息拆分成两部分看待，固定头部+剩余数据。

![带二进制URC消息](images/urcbin.png)

由于URC具有一定的随机性，所以URC处理程序会实时读取来自设备端的上报的信息，然后再进行消息匹配处理。至于匹配哪种URC消息，则是由用户通过`at_obj_set_urc`设置的表来决定的。另外，考虑到性能的原因，URC处理程序不会一接收到任何一个字符都会启动匹配程序工作，而是遇到`AT_URC_END_MARKS`中规定的字符才会触发执行。

下面是`at_obj_set_urc`原型：
```c
/**
 * @brief   Set the AT urc table.
 */
void at_obj_set_urc(at_obj_t *at, const urc_item_t *tbl, int count);

```

### URC处理项(urc_item_t)

URC表的基本单位是`urc_item_t`,它用于描述每一条URC消息的处理规则，包括了消息头，结束标志还有该消息的处理程序。

```c
/**
 * @brief urc处理项
 */
typedef struct {
    const char *prefix;            /* 需要匹配的帧前缀,如+CSQ:25,*/
    const char  endmark;           /* urc结束标志(参考@AT_URC_END_MARKS*/
    /**
     * @brief   urc处理程序(prefix与endmark满足时触发)
     * @params  ctx   - 上下文(Context)
     * @return  表示当前URC帧剩余未接收字节数
     *          @arg 0 当前URC帧已接收完成,可以接收下一个URC
     *          @arg n 仍需要等待接收n个字节(AT管理器继续接收剩余数据并继续回调此接口)
     */    
    int (*handler)(at_urc_ctx_t *ctx);
} urc_item_t;

```
其中`at_urc_ctx_t`存储URC的接收状态和数据。

```c
/**
 * @brief URC 上下文(Context) 定义
 */
typedef struct {
    urc_recv_state state;          /* urc接收状态*/
    char *urcbuf;                  /* urc数据缓冲区 */
    int   urclen;                  /* urc缓冲区已接收数据长度*/
} at_urc_ctx_t;
```

**示例1(WIFI断开连接)：**

消息格式:
```c
WIFI DISCONNECTED <\r\n>
```
代码实现：
```c

/**
 * @brief   wifi断开事件
 */
static int wifi_connected_handler(at_urc_info_t *info)
{
    printf("WIFI connection detected...\r\n");
    return 0;
}
//...
/**
 * @brief urc订阅表
 */
static const urc_item_t urc_table[] = 
{
    //其它URC...
    {.prefix = "WIFI DISCONNECTED",      .endmark = '\n',  .handler = wifi_connected_handler}
};
```

**示例2(socket数据接收)：**

URC消息格式:
```c
+IPD,<socket id>,<data length>:.....
```
代码实现：
```c
/**
 * @brief socket数据接收处理
 */
static int urc_socket_data_handler(at_urc_info_t *ctx)
{
    int  length, total_length, sockid, i;
    char *data;    
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
        printf("%d bytes of data were received form socket %d!\r\n", length, sockid);
    }
    return 0;
}

//....

/**
 * @brief urc订阅表
 */
static const urc_item_t urc_table[] = 
{
    //其它URC...
    {.prefix = "+IPD,",      .endmark = ':',  .handler = urc_socket_data_handler}
};

```

## 多实例并存

AT通信对象并不只限于一个, at_obj_create允许你在同一个系统中创建多个共存的AT通信设备，而且每个都拥有自己独立的用于配置和资源.

```c

//wifi 适配器
const at_adapter_t adap_wifi = {
    //...
};

//modem适配器
const at_adapter_t adap_modem = {
    //...
};

//...
at_obj_t *at_modem = at_obj_create(&modem_adapter);

at_obj_t *at_wifi = at_obj_create(&wifi_adapter);

//...

//轮询任务

/**
 * @brief AT轮询程序
 */
void at_device_process(void)
{
    static unsigned int timer = 0;
    //(为了加快AT命令处理响应速度,建议5ms以内轮询一次)
    if (at_get_ms() - timer > 5) {
        timer = at_get_ms();
        at_obj_process(at_modem);
        at_obj_process(at_wifi);  
    }
}

```
## AT作业上下文(at_context_t)

使用异步的一个弊端是它会让程序执行状态过于分散，增加程序编码和理解难度。比如一些代码需要根据异步的结果来执行下一步动作，一般是在异步回调中添加对应的状态标识，然后主程序根据这些状态标识来控制状态机或者程序分支的跳转，这使得代码间没有明显的流程线，代码执行流不好追踪管理。那么，在不使用同步或者不支持OS的情况下，如何避免异步带来的状态分散问题? 一种比较常用的方式是使用状态机轮询法，通过实时查询每一个异步请求的状态，并根据根据上一个结果执行下一个请求，这样代码执行上下文就紧密衔接在一块了，达到类似同步的效果。

对于异步AT请求，可以使用`at_context_t`获取其实时信息，包含当前的作业运行状态，命令执行结果及命令响应信息。使用AT上下文相关功能需要先启用`AT_WORK_CONTEXT_EN`宏, 相关API定义如下：

| 函数原型                                                     | 说明                                             |
| ------------------------------------------------------------ | ------------------------------------------------ |
| void at_context_init(at_context_t *ctx, void *respbuf, unsigned bufsize); | 初始化一个`at_context_t`同时设置命令接收缓冲区。 |
| void at_context_attach(at_attr_t *attr, at_context_t *ctx);  | 将`at_context_t`绑定到AT属性中。                 |
| at_work_state at_work_get_state(at_context_t *ctx);          | 通过`at_context_t`获取AT作业运行状态。           |
| bool at_work_is_finish(at_context_t *ctx);                   | 通过`at_context_t`获取AT作业完成状态。           |
| at_resp_code  at_work_get_result(at_context_t *ctx);         | 通过`at_context_t`获取AT请求状态码。             |

## OS应用之异步转同步

如果是在OS环境下使用，很多时候我们更希望采用同步的方式执行AT命令请求，即原地等待命令执行完成，下面介绍两种异步转同步的方式。
### 轮询方式

轮询方式主要是基于`at_context_t`实现的，发送请求前通过绑定一个`at_context_t`实时监视命令的执行状态，然后循环检测命令是否执行完成。

示例：
```c
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
    //初始化context,并设置响应缓冲区
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

```

### 信号量方式

使用轮询方式会造成CPU空转，可以通过使用信号量来优化，不过实现起来比轮询方式要稍微复杂一些，

示例：
```c
//...
#include "at_chat.h"
#include <pthread.h>
#include <semaphore.h>

typedef struct {
    pthread_mutex_t cmd_lock;         //命令锁
    sem_t           sem_finish;       //完成信号    
    char            *recvbuf;         //命令接收缓冲区
    unsigned short  bufsize;          //缓冲区大小
    unsigned short  recvcnt;          //接收计数器
    at_resp_code    resp_code;        //命令响应码
} at_watch_t;

static at_watch_t at_watch;

/**
 * @brief 命令锁及信号相关初始化
 */
void at_sync_init(void)
{
    sem_init(&at_watch.sem_finish, 0, 0);
    pthread_mutex_init(&at_watch.cmd_lock, NULL);    
}

/**
 * @brief AT命令响应处理
 */
static void at_callback_handler(at_response_t *r)
{
    int cnt = r->recvcnt;
    at_watch.resp_code = r->code;
    if (at_watch.recvbuf != NULL) {
        if (cnt > at_watch.bufsize) {
            //接收缓存不足
            cnt = at_watch.bufsize;
        }
        memcpy(at_watch.recvbuf, r->recvbuf, cnt);
        at_watch.recvcnt = cnt;
    }
    //通知命令已执行完毕
    sem_post(&at_watch.sem_finish);
}


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
    va_list      args; 
    bool         ret;
    //属性初始化
    at_attr_deinit(&attr);
    attr.timeout = timeout;
    attr.retry   = 1;
    attr.cb      = at_callback_handler;
    va_start(args, cmd);

    pthread_mutex_lock(&gw.sync_cmd_lock);
    //设置接收缓冲区
    at_watch.recvbuf = respbuf;
    at_watch.bufsize = bufsize;
    at_watch.recvcnt = 0;
    at_watch.resp_code = AT_RESP_ERROR;
    if (at_exec_vcmd(at_obj, &attr, cmd, args)) {
        sem_wait(&at_watch.sem_finish);   //等待命令执行完成
    }
    pthread_mutex_unlock(&gw.sync_cmd_lock);

    va_end(args); 

    return at_watch.resp_code;
}

```
## 内存监视器
嵌入式系统的内存资源极其有限，不当的使用动态内存，除了产生内存碎片、内存泄露这些问题外，严重时会导致死机，崩溃等事故，所以在使用动态内存时有必要加上一定限制手段，确保系统在一定安全边际下正常运行。`AT_MEM_LIMIT_SIZE`规定了AT请求所用的最大内存数量，这样可以避免程序异常执行时过度执行AT请求导致内存不足的问题。至于分配多少主要取决于你的应用，如果你一开始并不确定用多少比较合适，可以先设置一个相对来说大一些的值，然后让程序运行一段时间观察使用情况再设置，通过`at_max_used_memory`和`at_cur_used_memory`可以获取历史最大内存使用量和当前内存使用量。