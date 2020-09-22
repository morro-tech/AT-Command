/*******************************************************************************
* @file		at.h
* @brief	AT命令通信管理
* 			
* @version	5.0
* @date		2020-05-11
* @author	roger.luo
*
* Change Logs: 
* Date           Author       Notes 
* 2016-01-22     roger.luo   Initial version. 
* 2017-05-21     roger.luo   1.1 加入任务状态管理   
* 2018-02-11     roger.luo   3.0 
* 2020-01-02     roger.luo   4.0 分离os版本
*******************************************************************************/
#ifndef _AT_H_
#define _AT_H_

#include "at_util.h"
#include "list.h"
#include <stdbool.h>

#define MAX_AT_CMD_LEN          64

struct at_obj;                                                  /*AT对象*/

/*urc处理项 -----------------------------------------------------------------*/
typedef struct {
    const char *prefix;                                         //URC前缀
    void (*handler)(char *recvbuf, int size); 
}utc_item_t;
    
/*AT配置项 -------------------------------------------------------------------*/
typedef struct {
    /*数据读写接口 -----------------------------------------------------------*/
    unsigned int (*read)(void *buf, unsigned int len);          
    unsigned int (*write)(const void *buf, unsigned int len);
    void         (*debug)(const char *fmt, ...);
	utc_item_t    *utc_tbl;                                     /*utc 表*/
	char          *urc_buf;                                     /*urc接收缓冲区*/
	unsigned short urc_tbl_count;
	unsigned short urc_bufsize;                                 /*urc缓冲区大小*/
}at_conf_t;

/*AT命令响应码 ---------------------------------------------------------------*/
typedef enum {
    AT_RET_OK = 0,                                              /*执行成功*/
    AT_RET_ERROR,                                               /*执行错误*/
    AT_RET_TIMEOUT,                                             /*响应超时*/
	AT_RET_ABORT,                                               /*未知错误*/
}at_return;

/*AT响应参数 -----------------------------------------------------------------*/
typedef struct {    
    const char    *matcher;                                     /*接收匹配串*/
    char          *recvbuf;                                     /*接收缓冲区*/
    unsigned short bufsize;                                     /*最大接收长度*/
    unsigned int   timeout;                                     /*最大超时时间 */    
}at_respond_t;

/*AT作业 ---------------------------------------------------------------------*/
typedef struct at_work_env{   
    struct at_obj *at;
	void          *params;                                     
    unsigned int (*write)(const void *buf, unsigned int len);
    unsigned int (*read)(void *buf, unsigned int len);
    
	void         (*printf)(struct at_obj *at, const char *frm, ...);
	at_return    (*wait_resp)(struct at_obj *at, const char *resp, unsigned int timeout);
    void         (*recvclr)(struct at_obj *at);                /*清空接收缓冲区*/
}at_work_env_t;

/*AT对象 ---------------------------------------------------------------------*/
typedef struct at_obj {
    struct list_head        node;
	at_conf_t               cfg;   
    at_work_env_t           env;
	at_sem_t                cmd_lock;                           /*命令锁*/
	at_sem_t                completed;                          /*命令处理完成*/
    at_respond_t            *resp;
	unsigned int            resp_timer;
	unsigned int            urc_timer;
	at_return               ret;
	//urc接收计数, 命令响应接收计数器
	unsigned short          urc_cnt, rcv_cnt;
	unsigned char           wait   : 1;
	unsigned char           suspend: 1;
    unsigned char           dowork : 1;
}at_obj_t;

typedef int (*at_work)(at_work_env_t *);

void at_obj_create(at_obj_t *at, const at_conf_t cfg);         /*AT初始化*/

void at_obj_destroy(at_obj_t *at);

bool at_obj_busy(at_obj_t *at);

void at_suspend(at_obj_t *at);                                 /*挂起*/
 
void at_resume(at_obj_t *at);                                  /*恢复*/

at_return at_do_cmd(at_obj_t *at, at_respond_t *r, const char *cmd);

int at_split_respond_lines(char *recvbuf, char *lines[], int count);

int at_do_work(at_obj_t *at, at_work work, void *params);      /*执行AT作业*/

void at_thread(void);                                          /*AT线程*/
        
#endif
