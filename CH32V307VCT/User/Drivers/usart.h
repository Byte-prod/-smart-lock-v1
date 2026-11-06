#ifndef __USART_H
#define __USART_H
/*
串口3→天问51语音模块 (ASRPRO-01)
PB10  TX  → 天问51 RX
PB11  RX  → 天问51 TX
1启动成功
2门已上锁，请刷卡或输入密码解锁
3密码正确，门已打开，欢迎回家
4密码错误，请重新输入
5连续三次输入密码错误，已锁定，请等待30秒后重新输入
6修改开锁密码，请输入管理员密码
7管理员密码验证成功，请输入新密码
8新密码设置成功
9管理员密码错误，请重新输入
10连续三次输入管理员密码错误，已锁定，请等待30秒后重新输入
11刷卡成功，门已开启，欢迎回家
12刷卡失败，请重试
13录入卡片，请输入管理员密码
14管理员密码验证成功，请将卡片平放在传感器上
15卡片添加成功
16指纹验证成功，门已开启，欢迎回家
17指纹验证失败，请重试
18录入指纹，请输入管理员密码
19管理员密码验证成功，请将手指平放在传感器上
20指纹正在录入
21指纹录入成功
22网络已连接
23网络已断开
24清空成功
*/
void Usart3_Init(void);
void USART3_SendByte(u8 byte);

//串口2刷卡模块
// PA3  RX
// PA2  TX
// 卡号去重防重复播报
void Usart2_Init(void);

//串口7指纹模块
//PC3   RX
//PC2   TX
void Uart7_Init();
void uart7_send_string(u8* string,u8 len);//串口7发送数组
void usart1_send_srting(u8* string,u8 len);//串口1发送数组


//串口6ESP8266
//PC0(TX) ESP8266-RX
//PC1(RX) ESP8266-TX
void Uart6_Init();
void usart6_send_srting(u8* string,u8 len);//串口6发送数组
#endif