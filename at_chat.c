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
 
#include "at_chat.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

//超时判断
#define AT_IS_TIMEOUT(start, time) (at_get_ms() - (start) > (time))

/*ATCOMM work type -----------------------------------------------------------*/
#define AT_TYPE_WORK       0                             /*工作 --------------*/
#define AT_TYPE_CMD        1                             /*标准命令 ----------*/  
#define AT_TYPE_MULTILINE  3                             /*多行命令 ----------*/
#define AT_TYPE_SINGLLINE  4                             /*单行命令 ----------*/

typedef int (*base_work)(at_obj_t *at, ...);

static void at_send_line(at_obj_t *at, const char *fmt, va_list args);

static const inline at_core_conf_t *__get_adapter(at_obj_t *at) 
{
    return &at->cfg;
}

static bool is_timeout(at_obj_t *at, unsigned int ms)
{
    return AT_IS_TIMEOUT(at->resp_timer, ms);
}
/*
 * @brief   发送数据
 */
static void send_data(at_obj_t *at, const void *buf, unsigned int len)
{
    at->cfg.write(buf, len);
}

/*
 * @brief       格式化打印
 */
static void print(at_obj_t *at, const char *cmd, ...)
{
    va_list args;	
    va_start(args, cmd);
    at_send_line(at, cmd, args);
    va_end(args);	
}
/*
 * @brief   获取当前数据接收长度
 */
static unsigned int get_recv_count(at_obj_t *at)
{
    return at->rcv_cnt;
}

/*
 * @brief   获取数据缓冲区
 */
static char *get_recv_buf(at_obj_t *at)
{
    return (char *)at->cfg.rcv_buf;
}

/*
 * @brief   清除数据缓冲区
 */
static void recv_buf_clear(at_obj_t *at)
{
    at->rcv_cnt = 0;
}

/*前向查找字串*/
static char *search_string(at_obj_t *at, const char *str)
{
    return strstr(get_recv_buf(at), str);
}

/*前向查找字串*/
static bool at_isabort(at_obj_t *at)
{
	return at->cursor ? at->cursor->abort : 1;
}


/*
 * @brief  AT执行回调
 */
static void do_at_callbatk(at_obj_t *a, at_item_t *i, at_callbatk_t cb, at_return ret)
{
    at_response_t r;
    if (cb) {
        r.param   = i->param;
        r.recvbuf = get_recv_buf(a);
        r.recvcnt = get_recv_count(a);
        r.ret     = ret;       
        cb(&r);
    }
}

/*
 * @brief       AT配置
 * @param[in]   cfg   - AT响应
 */
void at_core_init(at_obj_t *at, const at_core_conf_t cfg)
{
    at_env_t *e;
    at->cfg  = cfg;
    e = &at->env;    
    at->rcv_cnt = 0;
    
    e->is_timeout = is_timeout;
    e->printf  = print;
    e->recvbuf = get_recv_buf;
    e->recvclr = recv_buf_clear;
    e->recvlen = get_recv_count;
    e->find    = search_string;
    e->abort   = at_isabort;
}
/*添加作业到队列*/
static bool add_work(at_obj_t *at, void *params, void *info, int type)
{
    at_item_t *i;
    if (list_empty(&at->ls_idle))                       //无空闲at_item
        return NULL;
    i = list_first_entry(&at->ls_idle, at_item_t, node);//从空闲链中取出作业
    i->info  = (void *)info;
    i->param = (void *)params;
    i->state = AT_STATE_WAIT;
    i->type  = type;
    i->abort = 0;
    list_move_tail(&i->node, &at->ls_ready);            //移入就绪链
    return i != 0;    
}

/*
 * @brief  执行任务
 */
static int do_work_handler(at_obj_t *at)
{
    at_item_t *i = at->cursor;
    return ((int (*)(at_env_t *e))i->info)(i->param);
}

/*******************************************************************************
 * @brief       通用命令执行
 * @param[in]   a - AT管理器
 * @return      0 - 保持工作,非0 - 结束工作
 ******************************************************************************/
static int do_cmd_handler(at_obj_t *a)
{
    at_item_t *i = a->cursor;
    at_env_t  *e = &a->env;
    const at_respond_t *c = (at_respond_t *)i->info;
    switch(e->state) {
    case 0:  /*发送状态 ------------------------------------------------------*/                              
        c->sender(e);
        e->state++;
        e->reset_timer(a);
        e->recvclr(a);
    break;
    case 1: /*接收状态 ------------------------------------------------------*/ 
        if (search_string(a, c->matcher)) {                      	
            do_at_callbatk(a, i, c->cb, AT_RET_OK);
            return true;
        } else if (search_string(a, "ERROR")) {    
            if (++e->i >= c->retry) {
                do_at_callbatk(a, i, c->cb, AT_RET_ERROR);
                return true;
            }
            e->state = 2;                             /*出错之后延时一段时间*/                
            e->reset_timer(a);                        /*重置定时器*/
        } else if (e->is_timeout(a, c->timeout))  {   
            if (++e->i >= c->retry) {
                do_at_callbatk(a, i, c->cb, AT_RET_TIMEOUT);
                return true;
            }                
            e->state = 0;                             /*返回上一状态*/
        }
    break; 
    case 2:
        if (e->is_timeout(a, 500))
            e->state = 0;                             /*返回初始状态*/    
    break;
    default: 
        e->state = 0;
    }
    return false;
}

