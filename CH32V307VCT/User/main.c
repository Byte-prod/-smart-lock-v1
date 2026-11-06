/********************************** (C) COPYRIGHT *******************************
* File Name          : main.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2021/06/06
* Description        : Main program body.
*******************************************************************************/

/*
 *@Note
 * 智能门锁主程序
 * 硬件: CH32V307VCT
 * 外设: USART3→天问51(语音) USART2(RFID) UART7(指纹) UART6(ESP8266 WiFi)
 *       TIM2(舵机PWM) TIM3(系统时基) 4x4键盘 LCD
 */

#include "driver_all.h"

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);     // 中断优先级分组
    SystemCoreClockUpdate();                             // 时钟更新
    Delay_Init();                                        // 延时初始化
    USART_Printf_Init(115200);                          // 串口1打印调试

    GPIO_Toggle_INIT();      // LED指示灯
    Usart3_Init();           // 串口3 = 语音模块
    Usart2_Init();           // 串口2 = RFID刷卡模块
    Uart7_Init();            // 串口7 = 指纹模块
    Uart6_Init();            // 串口6 = ESP8266 WiFi模块
    as608_init();            // 指纹模块GPIO初始化
    Tim3_Init(1000, 96-1);  // 系统时基1ms
    TIM2_PWM_Init();         // 舵机PWM
    GPIO_Init_PWM();         // 舵机GPIO
    key_init();              // 4x4矩阵键盘
    LCD_Init();              // LCD屏幕
    scheduler_init();        // 任务调度器初始化

    // 启动动画
    USART3_SendByte('1');    // 语音: 开机成功
    LCD_Fill(0, 0, 127, 127, WHITE);
    LCD_ShowPicture(0, 0, 120, 120, gImage_3);
    Delay_Ms(1000);


    LCD_Fill(0, 0, 127, 127, WHITE);
    LCD_ShowPicture(0, 0, 128, 128, gImage_2);
    show_flag = 0;//密码验证主页
    USART3_SendByte('2');
    LCD_Fill(16, 45, 112, 66, GBLUE);


    while(1)
    {
        scheduler_run();    // 运行任务调度器
    }
}
