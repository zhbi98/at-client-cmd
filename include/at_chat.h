/******************************************************************************
 * @brief        AT command communication management V2
 *
 * Copyright (c) 2020~2022, <morro_luo@163.com>
 *
 * SPDX-License-Identifier: Apathe-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-01-02     roger.luo    Initial version
 * 2021-01-20     roger.luo    Add debugging interface, and at the same time 
 *                             solve  the problem of uninitialized linked list 
 *                             leading  to segment fault.
 * 2021-03-03     roger.luo    Fix the problem of frequent print receive timeout 
 *                             caused by not clearing the URC counter.
 * 2022-10-01     roger.luo    Redesign the entire AT framework and release V2.0.
 * 2023-01-09     roger.luo    Optimized the processing logic when multi-line 
 *                             command execution fails.
 * 2023-01-10     roger.luo    Resolve URC message containing ':' character 
 *                             causing parsing failure.
 * 2023-02-23     roger.luo    Added a temporary disable interface for URC to 
 *                             solve the problem of URC matching exception when 
 *                             receiving binary data from the modem.
 * 2023-10-27     roger.luo    Added the function of transparent data transmission.
 ******************************************************************************/
#ifndef _AT_CHAT_H_
#define _AT_CHAT_H_

#include "at_port.h"
#include <stdbool.h>
#include <stdarg.h>

struct at_obj;                      
struct at_adapter;                  
struct at_response;                 

/**
 *@brief AT work running state.
 */
typedef enum {
    AT_WORK_STAT_IDLE,              
    AT_WORK_STAT_READY,             
    AT_WORK_STAT_RUN,               
    AT_WORK_STAT_FINISH,            
    AT_WORK_STAT_ABORT,             
} at_work_state;

/**
 *@brief AT command response code
 */
typedef enum {
    AT_RESP_OK = 0,                 
    AT_RESP_ERROR,                  
    AT_RESP_TIMEOUT,                
    AT_RESP_ABORT                   
} at_resp_code;

/**
 *@brief AT command request priority
 */
typedef enum {
    AT_PRIORITY_LOW = 0,
    AT_PRIORITY_HIGH   
} at_cmd_priority;

/**
 *@brief URC frame receiving status
 */
typedef enum {
    URC_RECV_OK = 0,               /* URC frame received successfully. */
    URC_RECV_TIMEOUT               /* Receive timeout (The frame prefix is matched but the suffix is not matched within AT_URC_TIMEOUT) */
} urc_recv_status;

/**
 * @brief URC frame info.
 */
typedef struct {
    urc_recv_status status;        /* URC frame receiving status.*/
    char *urcbuf;                  /* URC frame buffer*/
    int   urclen;                  /* URC frame buffer length*/
} at_urc_info_t;

/**
 * @brief URC subscription item.
 */
typedef struct {
    const char *prefix;            /* URC frame prefix,such as '+CSQ:'*/
    const char  endmark;           /* URC frame end mark (can only be selected from AT_URC_END_MARKS)*/
    /**
     * @brief   URC handler (triggered when matching prefix and mark are match)
     * @params  info   - URC frame info.
     * @return  Indicates the remaining unreceived bytes of the current URC frame.
     *          @retval 0 Indicates that the current URC frame has been completely received
     *          @retval n It still needs to wait to receive n bytes (AT manager continues 
     *                    to receive the remaining data and continues to call back this interface).
     */    
    int (*handler)(at_urc_info_t *info);
} urc_item_t;

/**
 * @brief AT response information
 */
typedef struct {
    struct at_obj  *obj;            /* AT object*/   
    void           *params;         /* User parameters (referenced from ->at_attr_t.params)*/	
    at_resp_code    code;           /* AT command response code.*/    
	unsigned short  recvcnt;        /* Receive data length*/    
    char           *recvbuf;        /* Receive buffer (raw data)*/
    /* Pointer to the receiving content prefix, valid when code=AT_RESP_OK, 
       if no prefix is specified, it pointer to recvbuf
    */
    char           *prefix;      
    /* Pointer to the receiving content suffix, valid when code=AT_RESP_OK, 
       if no suffix is specified, it pointer to recvbuf
    */
    char           *suffix;
} at_response_t;