/*******************************************************************************
 * @brief       单行命令
 * @param[in]   a - AT管理器
 * @return      0 - 保持工作,非0 - 结束工作
 ******************************************************************************/
static int send_signlline_handler(at_obj_t *a)
{            
    at_item_t *i = a->cursor;
    at_env_t  *e = &a->env;    
    const char *cmd  = (const char *)i->param;
    at_callbatk_t cb = (at_callbatk_t)i->info;
    
    switch(e->state) {
    case 0:  /*发送状态 ------------------------------------------------------*/                              
        e->printf(a, cmd);
        e->state++;
        e->reset_timer(a);
        e->recvclr(a);
    break;
    case 1: /*接收状态 ------------------------------------------------------*/ 
        if (search_string(a, "OK")) {                      	
            do_at_callbatk(a, i, cb, AT_RET_OK);
            return true;
        } else if (search_string(a, "ERROR")) {
            if (++e->i >= 3) {
                do_at_callbatk(a, i, cb, AT_RET_ERROR);
                return true;
            }
            e->state = 2;                             /*出错之后延时一段时间*/                
            e->reset_timer(a);                        /*重置定时器*/
        } else if (e->is_timeout(a, 3000 + e->i * 2000))  {   
            if (++e->i >= 3) {
                do_at_callbatk(a, i, cb, AT_RET_TIMEOUT);
                return true;
            }                
            e->state = 0;                             /*返回上一状态*/
        }            
    break; 
    case 2:
        if (e->is_timeout(a, 500))
            e->state = 0;                             /*返回初始状态*/    
    break;
    default: 
        e->state = 0;
    }
    return false;
}
/*******************************************************************************
 * @brief       多行命令管理
 * @param[in]   a - AT管理器
 * @return      0 - 保持工作,非0 - 结束工作
 ******************************************************************************/
static int send_multiline_handler(at_obj_t *a)
{            
    at_item_t *i = a->cursor;
    at_env_t  *e = &a->env;        
    const char **cmds = (const char **)i->param;
    at_callbatk_t cb  = (at_callbatk_t)i->info;
    switch(e->state) {
    case 0:
        if (cmds[e->i] == NULL) {                    /*命令执行完毕*/
            do_at_callbatk(a, i, cb, AT_RET_OK);
            return true;
        }
        e->printf(a, "%s\r\n", cmds[e->i]);
        e->recvclr(a);                               /*清除接收*/
        e->reset_timer(a);
        e->state++;
    break;
    case 1:
        if (search_string(a, "OK")){         
            e->state = 0;
            e->i++;
            e->i     = 0;
        } else if (search_string(a, "ERROR")) {
            if (++e->j >= 3) {
                do_at_callbatk(a, i, cb, AT_RET_ERROR);
                return true;
            }
            e->state = 2;                             /*出错之后延时一段时间*/                
            e->reset_timer(a);                        /*重置定时器*/            
        } else if (e->is_timeout(a, 3000)) {
            do_at_callbatk(a, i, cb, AT_RET_TIMEOUT);
            return true;
        }       
    break;
    default: 
        e->state = 0;    
    }
    return 0;
}

/*
 * @brief       发送行
 * @param[in]   fmt    - 格式化输出
 * @param[in]   args   - 如变参数列表
 */
static void at_send_line(at_obj_t *at, const char *fmt, va_list args)
{
    char buf[MAX_AT_CMD_LEN];
    int len;
    const at_core_conf_t *adt = __get_adapter(at);
    len = vsnprintf(buf, sizeof(buf), fmt, args);

    recv_buf_clear(at);     //清空接收缓存
    send_data(at, buf, len);
    send_data(at, "\r\n", 2);
}
/*
 * @brief       urc 处理总入口
 * @param[in]   urc
 * @return      none
 */
static void urc_handler_entry(at_obj_t *at, char *urc, unsigned int size)
{
    int i, n;
    utc_item_t *tbl = at->cfg.utc_tbl;
    for (i = 0; i < at->cfg.urc_tbl_count; i++){
    n = strlen(tbl->prefix);
    if (strncmp(urc, tbl->prefix, n) == 0)
        tbl[i].handler(urc, size);
    }
}

/*
 * @brief       urc 接收处理
 * @param[in]   buf  - 数据缓冲区
 * @return      none
 */
