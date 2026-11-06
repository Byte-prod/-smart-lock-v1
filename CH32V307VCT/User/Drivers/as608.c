/**
 * as608.c — AS608 光学指纹模块驱动 (非阻塞状态机版本)
 *
 * 硬件: UART7 — PC2(TX) PC3(RX) @ 115200
 *       触摸传感器 — PA1 (浮空输入, 手指触摸→高电平)
 *
 * 协议: 命令帧格式 (12~17字节)
 *   包头(1B) + 地址(4B) + 包标识(1B) + 包长度(2B) + 指令(1B) + 参数(N B) + 校验(2B)
 *
 * 功能流程:
 *   key_mode=6 (管理员录入指纹):
 *     触摸→拍照→生成特征1→拍照→生成特征2→合并模板→存储
 *   key_mode=0 (验证指纹开门):
 *     触摸→拍照→生成特征→搜索→匹配/不匹配
 *
 * 非阻塞设计:
 *   状态机由 25ms 定时器调度，每个状态只做一件事，等待由串口接收完成标志驱动。
 *   超时保护: AS608_TIMEOUT (约 1 秒)
 */

#include "driver_all.h"
#include <string.h>   // 如果有需要使用 strlen，没有的话可删

/*==========================================================================
 *  全局变量
 *========================================================================*/
u8  ps_wait_flag;             // 保留(原忙等标志，状态机不再使用)
u8  as608_task_flag;          // 触摸传感器当前电平
u8  as608_task_flag_old;      // 触摸传感器上一次电平（用于上升沿检测）
u8  as608_store_index = 1;    // 指纹存储地址(1~N)，录入后自动递增

/* ── 非阻塞状态机变量 ── */
static u8  as608_state     = 0;   // 当前状态 (见状态定义)
static u8  as608_store_tmp = 0;   // 临时保存本次录入的存储地址
static u16 as608_timer     = 0;   // 超时计数器，每 25ms 调用 as608_task() 时加 1

volatile u8 as608_rx_done  = 0;   // 一帧接收完成标志 (1ms 定时中断中置位)

/* 状态定义 */
#define AS608_IDLE               0   // 空闲
#define AS608_GETIMAGE1          1   // 录入第1次拍照
#define AS608_WAIT_IMAGE1        2   // 等待第1次拍照应答
#define AS608_GENCHAR1           3   // 生成特征1
#define AS608_WAIT_GENCHAR1      4   // 等待特征1生成应答
#define AS608_GETIMAGE2          5   // 录入第2次拍照
#define AS608_WAIT_IMAGE2        6   // 等待第2次拍照应答
#define AS608_GENCHAR2           7   // 生成特征2
#define AS608_WAIT_GENCHAR2      8   // 等待特征2生成应答
#define AS608_REGMODEL           9   // 合并模板
#define AS608_WAIT_REGMODEL     10   // 等待合并模板应答
#define AS608_STORE             11   // 存储模板
#define AS608_WAIT_STORE        12   // 等待存储应答
#define AS608_VERIFY_GETIMAGE   13   // 验证模式拍照
#define AS608_VERIFY_WAIT_IMAGE 14   // 等待验证拍照应答
#define AS608_VERIFY_GENCHAR    15   // 验证模式生成特征
#define AS608_VERIFY_WAIT_CHAR  16   // 等待验证特征生成应答
#define AS608_VERIFY_SEARCH     17   // 搜索匹配
#define AS608_VERIFY_WAIT_RESULT 18  // 等待搜索结果

#define AS608_TIMEOUT           40   // 超时时间 40 * 25ms = 1000ms

/*==========================================================================
 *  AS608 基础指令 (发送到模块，同时通过 usart1 打印原始数据)
 *========================================================================*/

/** 从传感器读取指纹图像到图像缓冲区 */
void ps_getimage()
{
    u8 string[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, 0x01, 0x00, 0x05};
    uart7_send_string(string, 12);   // 该函数内部会通过 usart1 打印
}

/** 将图像缓冲区内容生成特征文件
 *  @param buffer  特征缓冲区号(1或2)
 */
void ps_genchar(u8 buffer)
{
    u8 string[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x04, 0x02, buffer, 0x00, 0x01+0x00+0x04+0x02+buffer};
    uart7_send_string(string, 13);
}