/**
 * @brief  The configuration for transparent transmission mode.
 */
typedef struct {
    /**
     * @brief       Exit command(Example:AT+TRANS=0). When this command is matched through the 
     *              read interface, the on_exit event is generated.
     */    
    const char *exit_cmd;
    /**
     * @brief       Exit event, triggered when the exit command is currently matched. At this time, 
     *              you can invoke 'at_raw_transport_exit' to exit the transparent transport mode.
     */ 
    void         (*on_exit)(void);    
    /**
     * @brief       Writting interface for transparent data transmission
     * @param       buf   Data buffer
     * @param       len    data length
     * @return      Indicates the length of the written data
     */
    unsigned int (*write)(const void *buf, unsigned int len);
    /**
     * @brief       Reading interface for transparent data transmission
     * @param       buf   Data buffer
     * @param       len    data length
     * @return      The length of the data actually read
     */    
    unsigned int (*read) (void *buf, unsigned int len);
} at_raw_trans_conf_t;

/**
 * @brief AT interface adapter
 */
typedef struct  {
    //Lock, used in OS environment, fill in NULL if not required.
    void (*lock)(void);
    //Unlock, used in OS environment, fill in NULL if not required.
    void (*unlock)(void);
    /**
     * @brief       Data write operation (non-blocking)
     * @param       buf   Data buffer
     * @param       len    data length
     * @return      Indicates the length of the written data
     */    
    unsigned int (*write)(const void *buf, unsigned int len); 
    /**
     * @brief       Data read operation (non-blocking)
     * @param       buf   Data buffer
     * @param       len    data length
     * @return      The length of the data actually read
     */        
    unsigned int (*read)(void *buf, unsigned int len);       
    /**
     * @brief       AT error event ( if not required, fill in NULL)
     */    
    void (*error)(at_response_t *);
    /**
     * @brief       Log output interface, which can print the complete AT interaction 
     *              process, fill in NULL if not required.
     */      
    void (*debug)(const char *fmt, ...);  
#if AT_URC_WARCH_EN
    //URC buffer size, set according to the actual maximum URC frame when used.
	unsigned short urc_bufsize;
#endif    
    //Command response receiving buffer size, set according to the actual maximum command response length
    unsigned short recv_bufsize;
} at_adapter_t;

/**
 * @brief Public environment for AT work 
 */
typedef struct at_env {
    struct at_obj *obj;
    //Public variables (add as needed), these values are reset every time a new work starts.
    int i, j, state;     
    //User parameters (referenced from ->at_attr_t.params)
    void        *params;
    //Set the next polling wait interval (only takes effect once)
    void        (*next_wait)(struct at_env *self, unsigned int ms);
    //Reset timer
    void        (*reset_timer)(struct at_env *self);               
    //Timeout indication
    bool        (*is_timeout)(struct at_env *self, unsigned int ms);
    //Formatted printout with newlines
    void        (*println)(struct at_env *self, const char *fmt, ...);
    //Find a keyword from the received content      
    char *      (*contains)(struct at_env *self, const char *str); 
    //Get receives buffer       
    char *      (*recvbuf)(struct at_env *self);                  
    //Get receives buffer length
    unsigned int(*recvlen)(struct at_env *self);                  
    //Clear the receives buffer 
    void        (*recvclr)(struct at_env *self);
    //Indicates whether the current work has been abort
    bool        (*disposing)(struct at_env *self);
    //End the work and set the response code
    void        (*finish)(struct at_env *self, at_resp_code code);
} at_env_t;

/**
 *@brief AT execution callback
 *@param r   AT response information (including execution results, content information returned by the device)
 */
typedef void (*at_callback_t)(at_response_t *r);            

/**
 *@brief   AT work polling handler
 *@param   env  The public operating environment for AT work, including some common 
 *              variables and relevant interfaces needed to communicate AT commands.
 *@return  Work processing status, which determines whether to continue running the 
 *         work on the next polling cycle.
 * 
 *     @retval true  Indicates that the current work processing has been finished, 
 *                   and the work response code is set to AT_RESP_OK
 *     @retval false Indicates unfinished work processing, keep running.
 *@note   It must be noted that if the env->finish() operation is invoked in the 
 *        current work, the work will be forcibly terminated regardless of the return value.
 */
