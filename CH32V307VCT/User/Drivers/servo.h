#ifndef __SERVO_H
#define __SERVO_H
//PA0  PWM

// 定义舵机的PWM周期和占空比
#define PWM_PERIOD 20000  // PWM周期为20ms (20000us)
#define PWM_MIN 500       // 最小脉宽为0.5ms (500us)
#define PWM_MAX 2500      // 最大脉宽为2.5ms (2500us)

// 90度对应的脉宽定义
#define PWM_90  1500      // 90度对应脉宽1.5ms


void TIM2_PWM_Init(void);
void GPIO_Init_PWM(void);
void servo(uint8_t state);
void servo_set_angle(uint16_t angle);
void servo_task();

#endif