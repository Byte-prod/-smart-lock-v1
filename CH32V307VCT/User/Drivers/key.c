/**
 * key.c — 4x4矩阵键盘驱动 + 密码/RFID/指纹管理逻辑
 *
 * GPIO:
 *   OUT PP: PC5 PE7 PE9 PE11  (列扫描)
 *   IPU:    PE13 PE15 PD9 PD11 (行读取)
 *
 * 按键布局:
 *   1   2   3   修改密码
 *   4   5   6   录入指纹
 *   7   8   9   添加卡片
 *   删除 0   清除 确认
 *
 * 系统模式(key_mode):
 *   0 = 主页(密码验证开门)
 *   1 = 验证管理员密码(修改密码前)
 *   2 = 修改密码
 *   3 = 验证管理员密码(添加卡片前)
 *   4 = 等待刷卡添加
 *   5 = 验证管理员密码(录入指纹前)
 *   6 = 等待按指纹录入
 */

#include "driver_all.h"
#include "string.h"

/* ================================================================
 *  键盘 GPIO 初始化
 * ================================================================ */
void key_init()
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    // OUT PP — 列扫描输出
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_7 | GPIO_Pin_9 | GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOE, &GPIO_InitStructure);

    // IPU — 行读取输入
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_13 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOE, &GPIO_InitStructure);

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9 | GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
}

/* ================================================================
 *  按键扫描 (4x4矩阵)
 *  返回: 1~16 (按键值), 0 (无按键)
 *
 *  列输出低电平, 读取行输入:
 *    列1(PC5=0)    → 按键 1, 2, 3, 4
 *    列2(PE7=0)    → 按键 5, 6, 7, 8
 *    列3(PE9=0)    → 按键 9,10,11,12
 *    列4(PE11=0)   → 按键 13,14,15,16
 * ================================================================ */
u8 key_read()
{
    u8 temp = 0;

    // 扫描列1
    GPIO_ResetBits(GPIOC, GPIO_Pin_5);
    GPIO_SetBits(GPIOE, GPIO_Pin_7);
    GPIO_SetBits(GPIOE, GPIO_Pin_9);
    GPIO_SetBits(GPIOE, GPIO_Pin_11);
    if(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_13) == 0) temp = 1;
    if(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_15) == 0) temp = 2;
    if(GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_9)  == 0) temp = 3;
    if(GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_11) == 0) temp = 4;

    // 扫描列2
    GPIO_SetBits(GPIOC, GPIO_Pin_5);
    GPIO_ResetBits(GPIOE, GPIO_Pin_7);
    GPIO_SetBits(GPIOE, GPIO_Pin_9);
    GPIO_SetBits(GPIOE, GPIO_Pin_11);
    if(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_13) == 0) temp = 5;
    if(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_15) == 0) temp = 6;
    if(GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_9)  == 0) temp = 7;
    if(GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_11) == 0) temp = 8;

    // 扫描列3
    GPIO_SetBits(GPIOC, GPIO_Pin_5);
    GPIO_SetBits(GPIOE, GPIO_Pin_7);
    GPIO_ResetBits(GPIOE, GPIO_Pin_9);
    GPIO_SetBits(GPIOE, GPIO_Pin_11);
    if(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_13) == 0) temp = 9;
    if(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_15) == 0) temp = 10;
    if(GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_9)  == 0) temp = 11;
    if(GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_11) == 0) temp = 12;

    // 扫描列4
    GPIO_SetBits(GPIOC, GPIO_Pin_5);
    GPIO_SetBits(GPIOE, GPIO_Pin_7);
    GPIO_SetBits(GPIOE, GPIO_Pin_9);
    GPIO_ResetBits(GPIOE, GPIO_Pin_11);
    if(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_13) == 0) temp = 13;
    if(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_15) == 0) temp = 14;
    if(GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_9)  == 0) temp = 15;
    if(GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_11) == 0) temp = 16;

    return temp;
}

/* ================================================================
 *  全局变量
 * ================================================================ */
