/**
 * @Brief: The AT component drives the interface implementation
 * @Author: roger.luo
 * @Date: 2021-04-04
 * @Last Modified by: roger.luo
 * @Last Modified time: 2021-11-27
 */
#ifndef __AT_PORT_H__
#define __AT_PORT_H__

/**
 *@brief Default correct response identifier.
 */
#define AT_DEF_RESP_OK    "OK"     
/**
 *@brief Default error response identifier.
 */
#define AT_DEF_RESP_ERR   "ERROR"

/**
 *@brief Default command timeout (ms)
 */
#define AT_DEF_TIMEOUT    500  

/**
 *@brief Number of retries when a command timeout/error occurs.
 */
#define AT_DEF_RETRY      2   

/**
 *@brief Default URC frame receive timeout (ms).
 */
#define AT_URC_TIMEOUT    500

/**
 *@brief Maximum AT command send data length (only for variable parameter commands).
 */
#define AT_MAX_CMD_LEN    256     

/**
 *@brief Maximum number of work in queue (limit memory usage).
 */
#define AT_LIST_WORK_COUNT 32     
 
/**
 *@brief Enable URC watcher.
 */
#define AT_URC_WARCH_EN     1

/**
 *@brief A list of specified URC end marks (fill in as needed, the fewer the better).
 */
#define AT_URC_END_MARKS  ":,\n"
/**
 *@brief Enable memory watcher.
 */
#define AT_MEM_WATCH_EN     1u
  
/**
 *@brief Maximum memory usage limit (Valid when AT_MEM_WATCH_EN is enabled)
 */
#define AT_MEM_LIMIT_SIZE   (3 * 1024)

/**
 *@brief Enable AT work context interfaces.
 */
#define AT_WORK_CONTEXT_EN  1u

/**
 * @brief Supports raw data transparent transmission
 */
#define AT_RAW_TRANSPARENT_EN  1u

void *at_malloc(unsigned int nbytes);

void  at_free(void *ptr);

unsigned int at_get_ms(void);

#endif
