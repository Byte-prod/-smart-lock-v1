#ifndef __LED_H
#define __LED_H

void Tim3_Init(u16 arr,u16 psc);
void GPIO_Toggle_INIT(void);
void led_task();

#endif