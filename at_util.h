/******************************************************************************
 * @brief    at模块OS相关移植接口
 *
 * Copyright (c) 2020, <morro_luo@163.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs: 
 * Date           Author       Notes 
 * 2020-01-02     Morro        初版
 ******************************************************************************/

#ifndef _ATUTIL_H_
#define _ATUTIL_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef void *at_sem_t;                                /*信号量*/

/*
 * @brief	   获取当前系统毫秒数
 */
static inline unsigned int at_get_ms(void)
{
    /*Add  here...*/
    return 0;
}
/*
 * @brief	   超时判断
 * @retval     true | false
 */
static inline bool at_istimeout(unsigned int start_time, unsigned int timeout)
{
    return at_get_ms() - start_time > timeout;
}

/*
 * @brief	   毫秒延时
 * @retval     none
 */
static inline void at_delay(uint32_t ms)
{
    
}
/*
 * @brief	   创建信号
 * @retval     none
 */
static inline at_sem_t at_sem_new(int value)
{
    return NULL;
}
/*
 * @brief	   等待信号
 * @retval     none
 */
static inline bool at_sem_wait(at_sem_t s, uint32_t timeout)
{
    return false;
}

/*
 * @brief	   释放信号
 * @retval     none
 */  
static inline void at_sem_post(at_sem_t s)
{
    
}

/*
 * @brief	   销毁的信号
 * @retval     none
 */  
static inline void at_sem_free(at_sem_t s)
{
    
}

#endif