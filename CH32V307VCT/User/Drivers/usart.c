/**
 * usart.c — CH32V307 串口驱动集合
 *
 * USART3  PB10/PB11  语音模块   115200
 * USART2  PA2/PA3    RFID刷卡   115200
 * UART7   PC2/PC3    指纹模块   115200
 * UART6   PC0/PC1    ESP8266    115200
 */

#include "driver_all.h"

void USART3_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void UART7_IRQHandler(void)  __attribute__((interrupt("WCH-Interrupt-fast")));
void UART6_IRQHandler(void)  __attribute__((interrupt("WCH-Interrupt-fast")));

u8 rfid_index;
u8 rfid_temp[4] = {0};
u8 rfid[4][4]   = {0};
u8 rfid_password_index;
u8  rfid_last_id[4] = {0xFF, 0xFF, 0xFF, 0xFF};  // 上一次处理的卡号(去重用)
u32 rfid_last_tick = 0;                            // 上一次处理的时间戳

u8 uart7_rec_string[20] = {0};
u8 uart7_rec_index;
u8 uart7_rec_tick;

/* ================================================================
 *  USART3 — 语音模块
 * ================================================================ */

void USART3_SendByte(u8 byte)
{
    while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
    USART_SendData(USART3, byte);
    while(USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
}

void Usart3_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,  ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate            = 115200;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART3, &USART_InitStructure);

    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel    = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART3, ENABLE);
    USART_ClearFlag(USART3, USART_FLAG_TC);
}

void USART3_IRQHandler(void)
{
    if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
    {
        // 预留: 语音模块回传
    }
    USART_ClearITPendingBit(USART3, USART_IT_RXNE);
}

/* ================================================================
 *  USART2 — RFID 刷卡模块
 * ================================================================ */

void Usart2_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,  ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate            = 115200;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART2, &USART_InitStructure);

    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel    = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART2, ENABLE);
    USART_ClearFlag(USART2, USART_FLAG_TC);
}

/*
 * RFID 11字节帧: 04 0C 02 30 00 04 00 | ID[0..3]
 * key_mode=0 验证刷卡, key_mode=4 添加卡片
 *
 * 防重复播报: 同一张卡2秒内不重复处理 (卡号去重)
 */
void USART2_IRQHandler(void)
{
    u8 temp;
    u8 i;

    if (password_error == 3) return;

    /* ── RXNE: 逐字节接收 ────────────────────────── */
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        temp = USART_ReceiveData(USART2);

        switch (rfid_index)
        {
            case 0:  
            if (temp == 0x04) rfid_index++; break;
            case 1:  if (temp == 0x0C) rfid_index++; else rfid_index = 0; break;
            case 2:  if (temp == 0x02) rfid_index++; else rfid_index = 0; break;
            case 3:  if (temp == 0x30) rfid_index++; else rfid_index = 0; break;
            case 4:  if (temp == 0x00) rfid_index++; else rfid_index = 0; break;
            case 5:  if (temp == 0x04) rfid_index++; else rfid_index = 0; break;
            case 6:  if (temp == 0x00) rfid_index++; else rfid_index = 0; break;

            case 7:  rfid_temp[0] = temp; rfid_index++; break;
            case 8:  rfid_temp[1] = temp; rfid_index++; break;
            case 9:  rfid_temp[2] = temp; rfid_index++; break;
            case 10: rfid_temp[3] = temp; rfid_index = 0;

                /* ── 去重检查: 同一张卡2秒内跳过 ── */
                if (uwTick - rfid_last_tick < 2000)
                {
                    u8 same = 1;
                    for (i = 0; i < 4; i++)
                    {
                        if (rfid_temp[i] != rfid_last_id[i])
                        {
                            same = 0;
                            break;
                        }
                    }
                    if (same) 
                    break;  // 重复卡片, 跳过
                }

                /* ── 记录本次刷卡信息 ── */
                rfid_last_tick = uwTick;
                for (i = 0; i < 4; i++)
                    rfid_last_id[i] = rfid_temp[i];

                if (key_mode == 0)
                {
                    if (rfid_chek())
                    {
                        USART3_SendByte('1');
                        USART3_SendByte('1');
                        esp_open_method = OPEN_METHOD_RFID;
                        servo_flag = 1;
                        show_flag  = 8;
                        esp8266_report_open(OPEN_METHOD_RFID, 0xFF);
                    }
                    else
                    {
                        show_flag = 0;
                        USART3_SendByte('1');
                        USART3_SendByte('2');
                        esp8266_report_rfid_fail();
                    }
                }
                else if (key_mode == 4)
                {
                    if (rfid_password_index < 4)
                    {
                        string_copy(rfid_temp, rfid[rfid_password_index], 4);
                        rfid_password_index++;
                        USART3_SendByte('1');
                        USART3_SendByte('5');
                        show_flag = 6;
                    }
                }
                break;
        }
        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }

}

