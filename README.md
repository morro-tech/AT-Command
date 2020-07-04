# AT Command

#### 介绍
一种AT命令通信管理模块(支持单行发)，支持裸机和OS版本。适用于modem、WIFI模块、蓝牙通信。

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


