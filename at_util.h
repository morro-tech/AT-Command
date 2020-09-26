/******************************************************************************
 * @brief    at模块OS相关移植接口
 *
 * Copyright (c) 2020, <master_roger@sina.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs: 
 * Date           Author       Notes 
 * 2020-01-02     Morro        初版
 ******************************************************************************/

#ifndef _ATUTIL_H_
#define _ATUTIL_H_

#include "os.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct os_semaphore at_sem_t;                                /*信号量*/

/*
 * @brief	   获取当前系统毫秒数
 */
static inline unsigned int at_get_ms(void)
{
    return ril_get_ms();
}
/*
 * @brief	   超时判断
 * @retval     true | false
 */
static inline bool at_istimeout(unsigned int start_time, unsigned int timeout)
{
    return at_get_ms() - start_time > timeout;
}

/*
 * @brief	   毫秒延时
 * @retval     none
 */
static inline void at_delay(uint32_t ms)
{
    os_delay(ms);
}
/*
 * @brief	   初始化信号量
 * @retval     none
 */
static inline void at_sem_init(at_sem_t *s, int value)
{
    os_sem_init(s, value);
}
/*
 * @brief	   获取信号量
 * @retval     none
 */
static inline bool at_sem_wait(at_sem_t *s, uint32_t timeout)
{
    return os_sem_wait(s, timeout);
}

/*
 * @brief	   释放信号量
 * @retval     none
 */  
static inline void at_sem_post(at_sem_t *s)
{
    os_sem_post(s);
}

#endif