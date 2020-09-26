/******************************************************************************
 * @brief        AT命令通信管理(OS版本)
 *
 * Copyright (c) 2020, <master_roger@sina.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs: 
 * Date           Author       Notes 
 * 2020-01-02     Morro        Initial version. 
 ******************************************************************************/

#include "at.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

//超时判断
#define AT_IS_TIMEOUT(start, time) (at_get_ms() - (start) > (time))

static LIST_HEAD(atlist);                        /*链表头结点 ----------------*/

/*
 * @brief    输出字符串
 */
static void put_string(at_obj_t *at, const char *s)
{
    while (*s != '\0')
        at->cfg.write(s++, 1);
}


/*
 * @brief    输出字符串(带换行)
 */
static void put_line(at_obj_t *at, const char *s)
{
    put_string(at, s);
    put_string(at, "\r\n");    
    at->cfg.debug("->\r\n%s\r\n", s);
}

//打印输出
static void at_print(at_obj_t *at, const char *cmd, ...)
{
    va_list args;	
    va_start(args, cmd);
    char buf[MAX_AT_CMD_LEN];
    vsnprintf(buf, sizeof(buf), cmd, args);
    put_line(at, buf);
    va_end(args);	
}

/*
 * @brief   清除数据缓冲区
 */
static void recvbuf_clr(at_obj_t *at)
{
    at->rcv_cnt = 0;
}

//等待AT命令响应
static at_return wait_resp(at_obj_t *at, at_respond_t *r)
{    
    at->resp  = r;
    at->ret   = AT_RET_TIMEOUT;
    at->resp_timer = at_get_ms();    
    recvbuf_clr(at);                    //清空接收缓存
    at->wait  = 1;
    at_sem_wait(&at->completed, r->timeout);        
    at->cfg.debug("<-\r\n%s\r\n", r->recvbuf);
    at->resp = NULL;
    at->wait = 0;
    return at->ret;
}

/*
 * @brief       同步响应AT响应
 * @param[in]   resp    - 等待接收串(如"OK",">")
 * @param[in]   timeout - 等待超时时间
 */
at_return wait_resp_sync(struct at_obj *at, const char *resp, 
                         unsigned int timeout)
{
    char buf[64];
    int cnt = 0, len;
    at_return ret = AT_RET_TIMEOUT;
    unsigned int timer = at_get_ms();
    while (at_get_ms() - timer < timeout) {
        len = at->cfg.read(buf, sizeof(buf) - cnt);
        cnt += len;
        buf[cnt] = '\0';
        if (strstr(buf, resp)) {
            ret =  AT_RET_OK;
            break;
        } else if (strstr(buf, "ERROR")) {
            ret =  AT_RET_ERROR;
            break;
        }
        at_delay(10);
    }
    at->cfg.debug("%s", buf);
    return ret;
}


/*
 * @brief       AT内核配置
 */
void at_obj_create(at_obj_t *at, const at_conf_t cfg)
{
    at_work_env_t *e;
    at->cfg  = cfg;
    at->rcv_cnt = 0;
    
    at_sem_init(&at->cmd_lock, 1);
    at_sem_init(&at->completed, 0);
    e          = &at->env;
    e->at      = at;
    e->printf  = at_print;
    e->recvclr = recvbuf_clr;
    e->read    = cfg.read;
    e->write   = cfg.write;
    e->wait_resp = wait_resp_sync;
    
    list_add_tail(&at->node, &atlist);
    
}

/*
 * @brief 销毁AT
 */
void at_obj_destroy(at_obj_t *at)
{
    list_del(&at->node);
}

/*
 * @brief       执行命令
 * @param[in]   fmt    - 格式化输出
 * @param[in]   r      - 响应参数,如果填NULL, 默认返回OK表示成功,等待3s
 * @param[in]   args   - 如变参数列表
 */
at_return at_do_cmd(at_obj_t *at, at_respond_t *r, const char *cmd)
{
    at_return ret;
    char      defbuf[64];
    at_respond_t  default_resp = {"OK", defbuf, sizeof(defbuf), 3000};
    if (r == NULL) {
        r = &default_resp;                 //默认响应      
    }
    if (!at_sem_wait(&at->cmd_lock, r->timeout)) {
        return AT_RET_TIMEOUT;    
    }
    while (at->urc_cnt) {
        at_delay(10);
    }
    put_line(at, cmd);
    ret = wait_resp(at, r); 
    at_sem_post(&at->cmd_lock);
    return ret;    
}

/*
 * @brief       执行AT作业
 * @param[in]   urc
 * @return      none
 */
int at_do_work(at_obj_t *at, at_work work, void *params)
{
    int ret;
    if (!at_sem_wait(&at->cmd_lock, 150 * 1000)) {
        return AT_RET_TIMEOUT;    
    }    
    at->env.params = params;
    at->dowork = true;
    ret = work(&at->env);
    at->dowork = false;
    at_sem_post(&at->cmd_lock);
    return ret;
}

/*
 * @brief       分割响应行
 * @param[in]   recvbuf - 接收缓冲区 
 * @param[out]  lines   - 响应行数组
 * @return      行数
 */