static void urc_recv_process(at_obj_t *at, char *buf, unsigned int size)
{
    char *urc_buf;	
    unsigned short urc_size;
    urc_buf  = (char *)at->cfg.urc_buf;
    urc_size = at->cfg.urc_bufsize;	
    if (size == 0 && at->urc_cnt > 0) {
        if (AT_IS_TIMEOUT(at->urc_timer, 2000)){
            urc_handler_entry(at, urc_buf, at->urc_cnt);
            at->rcv_cnt = 0;
        }
    } else {
        at->urc_timer = at_get_ms();
        while (size--) {
            if (*buf == '\n') {
                urc_buf[at->urc_cnt] = '\0';
                urc_handler_entry(at, urc_buf, at->urc_cnt);
            } else {
            urc_buf[at->urc_cnt++] = *buf++;
            if (at->urc_cnt >= urc_size)
              at->urc_cnt = 0;
            }
        }
    }
}

/*
 * @brief       指令响应接收处理
 * @param[in]   buf  - 
 * @return      none
 */
static void resp_recv_process(at_obj_t *at, const char *buf, unsigned int size)
{
    char *rcv_buf;
    unsigned short rcv_size;	
    
    rcv_buf  = (char *)at->cfg.rcv_buf;
    rcv_size = at->cfg.rcv_bufsize;

    if (at->rcv_cnt + size >= rcv_size)         //接收溢出
        at->rcv_cnt = 0;
    
    memcpy(rcv_buf + rcv_cnt, buf, size);
    at->rcv_cnt += size;
    rcv_buf[at->rcv_cnt] = '\0';

}

/*
 * @brief       执行AT作业
 * @param[in]   a      - AT管理器
 * @param[in]   work   - AT作业入口
 * @param[in]   params - 
 */
bool at_do_work(at_obj_t *at, int (*work)(at_env_t *e), void *params)
{
    return add_work(at, params, (void *)work, AT_TYPE_WORK);
}

/*
 * @brief       执行AT指令
 * @param[in]   a - AT管理器
 * @param[in]   cmd   - cmd命令
 */
bool at_do_cmd(at_obj_t *at, void *params, const at_respond_t *cmd)
{
    return add_work(at, params, (void *)cmd, AT_TYPE_CMD);
}

/*
 * @brief       发送单行AT命令
 * @param[in]   at          - AT管理器
 * @param[in]   cb          - 执行回调
 * @param[in]   singlline   - 单行命令
 * @note        在命令执行完毕之前,singlline必须始终有效
 */
bool at_send_singlline(at_obj_t *at, at_callbatk_t cb, const char *singlline)
{
    return add_work(at, (void *)singlline, (void *)cb, AT_TYPE_SINGLLINE);
}

/*
 * @brief       发送多行AT命令
 * @param[in]   at          - AT管理器
 * @param[in]   cb          - 执行回调
 * @param[in]   multiline   - 单行命令
 * @note        在命令执行完毕之前,multiline
 */
bool at_send_multiline(at_obj_t *at, at_callbatk_t cb, const char **multiline)
{
    return add_work(at, multiline, (void *)cb, AT_TYPE_MULTILINE);    
}

/*
 * @brief       强行中止AT作业
 */

void at_item_abort(at_item_t *i)
{
	i->abort = 1;
}

/*
 * @brief       AT忙判断
 * @return      true - 有AT指令或者任务正在执行中
 */
bool at_core_busy(at_obj_t *at)
{
    return !list_empty(&at->ls_ready);
}

/*******************************************************************************
 * @brief   AT作业管理
 ******************************************************************************/
static void at_work_manager(at_obj_t *at)
{     
    register at_item_t *cursor = at->cursor;
    at_env_t           *e      = &at->env;
    /*通用工作处理者 ---------------------------------------------------------*/
    static int (*const work_handler_table[])(at_obj_t *) = {
    	do_work_handler, 
        do_cmd_handler,
        send_signlline_handler,
        send_multiline_handler
    };       
    if (at->cursor == NULL) {    
        if (list_empty(&at->ls_ready))                   //就绪链为空
            return;
        e->i     = 0; 
        e->j     = 0;
        e->state = 0;
        e->params = cursor->param;        
        e->recvclr(at);
        e->reset_timer(at);
        at->cursor = list_first_entry(&at->ls_ready, at_item_t, node);
    }
    /*工作执行完成,则将它放入到空闲工作链 ------------------------------------*/
    if (work_handler_table[cursor->type](at) || cursor->abort) {
    	list_move_tail(&at->cursor->node, &at->ls_idle);
		at->cursor = NULL;
    }
        
}
/*
 * @brief  AT轮询任务
 */
void at_poll_task(at_obj_t *at)
{
    char rbuf[32];
    int read_size;
    read_size = __get_adapter(at)->read(rbuf, sizeof(rbuf));
    urc_recv_process(at, rbuf, read_size);
    resp_recv_process(at, rbuf, read_size);    
    at_work_manager(at);
}