/** 合并特征文件(CharBuffer1 + CharBuffer2 → 模板) */
void ps_regmodel()
{
    u8 string[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, 0x05, 0x00, 0x09};
    uart7_send_string(string, 12);
}

/** 存储模板到指纹库
 *  @param addr  存储地址(索引)
 */
void ps_storeChar(u8 addr)
{
    u8 string[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x06, 0x06, 0x02, 0x00, addr, 0x00, 0x01+0x00+0x06+0x06+0x02+0x00+addr};
    uart7_send_string(string, 15);
}

/** 搜索指纹(在指纹库中匹配当前模板)
 *  匹配结果: uart7_rec_string[13] > 50 表示匹配成功
 */
void ps_search()
{
    u8 string[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x08, 0x04, 0x02, 0x00, 0x00, 0x00, 0xFF, 0x01, 0x0E};
    uart7_send_string(string, 17);
}

/** 清空指纹库 */
void PS_Empty()
{
    u8 string[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, 0x0D, 0x00, 0x11};
    uart7_send_string(string, 12);
}

/*==========================================================================
 *  触摸传感器 GPIO 初始化
 *  PA1 — 浮空输入, 手指触摸→拉高
 *========================================================================*/
void as608_init()
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/*==========================================================================
 *  调试输出到 USART1 (可读文本)
 *========================================================================*/
void usart1_debug(const char *str)
{
    while (*str)
    {
        USART_SendData(USART1, (u8)*str);
        while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
        str++;
    }
}

/*==========================================================================
 *  as608_task() — 触摸检测 + 指纹状态机（非阻塞，25ms 调用一次）
 *
 *  功能:
 *    1. 空闲时检测触摸上升沿，启动录入或验证流程。
 *    2. 如果状态机正在运行，则推进到下一状态（发送指令/等待应答/处理结果）。
 *    3. 超时保护: 若某个状态超过 AS608_TIMEOUT * 25ms 没有完成，自动回到空闲。
 *    4. 结束后同步触摸引脚电平，防止手指一直按着误触发。
 *========================================================================*/
