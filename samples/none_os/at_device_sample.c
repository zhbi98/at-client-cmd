/**
 * @file at_device.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "at_chat.h"
#include "at_port.h"
#include "time.h"
#include "log.h"

/**********************
 *  STATIC VARIABLES
 **********************/

/**
 * Construct an AT device adapter that provides 
 * the specified communication data read-write 
 * interface and other RTOS-related 
 * Operation Interfaces
 */
static const at_adapter_t at_adapter = {
    .lock          = NULL,            /**< Multi-tasking lockout (non-oss sub-filler)*/
    .unlock        = NULL,            /**< Multi-tasking unlock (non-oss sub-filler)*/
    .write         = at_device_write, /**< Serial Data Write Interface (Non-Blocking)*/
    .read          = at_device_read,  /**< Serial Data Read Interface (Non-Blocking)*/
    .debug         = NULL,            /**< Debugging the print interface (NULL if not needed)*/
    .recv_bufsize  = 256              /**< Receive buffer size (fill in as required)*/
};

static volatile uint32_t _at_tick = 0;
at_obj_t * at_obj_p = NULL;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Each task callback function is timed and used to 
 * execute the corresponding callback function at the end of time.
 */
void at_task_tick_inc(uint32_t tick_period)
{
    _at_tick += tick_period;
}

/**
 * Each task callback function is timed and used to 
 * execute the corresponding callback function at the end of time.
 */
uint32_t at_task_get_tick()
{
    return _at_tick;
}

/**
 * Construct an AT device adapter that provides 
 * the specified communication data read-write 
 * interface and other RTOS-related 
 * Operation Interfaces
 */
uint32_t at_device_write(const void * data_p, uint32_t len)
{
    int8_t * _data_p = (int8_t *)data_p;
    uint8_t res = false;

    logger("dev write:%s", _data_p);

    for (uint32_t i = 0; i < len; i++)
        put_txd4_queue(_data_p[i]);

    sleep_us(1);

    return len;
}

/**
 * Construct an AT device adapter that provides 
 * the specified communication data read-write 
 * interface and other RTOS-related 
 * Operation Interfaces
 */
uint32_t at_device_read(void * data_p, uint32_t len)
{
    int8_t * _data_p = (int8_t *)data_p;
    uint8_t res = false;
    uint32_t i = 0;

    for (i = 0; i < len; i++) {
        res = get_rxd4_queue(&_data_p[i]);
        if (!res) break;
    }

    sleep_us(1);

    return i;
}

void at_device_tick_work()
{
    static uint32_t _last_tick = 0;
    static uint32_t _tick = 0;

    _tick = at_task_get_tick();

    /*info("_tick:%d, _last_tick:%d", 
        _tick, _last_tick);*/

    if (_tick - _last_tick > 10) {
        _last_tick = at_task_get_tick();
        at_obj_process(at_obj_p);
    }
}

int32_t main()
{
    at_obj_p = at_obj_create(&at_adapter);
    if (at_obj_p != NULL)
        info("AT Dev Inited");
}

/**
 * Each task callback function is timed and used to 
 * execute the corresponding callback function at the end of time.
 */
void SysTick_Handler()
{
    at_task_tick_inc(10);
}

/**
 * Each task callback function is timed and used to 
 * execute the corresponding callback function at the end of time.
 */
void at_device_poll()
{
    at_device_tick_work();
}

/**
 * @brief Command response handler.
 */
void at_req_callback(at_response_t * resp_p)
{
    uint32_t baud = 0;
    int8_t verbuf[32] = {0};

    if (resp_p->code == AT_RESP_OK)
        info("req-OK:%s", resp_p->prefix);
    else info("req-Fail");
}

/**
 * @brief Command response handler.
 */
void _at_req_callback(at_response_t * resp_p)
{
    uint32_t baud = 0;
    int8_t verbuf[32] = {0};

    if (resp_p->code == AT_RESP_OK)
        info("req-OK:%s", resp_p->prefix);
    else info("req-Fail");
}

at_attr_t at_attr = {
    .params = NULL,
    .prefix = "+LBDADDR",
    .suffix = "OK",
    .cb = at_req_callback,
    .timeout = 1000,
    .retry = 2,
    .priority = 1,
};

at_attr_t _at_attr = {
    .params = NULL,
    .prefix = "+BAUD",
    .suffix = "OK",
    .cb = _at_req_callback,
    .timeout = 1000,
    .retry = 2,
    .priority = 1,
};

/**
 * @brief Command response handler.
 */
void at_read_lbd()
{
    /*Send the command with a timeout of 1000ms 
    and a repeat count of 0*/
    at_send_singlline(at_obj_p, 
        &at_attr, "AT+LBDADDR?");
}

/**
 * @brief Command response handler.
 */
void at_read_baud()
{
    /*Send the command with a timeout of 1000ms 
    and a repeat count of 0*/
    at_send_singlline(at_obj_p, 
        &_at_attr, "AT+BAUD?");
}
