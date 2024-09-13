/**
 * @Brief: The AT component drives the interface implementation
 * @Author: roger.luo
 * @Date: 2021-04-04
 * @Last Modified by: roger.luo
 * @Last Modified time: 2021-11-27
 */
#include <stddef.h>
#include <stdlib.h>
#include <sys/time.h>

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
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    return (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;
}