/* ================================================================
 *  UART7 — 指纹模块 (AS608)
 * ================================================================ */

void Uart7_Init()
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART7, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC,  ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate            = 115200;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(UART7, &USART_InitStructure);

    USART_Cmd(UART7, ENABLE);
    USART_ClearFlag(UART7, USART_FLAG_TC);
    USART_ITConfig(UART7, USART_IT_RXNE, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel    = UART7_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void usart1_send_srting(u8* string, u8 len)
{
    u8 i;
    for(i = 0; i < len; i++)
    {
        USART_SendData(USART1, string[i]);
        while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
    }
}

void uart7_send_string(u8* string, u8 len)
{
    u8 i;
    for(i = 0; i < len; i++)
    {
        USART_SendData(UART7, string[i]);
        while(USART_GetFlagStatus(UART7, USART_FLAG_TC) == RESET);
    }
    usart1_send_srting(string, len);
}

void UART7_IRQHandler(void)
{
    u8 temp = 0;

    if(USART_GetITStatus(UART7, USART_IT_RXNE) != RESET)
    {
        ps_wait_flag = 0;

        if (uart7_rec_tick > 10)
        {
            uart7_rec_index = 0;
        }

        temp = USART_ReceiveData(UART7);

        if(uart7_rec_index < sizeof(uart7_rec_string))
        {
            uart7_rec_string[uart7_rec_index] = temp;
            uart7_rec_index++;
        }

        uart7_rec_tick = 0;
    }

    if(USART_GetITStatus(UART7, USART_IT_ORE) != RESET)
    {
        USART_ReceiveData(UART7);
    }
}

/* ================================================================
 *  UART6 — ESP8266 WiFi 模块
 * ================================================================ */

void Uart6_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART6, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC,  ENABLE);

    // PC0 = TX (AF_PP)
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // PC1 = RX (IN_FLOATING) 
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate            = 115200;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(UART6, &USART_InitStructure);

    USART_Cmd(UART6, ENABLE);
    USART_ClearFlag(UART6, USART_FLAG_TC);
    USART_ITConfig(UART6, USART_IT_RXNE, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel    = UART6_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void usart6_send_srting(u8* string, u8 len)
{
    u8 i;
    for(i = 0; i < len; i++)
    {
        USART_SendData(UART6, string[i]);
        while(USART_GetFlagStatus(UART6, USART_FLAG_TC) == RESET);
    }
}

void UART6_IRQHandler(void)
{
    u8 temp = 0;

    if(USART_GetITStatus(UART6, USART_IT_RXNE) != RESET)
    {
        temp = USART_ReceiveData(UART6);
        esp8266_rx_byte(temp);
    }

    if(USART_GetITStatus(UART6, USART_IT_ORE) != RESET)
    {
        USART_ReceiveData(UART6);
    }

    USART_ClearITPendingBit(UART6, USART_IT_RXNE);
}