int at_split_respond_lines(char *recvbuf, char *lines[], int count)
{
    char *s = recvbuf;
    size_t i = 0;      
    if (s == NULL || lines == NULL) 
        return 0;     
        
    lines[i++] = s;    
    while(*s && i < count) {       
        if (*s == ',') {
            *s = '\0';                                       
            lines[i++] = s + 1;                           /*指向下一个子串*/
        }
        s++;        
    }    
    return i;    
}

/*
 * @brief       urc 处理总入口
 * @param[in]   urcline - URC行
 * @return      none
 */
static void urc_handler_entry(at_obj_t *at, char *urcline, unsigned int size)
{
    int i, n;
    utc_item_t *tbl = at->cfg.utc_tbl;
    
    for (i = 0; i < at->cfg.urc_tbl_count; i++){
        n = strlen(tbl->prefix);
        if (n > 0 && strncmp(urcline, tbl->prefix, n) == 0) {            
            tbl->handler(urcline, size);
            at->cfg.debug("<=\r\n%s\r\n", urcline);
            return;
        }    
        tbl++;
    }
    
    if (size >= 2 && !at->wait)              //自动输出
        at->cfg.debug("%s\r\n", urcline);          
}

/*
 * @brief       urc 接收处理
 * @param[in]   buf  - 接收缓存
 * @return      none
 */
static void urc_recv_process(at_obj_t *at, const char *buf, unsigned int size)
{
    char *urc_buf;	
    unsigned short urc_size;
    unsigned char  c;
    urc_buf  = (char *)at->cfg.urc_buf;
    urc_size = at->cfg.urc_bufsize;
    if (at->urc_cnt > 0 && size == 0) {
        if (AT_IS_TIMEOUT(at->urc_timer, 100)) {               //100ms超时
            urc_buf[at->urc_cnt] = '\0';
            at->urc_cnt = 0;
            at->cfg.debug("urc recv timeout=>%s\r\n", urc_buf);       
        }
    } else {
        at->urc_timer = at_get_ms();
        while (size--) {
            c = *buf++;
            if (c == '\r' || c == '\n') {                       //收到1行
                urc_buf[at->urc_cnt] = '\0';
                if (at->urc_cnt > 2)
                    urc_handler_entry(at, urc_buf, at->urc_cnt);
                at->urc_cnt = 0;
            } else {
                urc_buf[at->urc_cnt++] = c;
                if (at->urc_cnt >= urc_size)                    //溢出处理
                    at->urc_cnt = 0;
            }
        }
    }
}

/*
 * @brief       指令响应接收处理
 * @param[in]   buf  - 接收缓冲区
 * @param[in]   size -  缓冲区数据长度
 * @return      none
 */
static void resp_recv_process(at_obj_t *at, const char *buf, unsigned int size)
{
    char *rcv_buf;
    unsigned short rcv_size;	
    at_respond_t *resp = at->resp;
    
    if (resp == NULL || size  == 0)
        return;

    rcv_buf  = (char *)resp->recvbuf;
    rcv_size = resp->bufsize;

    if (at->rcv_cnt + size >= rcv_size) {             //接收溢出
        at->rcv_cnt = 0;
        at->cfg.debug("Receive overflow:%s", rcv_buf);
    }
    /*将接收到的数据放至rcv_buf中 ---------------------------------------------*/
    memcpy(rcv_buf + at->rcv_cnt, buf, size);
    at->rcv_cnt += size;
    rcv_buf[at->rcv_cnt] = '\0';
    

    if (!at->wait)
        return;    
    if (strstr(rcv_buf, resp->matcher)) {            //接收匹配
        at->ret = AT_RET_OK;
    } else if (strstr(rcv_buf, "ERROR")) {
        at->ret = AT_RET_ERROR;
    } else if (AT_IS_TIMEOUT(at->resp_timer, resp->timeout)) {
        at->ret = AT_RET_TIMEOUT;		
    } else if (at->suspend)                         //强制终止
        at->ret = AT_RET_ABORT;
    else
        return;
    
    at_sem_post(&at->completed);
}

/*
 * @brief       AT忙判断
 * @return      true - 有AT指令或者任务正在执行中
 */
bool at_obj_busy(at_obj_t *at)
{
    return at->wait == 0 && AT_IS_TIMEOUT(at->urc_timer, 2000);
}

/*
 * @brief       挂起AT作业
 * @return      none
 */
void at_suspend(at_obj_t *at)
{
    at->suspend = 1;
}

/*
 * @brief       恢复AT作业
 * @return      none
 */
void at_resume(at_obj_t *at)
{
    at->suspend = 0;
}

/*
 * @brief       AT轮询线程
 * @return      none
 */
void at_thread(void)
{
    at_obj_t *at;
    struct list_head *list ,*n = NULL;     
    int len;
    char buf[32];
    while (1) {
        /*遍历所有at_obj结点*/
        list_for_each_safe(list, n, &atlist) {
            at = list_entry(list, at_obj_t, node);
            if (!at->dowork) {
#warning "读取优化(readline) ..."                
                len = at->cfg.read(buf, sizeof(buf));
                urc_recv_process(at, (char *)buf, len);
                if (len > 0) {
                    resp_recv_process(at, buf, len);
                }
            }        
        }
        at_delay(1);        
    }
}
