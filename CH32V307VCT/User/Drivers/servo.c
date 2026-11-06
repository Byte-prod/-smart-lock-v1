/**
 * servo.c — 舵机驱动 (门锁执行器)
 *
 * 硬件: TIM2_CH1 → PA0 (PWM)
 * 时序: 20ms周期, 0.5~2.5ms脉宽 → 0°~180°
 *       0° = 关锁, 90° = 开锁
 */

#include "driver_all.h"

/*==========================================================================
 *  静态变量
 *========================================================================*/
static uint32_t servo_timer = 0;
static uint8_t  servo_current_state = 0;    // 0=0°, 1=90°
u8 servo_flag = 0;                          // 舵机标志: 1=开门, 0=关门

/*==========================================================================
 *  TIM2 PWM 初始化
 *  时钟: 96MHz / 96 = 1MHz (1us分辨率)
 *  周期: 20000 → 20ms (50Hz标准舵机频率)
 *========================================================================*/
void TIM2_PWM_Init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    TIM_TimeBaseStructure.TIM_Period        = PWM_PERIOD - 1;
    TIM_TimeBaseStructure.TIM_Prescaler     = 96 - 1;       // 1us
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = PWM_MIN;          // 初始=关锁
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;

    TIM_OC1Init(TIM2, &TIM_OCInitStructure);
    TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM2, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
}

/*==========================================================================
 *  GPIO 初始化 (PA0 → TIM2_CH1 PWM输出)
 *========================================================================*/
void GPIO_Init_PWM(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/*==========================================================================
 *  设置舵机角度 (0~180°)
 *  线性映射: angle → 500 + (angle * 2000) / 180 us
 *========================================================================*/
void servo_set_angle(uint16_t angle)
{
    uint16_t compare;

    if (angle > 180) angle = 180;

    compare = PWM_MIN + (angle * (PWM_MAX - PWM_MIN)) / 180;
    TIM_SetCompare1(TIM2, compare);

    servo_current_state = (angle >= 45) ? 1 : 0;
    servo_timer = uwTick;
}

/*==========================================================================
 *  舵机开关控制
 *  state = 0 → 关闭 (0°)   关锁
 *  state = 1 → 打开 (90°)  开锁
 *========================================================================*/
void servo(uint8_t state)
{
    if (state > 1) state = 0;

    if (state == 1)
        TIM_SetCompare1(TIM2, PWM_90);      // 90° = 开锁
    else
        TIM_SetCompare1(TIM2, PWM_MIN);     // 0°  = 关锁
}

/*==========================================================================
 *  舵机调度任务 — 根据 servo_flag 执行开关 + 5秒自动上锁
 *========================================================================*/
void servo_task(void)
{
    static uint32_t servo_open_time = 0;
    static uint8_t  servo_auto_lock_active = 0;

    servo(servo_flag);

    // 5秒自动上锁
    if (servo_flag == 1)
    {
        if (!servo_auto_lock_active)
        {
            servo_auto_lock_active = 1;
            servo_open_time = uwTick;
        }
        else if ((uwTick - servo_open_time) >= 5000)
        {
            servo_flag = 0;
            servo_auto_lock_active = 0;
            esp8266_report_close();  // 上锁后上报关门事件
        }
    }
    else
    {
        servo_auto_lock_active = 0;
    }
}
