# AT Command

#### 介绍
一种AT命令通信解析模块,支持裸机(at_chat)和OS版本(at)。适用于modem、WIFI模块、蓝牙通信。

#### 软件架构
软件架构说明
at_chat.c at_chat.h用于无OS版本，使用链式队列及异步回调方式处理AT命令收发，支持URC处理。
at_core.c at_core.h用于OS版本
#### 使用说明

##### at_chat 模块(无OS)


```

static at_core_t at;          //定义AT管理器

const at_core_conf_t conf = { //AT管理器配置参数
	
};
```


3.  初始化AT管理器

```
at_core_init(&at, &conf);
```


4.  将AT管理器放入任务中轮询

```
void main(void)
{
    /*do something ...*/
    while (1) {
        /*do something ...*/
        
        at_poll_task(&at);
    }
}

```


5.  发送单行命令


```
static void read_csq_callback(at_response_t *r)
{
	/*...*/
}
at_send_singlline(&at, read_csq_callback, "AT+CSQ");
```

##### at 模块(OS版本)


```

static at_obj_t at;          //定义AT管理器

char urc_buf[128];           //URC主动上报缓冲区

utc_item_t utc_tbl[] = {     //定义URC表
	"+CSQ: ", csq_updated_handler
}

const at_conf_t conf = {     //AT管理器配置参数
	.urc_buf     = urc_buf,
	.urc_bufsize = sizeof(urc_buf),
	.utc_tbl     = utc_tbl,
	.urc_tbl_count = sizeof(utc_tbl) / sizeof(utc_item_t),	
	
	//适配GPRS模块的串口读写接口
	.write       = uart_write,
	.read        = uart_read
};
```

3.  初始化AT管理器并创建AT线程

```
void at_thread(void)
{
	at_obj_create(&at, &conf);
    while (1) {
        /*do something ...*/
        at_thread(&at);
    }
}

```


4.  使用例子
查询GPRS模块信号质量：
	=> AT+CSQ
	
	<= +CSQ: 24, 0
	<= OK
```
/* 
 * @brief    获取csq值
 */ 
bool read_csq_value(at_obj_t *at, int *rssi, int *error_rate)
{
	//接收缓冲区
	unsigned char recvbuf[32];
	//AT响应
	at_respond_t r = {"OK", recvbuf, sizeof(recvbuf), 3000};
	//
	if (at_do_cmd(at, &r, "AT+CSQ") != AT_RET_OK)
		return false;
	//解析响应响应
	return (sscanf(recv, "%*[^+]+CSQ: %d,%d", rssi, error_rate) == 2);

}


```