void as608_task()
{
    /* ———— 如果状态机正在运行，只推进状态机，不检测触摸 ———— */
    if (as608_state != AS608_IDLE)
    {
        /* 超时处理: 任何状态超过设定时间自动回到空闲 */
        if (as608_timer > AS608_TIMEOUT)
        {
            
            as608_state = AS608_IDLE;
            as608_task_flag_old = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1); // 防止误触发
            return;
        }
        as608_timer++;   // 每 25ms 递增一次

        switch (as608_state)
        {
            /* ==================== 录入流程 ==================== */
            case AS608_GETIMAGE1:   // 第1次拍照
                
                ps_getimage();
                as608_state = AS608_WAIT_IMAGE1;
                as608_timer = 0;
                uart7_rec_index = 0;    // 清空接收缓冲区，准备接收应答
                as608_rx_done = 0;      // 清除接收完成标志
                break;

            case AS608_WAIT_IMAGE1: // 等待第1次拍照应答
                if (as608_rx_done)
                {
                    
                    as608_state = AS608_GENCHAR1;
                    as608_timer = 0;
                }
                break;

            case AS608_GENCHAR1:    // 生成特征1 (buffer=1)
                
                ps_genchar(1);
                as608_state = AS608_WAIT_GENCHAR1;
                as608_timer = 0;
                uart7_rec_index = 0;
                as608_rx_done = 0;
                break;

            case AS608_WAIT_GENCHAR1:
                if (as608_rx_done)
                {
                    
                    as608_state = AS608_GETIMAGE2;
                    as608_timer = 0;
                }
                break;

            case AS608_GETIMAGE2:   // 第2次拍照
                
                ps_getimage();
                as608_state = AS608_WAIT_IMAGE2;
                as608_timer = 0;
                uart7_rec_index = 0;
                as608_rx_done = 0;
                break;

            case AS608_WAIT_IMAGE2:
                if (as608_rx_done)
                {
                    
                    as608_state = AS608_GENCHAR2;
                    as608_timer = 0;
                }
                break;

            case AS608_GENCHAR2:    // 生成特征2 (buffer=2)
                
                ps_genchar(2);
                as608_state = AS608_WAIT_GENCHAR2;
                as608_timer = 0;
                uart7_rec_index = 0;
                as608_rx_done = 0;
                break;

            case AS608_WAIT_GENCHAR2:
                if (as608_rx_done)
                {
                    
                    as608_state = AS608_REGMODEL;
                    as608_timer = 0;
                }
                break;

            case AS608_REGMODEL:    // 合并模板
                
                ps_regmodel();
                as608_state = AS608_WAIT_REGMODEL;
                as608_timer = 0;
                uart7_rec_index = 0;
                as608_rx_done = 0;
                break;

            case AS608_WAIT_REGMODEL:
                if (as608_rx_done)
                {
                    
                    as608_state = AS608_STORE;
                    as608_timer = 0;
                }
                break;

            case AS608_STORE:       // 存储模板
                
                ps_storeChar(as608_store_tmp);
                as608_state = AS608_WAIT_STORE;
                as608_timer = 0;
                uart7_rec_index = 0;
                as608_rx_done = 0;
                break;

            case AS608_WAIT_STORE:  // 等待存储应答
                if (as608_rx_done)
                {
                    
                    show_flag = 6;
                    USART3_SendByte('2');
                    USART3_SendByte('1');       // 语音: 录入成功
                    key_mode = 0;
                    as608_state = AS608_IDLE;
                    as608_task_flag_old = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1); // 同步触摸
                }
                break;

            /* ==================== 验证流程 ==================== */
            case AS608_VERIFY_GETIMAGE: // 拍照
                
                ps_getimage();
                as608_state = AS608_VERIFY_WAIT_IMAGE;
                as608_timer = 0;
                uart7_rec_index = 0;
                as608_rx_done = 0;
                break;

            case AS608_VERIFY_WAIT_IMAGE:
                if (as608_rx_done)
                {
                    
                    as608_state = AS608_VERIFY_GENCHAR;
                    as608_timer = 0;
                }
                break;

            case AS608_VERIFY_GENCHAR: // 生成特征 (buffer=2，与录入时保持一致)
                
                ps_genchar(2);
                as608_state = AS608_VERIFY_WAIT_CHAR;
                as608_timer = 0;
                uart7_rec_index = 0;
                as608_rx_done = 0;
                break;

            case AS608_VERIFY_WAIT_CHAR:
                if (as608_rx_done)
                {
                    
                    as608_state = AS608_VERIFY_SEARCH;
                    as608_timer = 0;
                }
                break;

            case AS608_VERIFY_SEARCH:   // 搜索匹配
                
                ps_search();
                as608_state = AS608_VERIFY_WAIT_RESULT;
                as608_timer = 0;
                uart7_rec_index = 0;
                as608_rx_done = 0;
                break;

            case AS608_VERIFY_WAIT_RESULT:  // 等待搜索结果
                if (as608_rx_done)
                {
                    if (uart7_rec_string[13] > 50)   // 匹配成功
                    {
                        
                        esp_open_method = OPEN_METHOD_FINGER;
                        servo_flag = 1;
                        show_flag  = 1;
                        USART3_SendByte('1');
                        USART3_SendByte('6');
                        esp8266_report_open(OPEN_METHOD_FINGER, 0xFF);
                    }
                    else                             // 匹配失败
                    {
                        
                        USART3_SendByte('1');
                        USART3_SendByte('7');
                        esp8266_report_finger_fail();
                    }
                    as608_state = AS608_IDLE;
                    as608_task_flag_old = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1); // 同步触摸
                }
                break;
        }
        return;   // 状态机运行时不执行下面的触摸检测
    }

    /* ———— 空闲时检测触摸上升沿 ———— */
    as608_task_flag = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1);
    if (as608_task_flag == as608_task_flag_old)
        return;   // 电平未变化
    as608_task_flag_old = as608_task_flag;
    if (as608_task_flag == 0)
        return;   // 下降沿或低电平，忽略

    /* 触摸触发（上升沿），启动对应流程 */
    if (key_mode == 6)          // 管理员录入模式
    {
        
        as608_store_tmp = as608_store_index;
        as608_store_index++;
        as608_state = AS608_GETIMAGE1;
        as608_timer = 0;
    }
    else if (key_mode == 0)    // 验证开门模式
    {
       
        as608_state = AS608_VERIFY_GETIMAGE;
        as608_timer = 0;
    }
}