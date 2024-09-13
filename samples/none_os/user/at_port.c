/**
 * @Brief: The AT component drives the interface implementation
 * @Author: roger.luo
 * @Date: 2021-04-04
 * @Last Modified by: roger.luo
 * @Last Modified time: 2021-11-27
 */
#include "platform.h"
#include <stddef.h>
#include <stdlib.h>

/**
 * @brief Custom malloc for AT component.
 */
void *at_malloc(unsigned int nbytes)
{
    return malloc(nbytes);
}

/**
 * @brief Custom free for AT component.
 */
void  at_free(void *ptr)
{
    free(ptr);
}

/**
 * @brief Gets the total number of milliseconds in the system.
 */
unsigned int at_get_ms(void)
{
   return get_tick();
}
