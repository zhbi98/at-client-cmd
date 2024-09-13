
# 平台移植及配置

在使用前,你需要先进行平台移植,下面是下整个组件的目录结构:
```shell

at_chat
│
|───docs          使用文档
|───samples       演示案例
|───inlcude       头文件目录
|    │───at_chat.h
|    │───at_port.h
|    │───linux_list.h
└───src
    │───at_port.c
    │───at_chat.c    
```

## 平台移植

移植只涉及到3个接口实现及若干配置定义(at_port.h中声明了这些需要实现的接口及配置).

```c

//内存申请,释放
void *at_malloc(unsigned int nbytes);

void  at_free(void *ptr);
//获取系统毫秒数(从开机开始)
unsigned int at_get_ms(void);

```

### 内存相关接口实现

```C

void *at_malloc(unsigned int nbytes)
{
	return malloc(nbytes);
}

void  at_free(void *ptr)
{
	free(ptr);
}
 
```

### 系统时间获取接口实现


**如果你使用的是MCU,可以通过定时器计数方式实现，参考下面的例程:**

```c
/* 滴答计数器*/
static volatile unsigned int tick = 0;

/**
 * @brief 定时器中断服务程序(1ms 1次)
 */
void timer_handler(void)
{
    tick++;
}

/**
 * @brief 获取系统毫秒数
 */
unsigned int at_get_ms(void)
{
    return tick;
}

```

**对于linux系统,可以参考下面方式获取:**

```C
/**
 * @brief 获取系统毫秒数
 */
unsigned int at_get_ms(void)
{
    struct timeval tv_now;
    //这里需要注意的是,当系统时间被更改后,会获取到一个错误的值，造成命令超时(代码仅做的演示)。
    gettimeofday(&tv_now, NULL);
    return (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;
}
```

## 配置说明

这些配置项主用于命令交互响应设置,内存使用限制及模块开关等,实际应用时需要考虑你所在系统的资源情况,对于大多数情况,默认值已经够用。

| 配置项             | 默认值     | 配置说明                                                     |
| ------------------ | ---------- | ---------------------------------------------------------- |
| AT_DEF_RESP_OK     | "OK"       | 默认AT命令正确响应,当匹配到此值到,状态码返回AT_RESP_OK         |
| AT_DEF_RESP_ERR    | "ERROR"    | 默认AT命令错误响应,当匹配到此值到,状态码返回AT_RESP_ERR       |
| AT_DEF_TIMEOUT     | 500        | 默认AT响应超时时间(ms),当命令超时时,状态码返回AT_RESP_TIMEOUT |
| AT_DEF_RETRY       | 2          | 当发生AT响应错误或者超时时重发次数                           |
| AT_URC_TIMEOUT     | 1000       | 默认URC帧超时时间(ms)                                       |
| AT_MAX_CMD_LEN     | 256        | 最大命令长度(用于可变参数命令内存限制).                      |
| AT_LIST_WORK_COUNT | 32         | 它规定了同时能够支持的AT异步请求个数, 他可以限制应用程序(使用不当时)短时间内大量突发请求造成内存不足的问题,一般来说8-16已经够用了. |
| AT_URC_WARCH_EN    | 1          | URC消息监视使能                                              |
| AT_URC_END_MARKS   | ":,\n"     | URC结束标记列表,越少越好,因为URC匹配程序会根据此列表对接收到的字符做URC结束帧匹配处理,列表太大会影响程序性能. |
| AT_MEM_WATCH_EN    | 1u         | 内存监视使能                                                 |
| AT_MEM_LIMIT_SIZE  | (3 * 1024) | 内存使用限制                                                 |
| AT_WORK_CONTEXT_EN | 1u         | AT作业上下文相关接口                                          |


