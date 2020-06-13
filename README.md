# AT Command

#### 介绍
一种AT命令通信管理模块(支持单行发)，支持裸机和OS版本。适用于modem、WIFI模块、蓝牙通信。

#### 软件架构
软件架构说明
at_chat.c at_chat.h用于无OS版本，使用链式队列及异步回调方式处理AT命令收发，支持URC处理。
at_core.c at_core.h用于OS版本
#### 使用说明

##### at_chat 模块(无OS)

1.  定义AT管理器
    at_core_t at;  
	
2.  AT管理器配置参数
const char at_core_conf_t conf = { 	
};

3.  初始化AT管理器
at_core_init(&at, conf);

4.  将AT管理器放入任务中轮询
at_poll_task(&at);

5.  发送单行命令

at_send_singlline(&at, NULL, "AT+CSQ?");


#### 码云特技

1.  使用 Readme\_XXX.md 来支持不同的语言，例如 Readme\_en.md, Readme\_zh.md
2.  码云官方博客 [blog.gitee.com](https://blog.gitee.com)
3.  你可以 [https://gitee.com/explore](https://gitee.com/explore) 这个地址来了解码云上的优秀开源项目
4.  [GVP](https://gitee.com/gvp) 全称是码云最有价值开源项目，是码云综合评定出的优秀开源项目
5.  码云官方提供的使用手册 [https://gitee.com/help](https://gitee.com/help)
6.  码云封面人物是一档用来展示码云会员风采的栏目 [https://gitee.com/gitee-stars/](https://gitee.com/gitee-stars/)
