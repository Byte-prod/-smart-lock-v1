#ifndef __DRIVER_ALL_H
#define __DRIVER_ALL_H

#include "debug.h"
#include "string.h"
#include "scheduler.h"
#include "led.h"
#include "servo.h"
#include "usart.h"
#include "as608.h"
#include "lcd.h"
#include "key.h"
#include "esp8266.h"

extern const unsigned char gImage_1[32520];
extern const unsigned char gImage_2[32768];
extern const unsigned char gImage_3[28808];
extern u16 time_led_1000ms;
extern u8 led_flag;
extern uint32_t uwTick;

extern u8 password [6];
extern u8 password_cmd [6];
extern u8 key_temp [7];
extern u8 key_index;
extern u8 key_index_old;
extern u8 password_error;
extern u8 key_shuo;
extern u8 key_mode;
extern u16 time_30000ms;

extern u8 servo_flag;

extern u8 show_flag;
extern u8 show_flag_old;

extern u8 rfid_index;
extern u8 rfid_temp[4];
extern u8 rfid[4][4];
extern u8 rfid_password_index;
extern u8  rfid_last_id[4];     // 去重: 上一次处理的卡号
extern u32 rfid_last_tick;       // 去重: 上一次处理的时间戳

extern u8 uart7_rec_string[20];
extern u8 uart7_rec_index;
extern u8 uart7_rec_tick;

extern u8 ps_wait_flag;
extern u8 as608_store_index;    // 指纹存储数量 (1~N, 实际指纹数=store_index-1)
extern volatile u8 as608_rx_done;

#endif
