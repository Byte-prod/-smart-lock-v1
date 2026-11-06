/**
 * led.c — 系统时基 + LED指示灯 + 自动关门/锁定计时
 *
 * TIM3 提供 1ms 系统时基
 * 中断中处理:
 *   - uwTick 系统时钟
 *   - LED 闪烁
 *   - 开门后 5秒自动关门
 *   - 密码错误 3次锁定 30秒
 */

#include "driver_all.h"

/*==========================================================================
 *  全局变量
 *========================================================================*/
uint32_t uwTick = 0;           // 系统滴答时钟(毫秒), 每1ms递增

u16 time_led_1000ms = 0;       // LED闪烁计数(1000ms周期)
u16 time_5000ms = 0;           // 5秒倒计时(自动关门)
u16 time_30000ms = 30;         // 30秒倒计时(错误锁定)
u16 time_1000ms;               // 1秒计数(锁定倒计时用)
u16 time_3000ms = 0;           // 3秒倒计时(设置成功后自动回首页)
u8  led_flag = 0;              // LED状态标志

/*==========================================================================
 *  TIM3 初始化 (系统时基 1ms)
 *========================================================================*/
void TIM3_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

void Tim3_Init(u16 arr, u16 psc)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    TIM_TimeBaseInitStruct.TIM_ClockDivision = 0;
    TIM_TimeBaseInitStruct.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInitStruct.TIM_Period        = arr;
    TIM_TimeBaseInitStruct.TIM_Prescaler     = psc;

    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStruct);
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel    = TIM3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_Cmd(TIM3, ENABLE);
}

/*==========================================================================
 *  TIM3 中断服务函数 (每1ms触发一次)
 *========================================================================*/
void TIM3_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
    {
        uwTick++;               // 系统滴答递增
        uart7_rec_tick++;       // 指纹模块接收超时计数
        if(uart7_rec_tick > 10 && uart7_rec_index > 0)
        {
            as608_rx_done = 1;       // 超过10ms没收到新字节 → 一帧结束
            uart7_rec_tick = 11;     // 防止重复置位
        }
        /* ── LED 闪烁 (1Hz) ──────────────────────────── */
        if (++time_led_1000ms == 1000)
        {
            time_led_1000ms = 0;
            led_flag = !led_flag;
            if (led_flag)
                GPIO_SetBits(GPIOC, GPIO_Pin_4);
            else
                GPIO_ResetBits(GPIOC, GPIO_Pin_4);
        }

        /* ── 开门后 5秒自动关门 ───────────────────────── */
        if (servo_flag == 1)
        {
            key_shuo = 1;                   // 锁定键盘
            if (++time_5000ms == 5000)
            {
                time_5000ms = 0;
                key_shuo = 0;
                servo_flag = 0;             // 关门
                show_flag  = 0;             // LCD显示首页
                USART3_SendByte('2');       // 语音: 门已关闭
            }
        }

        /* 3秒后自动返回主界面 */
        if (show_flag == 6 || show_flag == 12 || show_flag == 13)
        {
            key_shuo = 1;
            if (++time_3000ms == 3000)
            {
                time_3000ms    = 0;
                key_shuo       = 0;
                password_error = 0;
                show_flag      = 0;
                key_mode       = 0;         // 回到首页模式
                
            }
        }
        else
        {
            time_3000ms = 0;
        }

        /* ── 密码错误 3次 → 锁定 30秒倒计时 ───────────── */
        if (password_error == 3)
        {
            if (++time_1000ms == 1000)
            {
                time_1000ms = 0;
                if (--time_30000ms == 0)
                {
                    time_30000ms   = 30;     // 重置30秒
                    password_error = 0;      // 清除错误计数
                    show_flag      = 0;      // LCD显示首页
                }
            }
        }
    }
    TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
}

/*==========================================================================
 *  LED GPIO 初始化
 *========================================================================*/
void GPIO_Toggle_INIT(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_SetBits(GPIOC, GPIO_Pin_4);    // 默认关闭LED
}

/*==========================================================================
 *  LED 任务 (调度器每1ms调用, 实际逻辑在中断中)
 *========================================================================*/
void led_task()
{
    
}
