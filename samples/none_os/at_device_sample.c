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

/*********************
 *      DEFINES
 *********************/

#define AT_TICKS(v1, v2, quality) \
    ( \
        (uint32_t)( \
            ((uint32_t)(v1) * (uint32_t)(quality)) / \
            ((uint32_t)(v2)) \
        ) \
    )

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

static uint8_t at_cmd[64] = {0};
static volatile uint32_t _at_tick = 0;
at_obj_t * at_obj_p = NULL;

static uint32_t at_busy_tick_val = 0;
static bool at_device_busy = 0;

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
    at_device_busy_reload();

    for (uint32_t i = 0; i < len; i++)
        put_txd4_queue(_data_p[i]);

    /**Wait a bit to avoid sending errors*/
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

    /**Wait a bit to avoid sending errors*/
    sleep_us(1);

    return i;
}

/**
 * Construct an AT device adapter that provides 
 * the specified communication data read-write 
 * interface and other RTOS-related 
 * Operation Interfaces
 */
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

/**
 * Construct an AT device adapter that provides 
 * the specified communication data read-write 
 * interface and other RTOS-related 
 * Operation Interfaces
 */
bool at_device_get_object(at_obj_t ** at_dev_p)
{
    if (at_obj_p != NULL) {
        *at_dev_p = at_obj_p;
        return true;
    }
    return false;
}

/**
 * Defines an AT instruction property that holds the 
 * information associated with each instruction.
 */
static at_attr_t at_attr = {
    .params = NULL,
    .prefix = NULL,
    .suffix = NULL,
    .cb = NULL,
    .timeout = 1200,
    .retry = 1,
    .priority = 1,
};

/**
 * Gets the current status of the Bluetooth device, 
 * which can be updated to the graphical interface to provide 
 * appropriate interaction.
 */
uint32_t at_device_get_timeout()
{
    return at_attr.timeout;
}

/**
 * Reload a timeout reset timer, ready 
 * to receive data frame.
 */
static void at_device_busy_reload()
{
    uint32_t time = at_device_get_timeout();
    uint32_t val = AT_TICKS(time, 10, 1) + 50;

    at_busy_tick_val = val;
    at_device_busy = 1;
}

/**
 * Data communication error (usually a format error) that 
 * resets the state after a timeout. This function is
 * placed in a timed interrupt to execute.
 */
static void at_device_busy_expired()
{
    /*The device connection 
    status has expired*/
    at_device_busy = 0;
}

/**
 * Device Connection status, used to display 
 * identification, and operational reference.
 */
bool at_device_is_busy()
{
    return at_device_busy;
}

/**
 * The timeout timer updates the function, placing
 * the function intoa timed interrupt function to execute.
 */
void at_device_busy_tick_work()
{
    if (at_busy_tick_val == 0xFFFFFFFFU) 
        return;

    /*A frame of data received, off the timer, 
    ready to process the data*/
    if (at_busy_tick_val > 0) 
        at_busy_tick_val--;
    /*If the counter is an unsigned number, 
    you need to use judgement, otherwise it is 
    easy to have an abnormal negative value*/

    if (at_busy_tick_val > 0) return;

    /*at_busy_tick_val = _AT_TIM_INV;*/
    at_busy_tick_val = 0xFFFFFFFFU;

    /*The packet frame interval is up 
    and the bus status update begins*/
    at_device_busy_expired();
}

/**
 * Constructs the AT instruction to be sent, 
 * changing the instruction parameter entry in 
 * the form of a specified parameter fill-in.
 */
void build_bdaddr_atcmd()
{
    at_attr.prefix = "+LBDADDR";
    at_attr.suffix = "OK";
    at_attr.cb = _at_bdaddr_cmd;

    memset(at_cmd, '\0', 64);
    snprintf(at_cmd, 64, "AT+LBDADDR?");

    /*Send the command with a timeout of 1000ms 
    and a repeat count of 0*/
    at_send_singlline(at_obj_p, 
        &_at_attr, at_cmd);
}

/**
 * Constructs the AT instruction to be sent, 
 * changing the instruction parameter entry in 
 * the form of a specified parameter fill-in.
 */
void build_baudrate_atcmd()
{
    at_attr.prefix = "+BAUD";
    at_attr.suffix = "OK";
    at_attr.cb = _at_baudrate_cmd;

    memset(at_cmd, '\0', 64);
    snprintf(at_cmd, 64, "AT+BAUD?");

    /*Send the command with a timeout of 1000ms 
    and a repeat count of 0*/
    at_send_singlline(at_obj_p, 
        &_at_attr, at_cmd);
}

/**
 * Process the AT device's response results, 
 * such as updating some status, updating 
 * UI content or identity, and so on.
 */
static void _at_bdaddr_cmd(at_response_t * resp_p)
{
    const int8_t * prefix_p = NULL;
    const int8_t * suffix_p = NULL;
    uint8_t code = resp_p->code;

    /**Disable AT MODE data channel entry to prevent 
    other data from entering and disturbing AT mode*/
    read_atcmd = false;

    if (code == AT_RESP_TIMEOUT) {
        info("req-Timeout");
        return;
    }

    if (code == AT_RESP_ERROR) {
        info("req-Error");
        return;
    }

    if (code == AT_RESP_OK) {
        info("req-OK");
    }

    const int8_t * prefix = resp_p->prefix;
    const int8_t * suffix = resp_p->suffix;
    
    prefix_p = strstr(prefix, "+LBDADDR");
    suffix_p = strstr(suffix, "OK");
}

/**
 * Process the AT device's response results, 
 * such as updating some status, updating 
 * UI content or identity, and so on.
 */
static void _at_baudrate_cmd(at_response_t * resp_p)
{
    const int8_t * prefix_p = NULL;
    const int8_t * suffix_p = NULL;
    uint8_t code = resp_p->code;

    /**Disable AT MODE data channel entry to prevent 
    other data from entering and disturbing AT mode*/
    read_atcmd = false;

    if (code == AT_RESP_TIMEOUT) {
        info("req-Timeout");
        return;
    }

    if (code == AT_RESP_ERROR) {
        info("req-Error");
        return;
    }

    if (code == AT_RESP_OK) {
        info("req-OK");
    }

    const int8_t * prefix = resp_p->prefix;
    const int8_t * suffix = resp_p->suffix;
    
    prefix_p = strstr(prefix, "+BAUD");
    suffix_p = strstr(suffix, "OK");
}

int32_t main()
{
    at_obj_p = at_obj_create(&at_adapter);
    if (at_obj_p != NULL)
        info("AT Device initialize OK.");
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