u8 key_val, key_down, key_old, key_up;   // 按键状态变量
u8 password [6]     = {6,6,6,6,6,6};     // 开门密码 (默认666666)
u8 password_cmd [6] = {5,5,5,6,6,6};     // 管理员密码 (默认555666)
u8 key_temp [7]     = {10,10,10,10,10,10}; // 按键输入缓存
u8 key_index;                             // 输入计数
u8 key_index_old;                         // 输入计数(旧)
u8 password_error = 0;                    // 密码错误次数 (3次锁定30秒)
u8 key_shuo       = 0;                    // 键盘锁定标志
u8 key_mode       = 0;                    // 系统模式

/* ================================================================
 *  工具函数
 * ================================================================ */

/** 数组比较 — 全部相等返回1, 否则返回0 */
u8 string_check(u8* string1, u8* string2, u8 len)
{
    while(len--)
    {
        if (string1[len] != string2[len])
            return 0;
    }
    return 1;
}

/** 验证刷卡卡号是否在授权列表中 */
u8 rfid_chek()
{
    u8 i;
    for (i = 0; i < rfid_password_index; ++i)
    {
        if (string_check(rfid_temp, rfid[i], 4))
            return 1;
    }
    return 0;
}

/** 数组拷贝 */
void string_copy(u8* string1, u8* string2, u8 len)
{
    u8 i;
    for (i = 0; i < len; ++i)
        string2[i] = string1[i];
}

/** 清空按键输入缓存 */
void key_clear()
{
    memset(key_temp, 10, 6);
    key_index = 0;
}

/* ================================================================
 *  键盘任务 (调度器每10ms调用)
 *
 *  按键映射:
 *    1~3,5~7,9~11,14 — 数字键 (0~9)
 *    4  — 修改密码模式
 *    8  — 录入指纹模式
 *    12 — 添加卡片模式
 *    13 — 清除 (清空指纹库/清空卡库)
 *    15 — 删除/返回
 *    16 — 确认 (验证密码/执行操作)
 * ================================================================ */
