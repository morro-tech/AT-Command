/******************************************************************************
 * @brief        AT指令通信
 *
 * Copyright (c) 2020, <master_roger@sina.com>
 *
 * SPDX-License-Identifier: Apathe-2.0
 *
 * Change Logs: 
 * Date           Author       Notes 
 * 2020-01-02     Morro        初版
 ******************************************************************************/

#ifndef _ATCHAT_H_
#define _ATCHAT_H_

#include "at_util.h"
#include <list.h>
#include <stdbool.h>

#define MAX_AT_CMD_LEN          128

struct at_obj;

/*urc处理项 -----------------------------------------------------------------*/
typedef struct {
    const char *prefix;  //需要匹配的头部
    void (*handler)(char *recvbuf, int size); 
}utc_item_t;

typedef struct {
    unsigned int (*write)(const void *buf, unsigned int len);   /*发送接口*/
    unsigned int (*read)(void *buf, unsigned int len);          /*接收接口*/
    /*Events -----------------------------------------------------------------*/
    void         (*before_at)(void);                            /*开始执行AT*/
    void         (*after_at)(void);
    void         (*error)(void);
	utc_item_t    *utc_tbl;                                     /*utc 表*/
	unsigned char *urc_buf;                                     /*urc接收缓冲区*/
    unsigned char *rcv_buf;
	unsigned short urc_tbl_count;
	unsigned short urc_bufsize;                                 /*urc缓冲区大小*/
    unsigned short rcv_bufsize;                                 /*接收缓冲区*/
}at_obj_conf_t;

/*AT作业运行环境*/
typedef struct {
	int         i,j,state;   
	void        *params;
    void        (*reset_timer)(struct at_obj *at);
	bool        (*is_timeout)(struct at_obj *at, unsigned int ms); /*时间跨度判断*/
	void        (*printf)(struct at_obj *at, const char *fmt, ...);
	char *      (*find)(struct at_obj *at, const char *expect);
    char *      (*recvbuf)(struct at_obj *at);                 /*指向接收缓冲区*/
    unsigned int(*recvlen)(struct at_obj *at);                 /*缓冲区总长度*/
    void        (*recvclr)(struct at_obj *at);                 /*清空接收缓冲区*/
    bool        (*abort)(struct at_obj *at);                   /*终止执行*/
}at_env_t;

/*AT命令响应码*/
typedef enum {
    AT_RET_OK = 0,                                             /*执行成功*/
    AT_RET_ERROR,                                              /*执行错误*/
    AT_RET_TIMEOUT,                                            /*响应超时*/
	AT_RET_ABORT,                                              /*强行中止*/
}at_return;

/*AT响应 */
typedef struct {
    void           *param;
	char           *recvbuf;
	unsigned short  recvcnt;
    at_return       ret;
}at_response_t;

typedef void (*at_callbatk_t)(at_response_t *r);

/*AT状态 */
typedef enum {
    AT_STATE_IDLE,                                             /*空闲状态*/
    AT_STATE_WAIT,                                             /*等待执行*/
    AT_STATE_EXEC,                                             /*正在执行*/
}at_work_state;

/*AT作业项*/
typedef struct {
    at_work_state state : 3;
    unsigned char type  : 3;
    unsigned char abort : 1;
    void          *param;
	void          *info;
    struct list_head node;
}at_item_t;

/*AT管理器 ------------------------------------------------------------------*/
typedef struct at_obj{
	at_obj_conf_t          cfg;
    at_env_t                env;
	at_item_t               tbl[10];
    at_item_t               *cursor;
    struct list_head        ls_ready, ls_idle;               /*就绪,空闲作业链*/
	unsigned int            resp_timer;
	unsigned int            urc_timer;
	at_return               ret;
	//urc接收计数, 命令响应接收计数器
	unsigned short          urc_cnt, rcv_cnt;
	unsigned char           suspend: 1;
}at_obj_t;

typedef struct {
    void (*sender)(at_env_t *e);                            /*自定义发送器 */
    const char *matcher;                                    /*接收匹配串 */
    at_callbatk_t  cb;                                      /*响应处理 */
    unsigned char  retry;                                   /*错误重试次数 */
    unsigned short timeout;                                 /*最大超时时间 */
}at_cmd_t;

void at_obj_init(at_obj_t *at, const at_obj_conf_t cfg);

/*发送单行AT命令*/
bool at_send_singlline(at_obj_t *at, at_callbatk_t cb, const char *singlline);
/*发送多行AT命令*/
bool at_send_multiline(at_obj_t *at, at_callbatk_t cb, const char **multiline);
/*执行AT命令*/
bool at_do_cmd(at_obj_t *at, void *params, const at_cmd_t *cmd);
/*自定义AT作业*/
bool at_do_work(at_obj_t *at, int (*work)(at_env_t *e), void *params);

void at_item_abort(at_item_t *it);                          /*终止当前作业*/
         
bool at_obj_busy(at_obj_t *at);                              /*忙判断*/

void at_suspend(at_obj_t *at);

void at_resume(at_obj_t *at);

void at_poll_task(at_obj_t *at);


#endif