typedef int  (*at_work_t)(at_env_t *env);

#if AT_WORK_CONTEXT_EN

/**
 *@brief AT work item context (used to monitor the entire life cycle of AT work item)
 */
typedef struct {
    at_work_state   work_state;   /* Indicates the state at which the AT work item is running. */
    at_resp_code    code;         /* Indicates the response code after the AT command has been run.*/    
    unsigned short  bufsize;      /* Indicates receive buffer size*/
    unsigned short  resplen;      /* Indicates the actual response valid data length*/
    unsigned char   *respbuf;     /* Point to the receive buffer*/
} at_context_t;

#endif

/**
 *@brief AT attributes
 */
typedef struct {
#if AT_WORK_CONTEXT_EN
    at_context_t  *ctx;          /* Pointer to the Work context. */
#endif    
    void          *params;       /* User parameter, fill in NULL if not required. */
    const char    *prefix;       /* Response prefix, fill in NULL if not required. */ 
    const char    *suffix;       /* Response suffix, fill in NULL if not required. */
    at_callback_t  cb;           /* Response callback handler, Fill in NULL if not required. */
    unsigned short timeout;      /* Response timeout(ms).. */
    unsigned char  retry;        /* Response error retries. */
    at_cmd_priority priority;    /* Command execution priority. */
} at_attr_t;

/**
 *@brief AT object.
 */
typedef struct at_obj {
    const at_adapter_t *adap;    
#if AT_RAW_TRANSPARENT_EN    
    const at_raw_trans_conf_t *raw_conf;  
#endif    
    void  *user_data;                                                 
} at_obj_t;

at_obj_t *at_obj_create(const at_adapter_t *);

void at_obj_destroy(at_obj_t *at);

bool at_obj_busy(at_obj_t *at);

void at_obj_set_user_data(at_obj_t *at, void *user_data);

void *at_obj_get_user_data(at_obj_t *at);
#if AT_URC_WARCH_EN
void at_obj_set_urc(at_obj_t *at, const urc_item_t *tbl, int count);

int at_obj_get_urcbuf_count(at_obj_t *at);

void at_obj_urc_set_enable(at_obj_t *at, int enable, unsigned short timeout);

#endif

void at_obj_process(at_obj_t *at);

void at_attr_deinit(at_attr_t *attr);

bool at_exec_cmd(at_obj_t *at, const at_attr_t *attr, const char *cmd, ...);

bool at_exec_vcmd(at_obj_t *at, const at_attr_t *attr, const char *cmd, va_list va);

bool at_send_singlline(at_obj_t *at, const at_attr_t *attr, const char *singlline);

bool at_send_multiline(at_obj_t *at, const at_attr_t *attr, const char **multiline);

bool at_send_data(at_obj_t *at, const at_attr_t *attr, const void *databuf, unsigned int bufsize);

bool at_custom_cmd(at_obj_t *at, const at_attr_t *attr, void (*sender)(at_env_t *env));

bool at_do_work(at_obj_t *at,  void *params, at_work_t work);

void at_work_abort_all(at_obj_t *at);

#if AT_MEM_WATCH_EN
unsigned int at_max_used_memory(void);

unsigned int at_cur_used_memory(void);
#endif

#if AT_WORK_CONTEXT_EN

void at_context_init(at_context_t *ctx, void *respbuf, unsigned bufsize);

void at_context_attach(at_attr_t *attr, at_context_t *ctx);

at_work_state at_work_get_state(at_context_t *ctx);

bool at_work_is_finish(at_context_t *ctx);

at_resp_code  at_work_get_result(at_context_t *ctx);

#endif //End of AT_WORK_CONTEXT_EN

#if AT_RAW_TRANSPARENT_EN
void at_raw_transport_enter(at_obj_t *obj, const at_raw_trans_conf_t *conf);

void at_raw_transport_exit(at_obj_t *obj);
#endif //End of AT_RAW_TRANSPARENT_EN

#endif //End of _AT_CHAT_H_