void key_task()
{
    key_val  = key_read();
    key_down = key_val & (key_val ^ key_old);   // 按下检测
    key_up   = ~key_val & (key_val ^ key_old);  // 释放检测
    key_old  = key_val;

    if (password_error == 3) return;   // 锁定中, 不响应按键
    if (key_shuo == 0)
    {
        switch (key_down)
        {
            // ── 数字 1~9, 0 ────────────────────────────
            case 1:  key_temp[key_index] = 1; key_index++; break;
            case 2:  key_temp[key_index] = 2; key_index++; break;
            case 3:  key_temp[key_index] = 3; key_index++; break;
            case 5:  key_temp[key_index] = 4; key_index++; break;
            case 6:  key_temp[key_index] = 5; key_index++; break;
            case 7:  key_temp[key_index] = 6; key_index++; break;
            case 9:  key_temp[key_index] = 7; key_index++; break;
            case 10: key_temp[key_index] = 8; key_index++; break;
            case 11: key_temp[key_index] = 9; key_index++; break;
            case 14: key_temp[key_index] = 0; key_index++; break;

            // ── 按键4: 修改密码 ───────────────────────────
            case 4:
                if (key_mode == 0)
                {
                    key_mode  = 1;
                    show_flag = 3;                // LCD: 请输入管理员密码
                    USART3_SendByte('6');
                    key_clear();
                    password_error = 0;
                }
                break;

            // ── 按键8: 录入指纹 ───────────────────────────
            case 8:
                if (key_mode == 0)
                {
                    key_mode  = 5;
                    show_flag = 3;                // LCD: 请输入管理员密码
                    USART3_SendByte('1');
                    USART3_SendByte('8');
                    key_clear();
                    password_error = 0;
                }
                break;

            // ── 按键12: 添加卡片 ──────────────────────────
            case 12:
                if (key_mode == 0)
                {
                    key_mode  = 3;
                    show_flag = 3;                // LCD: 请输入管理员密码
                    USART3_SendByte('1');
                    USART3_SendByte('3');
                    key_clear();
                    password_error = 0;
                }
                break;

            // ── 按键13: 清除 ──────────────────────────────
            case 13:
                key_clear();
                if (key_mode == 6)
                {
                    PS_Empty();                   // 清空指纹库
                    key_mode  = 0;
                    show_flag = 0;
                    USART3_SendByte('2');//播报清空成功
                    USART3_SendByte('4');
                }
                if (key_mode == 4)
                {
                    memset(rfid, 0, sizeof(rfid)); // 清空卡库
                    key_mode  = 0;
                    show_flag = 0;
                    USART3_SendByte('2');
                    USART3_SendByte('4');
                }
                break;

            // ── 按键15: 删除/返回 ─────────────────────────
            case 15:
                if (key_index)                     // 删除最后一位输入
                {
                    key_index--;
                    key_temp[key_index] = 10;
                }
                else if (key_mode > 0)             // 返回主页
                {
                    key_mode  = 0;
                    show_flag = 0;
                    USART3_SendByte('2');
                }
                break;

            // ── 按键16: 确认 ──────────────────────────────
            case 16:
                switch (key_mode)
                {
                    /* ── 主页: 验证开门密码 ────────────── */
                    case 0:
                        if (string_check(key_temp, password, 6) == 1)
                        {
                            key_clear();
                            esp_open_method = OPEN_METHOD_PASSWORD;
                            servo_flag      = 1;
                            show_flag       = 1;  // LCD: 门已打开
                            password_error  = 0;
                            USART3_SendByte('3');
                            esp8266_report_open(OPEN_METHOD_PASSWORD, 0xFF);
                        }
                        else
                        {
                            key_clear();
                            show_flag = 0;         // LCD: 密码错误
                            esp8266_report_pw_error();
                            if (++password_error == 3)
                            {
                                USART3_SendByte('5');
                                show_flag = 2;     // LCD: 已锁定
                                esp8266_report_lock_alert();
                            }
                            else
                            {
                                USART3_SendByte('4');
                            }
                        }
                        break;

                    /* ── 验证管理员密码(修改密码前) ──────── */
                    case 1:
                        if (string_check(key_temp, password_cmd, 6) == 1)
                        {
                            key_clear();
                            key_mode  = 2;         // 进入修改密码模式
                            show_flag = 4;         // LCD: 请输入新密码
                            USART3_SendByte('7');
                        }
                        else
                        {
                            key_clear();
                            show_flag = 5;         // LCD: 管理员密码错误
                            if (++password_error == 3)
                            {
                                USART3_SendByte('1');
                                USART3_SendByte('0');
                                show_flag = 2;
                            }
                            else
                            {
                                USART3_SendByte('9');
                            }
                        }
                        break;

                    /* ── 确认修改密码 ───────────────────── */
                    case 2:
                        password_error = 0;
                        string_copy(key_temp, password, 6);
                        USART3_SendByte('8');
                        show_flag = 6;             // LCD: 设置成功
                        key_clear();
                        break;

                    /* ── 验证管理员密码(添加卡片前) ──────── */
                    case 3:
                        if (string_check(key_temp, password_cmd, 6) == 1)
                        {
                            key_clear();
                            key_mode  = 4;         // 进入等待刷卡模式
                            show_flag = 7;         // LCD: 请刷卡
                            USART3_SendByte('1');
                            USART3_SendByte('4');
                        }
                        else
                        {
                            key_clear();
                            show_flag = 5;
                            if (++password_error == 3)
                            {
                                USART3_SendByte('1');
                                USART3_SendByte('0');
                                show_flag = 2;
                            }
                            else
                            {
                                USART3_SendByte('9');
                            }
                        }
                        break;

                    /* ── 等待刷卡添加(刷卡由RFID中断处理) ── */
                    case 4:
                        password_error = 0;
                        break;

                    /* ── 验证管理员密码(录入指纹前) ──────── */
                    case 5:
                        if (string_check(key_temp, password_cmd, 6) == 1)
                        {
                            key_clear();
                            key_mode  = 6;         // 进入录入指纹模式
                            show_flag = 9;         // LCD: 请放置手指
                            USART3_SendByte('1');
                            USART3_SendByte('9');
                        }
                        else
                        {
                            key_clear();
                            show_flag = 5;
                            if (++password_error == 3)
                            {
                                USART3_SendByte('1');
                                USART3_SendByte('0');
                                show_flag = 2;
                            }
                            else
                            {
                                USART3_SendByte('9');
                            }
                        }
                        break;

                    /* ── 等待按指纹(录入由as608_task处理) ─── */
                    case 6:
                        password_error = 0;
                    break;
                }
                
                break;  // case 16

        }  // switch(key_down)
    }  // if(key_shuo == 0)
}
