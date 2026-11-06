/********************************** (C) COPYRIGHT *******************************
* File Name          : esp8266.c
* Description        : ESP8266 WiFi模块驱动 — CH32 ↔ ESP8266 串口通信协议
* 硬件连接           : UART6 — PC0(TX) ESP8266-RX, PC1(RX) ESP8266-TX
* 通信参数           : 115200-8-N-1, 全双工异步
* 帧格式             : AA 55 | CMD | LEN | DATA... | CSUM
*******************************************************************************/

#include "driver_all.h"

/*==========================================================================
 *  全局变量定义
 *========================================================================*/
u8  esp_rx_buffer[ESP_RX_BUF_SIZE];   // 接收数据缓冲区
u8  esp_rx_cmd;                       // 当前帧命令字
u8  esp_rx_data_len;                  // 当前帧数据长度
u8  esp_rx_index;                     // 数据接收索引
u8  esp_rx_complete = 0;              // 完整帧就绪标志(任务中处理)
u8  esp_wifi_connected = 0;           // WiFi连接状态 (ESP就绪后置1)
u16 esp_heartbeat_timer = 0;          // 心跳倒计时
u8  esp_open_method = 0;              // 本次开门方式(由验证模块设置)

/*==========================================================================
 *  延迟发送队列 (ISR安全 — 只设标志, task中实际发送)
 *========================================================================*/
static u8  esp_pending_open        = 0;    // 有待发送的开门事件
static u8  esp_pending_open_method = 0;    // 开门方式
static u8  esp_pending_close       = 0;    // 有待发送的关门事件
static u8  esp_pending_pw_err      = 0;    // 密码错误
static u8  esp_pending_lock_alert  = 0;    // 锁定告警
static u8  esp_pending_rfid_fail   = 0;    // 刷卡失败
static u8  esp_pending_finger_fail = 0;    // 指纹失败

/*==========================================================================
 *  内部静态变量
 *========================================================================*/
static esp_rx_state_t esp_rx_state = ESP_RX_IDLE;  // 帧接收状态机
static u32 esp_last_heartbeat = 0;                  // 上次心跳时间戳
static u32 esp_rx_last_byte = 0;                    // 最后收到字节的时间(超时用)
static u8  esp_last_door = 0;                       // 上次门状态(变化检测用)
static u8  esp_first_run  = 1;                      // 首次运行标志

/*==========================================================================
 *  内部辅助函数
 *========================================================================*/

/*********************************************************************
 * @fn      esp_calc_checksum
 * @brief   计算帧校验和 (命令字+数据长度+各数据字节 的低8位)
 * @param   cmd  命令字
 * @param   len  数据长度
 * @param   data 数据指针
 * @return  校验和 (1字节)
 */
static u8 esp_calc_checksum(u8 cmd, u8 len, u8 *data)
{
    u16 sum = cmd + len;
    u8 i;
    for (i = 0; i < len; i++) {
        sum += data[i];
    }
    return (u8)(sum & 0xFF);
}

/*********************************************************************
 * @fn      esp8266_rx_byte
 * @brief   帧接收状态机 — 逐字节喂入,由UART6 ISR调用
 * @param   byte 从UART6刚收到的1字节
 * @note    状态机自动完成帧同步/命令/长度/数据/校验
 *          完整帧到达后设置 esp_rx_complete = 1
 */
void esp8266_rx_byte(u8 byte)
{
    esp_rx_last_byte = uwTick;  // 记录最后收字节时间

    switch (esp_rx_state) {

        case ESP_RX_IDLE:
            if (byte == ESP_FRAME_HEAD1) {
                esp_rx_state = ESP_RX_WAIT_HEAD2;
            }
            break;

        case ESP_RX_WAIT_HEAD2:
            if (byte == ESP_FRAME_HEAD2) {
                esp_rx_state = ESP_RX_WAIT_CMD;
            } else if (byte != ESP_FRAME_HEAD1) {
                // 既非0x55也非0xAA — 重新搜索帧头
                esp_rx_state = ESP_RX_IDLE;
            }
            // byte==0xAA 保持当前状态（可能是连续的0xAA）
            break;

        case ESP_RX_WAIT_CMD:
            esp_rx_cmd  = byte;
            esp_rx_state = ESP_RX_WAIT_LEN;
            break;

        case ESP_RX_WAIT_LEN:
            esp_rx_data_len = byte;
            if (esp_rx_data_len > ESP_RX_BUF_SIZE) {
                // 超长数据 → 帧异常,放弃
                esp_rx_state = ESP_RX_IDLE;
            } else if (esp_rx_data_len == 0) {
                // 无数据段 → 直接校验
                esp_rx_state = ESP_RX_WAIT_CSUM;
            } else {
                esp_rx_index = 0;
                esp_rx_state = ESP_RX_WAIT_DATA;
            }
            break;

        case ESP_RX_WAIT_DATA:
            esp_rx_buffer[esp_rx_index] = byte;
            esp_rx_index++;
            if (esp_rx_index >= esp_rx_data_len) {
                esp_rx_state = ESP_RX_WAIT_CSUM;
            }
            break;

        case ESP_RX_WAIT_CSUM:
            // 校验和验证
            if (byte == esp_calc_checksum(esp_rx_cmd, esp_rx_data_len, esp_rx_buffer)) {
                esp_rx_complete = 1;   // ✓ 帧完整且正确
            }
            // 无论校验是否通过, 都回到IDLE准备下一帧
            esp_rx_state = ESP_RX_IDLE;
            break;

        default:
            esp_rx_state = ESP_RX_IDLE;
            break;
    }
}

/*********************************************************************
 * @fn      esp8266_send_frame
 * @brief   组装并发送一帧数据到ESP8266
 * @param   cmd  命令字
 * @param   data 数据指针(可为NULL当len=0)
 * @param   len  数据长度(0~255)
 * @note    帧格式: AA 55 | CMD | LEN | DATA[0..len-1] | CSUM
 */
void esp8266_send_frame(u8 cmd, u8 *data, u8 len)
{
    u8 csum = esp_calc_checksum(cmd, len, data);
    u8 i;

    USART_SendData(UART6, ESP_FRAME_HEAD1);
    while (USART_GetFlagStatus(UART6, USART_FLAG_TC) == RESET);
    USART_SendData(UART6, ESP_FRAME_HEAD2);
    while (USART_GetFlagStatus(UART6, USART_FLAG_TC) == RESET);

    USART_SendData(UART6, cmd);
    while (USART_GetFlagStatus(UART6, USART_FLAG_TC) == RESET);

    USART_SendData(UART6, len);
    while (USART_GetFlagStatus(UART6, USART_FLAG_TC) == RESET);

    for (i = 0; i < len; i++) {
        USART_SendData(UART6, data[i]);
        while (USART_GetFlagStatus(UART6, USART_FLAG_TC) == RESET);
    }

    USART_SendData(UART6, csum);
    while (USART_GetFlagStatus(UART6, USART_FLAG_TC) == RESET);
}

/*==========================================================================
 *  事件上报函数 (ISR安全 - 只设标志位, 实际发送在esp8266_task中)
 *========================================================================*/

/*********************************************************************
 * @fn      esp8266_report_open
 * @brief   标记开门事件待发送 (ISR安全)
 */
void esp8266_report_open(u8 method, u8 user_index)
{
    esp_pending_open = 1;
    esp_pending_open_method = method;
}

/*********************************************************************
 * @fn      esp8266_report_close
 * @brief   标记关门事件待发送 (ISR安全)
 */
void esp8266_report_close(void)
{
    esp_pending_close = 1;
}

/*********************************************************************
 * @fn      esp8266_report_pw_error
 * @brief   标记密码错误待发送 (ISR安全)
 */
void esp8266_report_pw_error(void)
{
    esp_pending_pw_err = 1;
}

/*********************************************************************
 * @fn      esp8266_report_lock_alert
 * @brief   标记锁定告警待发送 (ISR安全)
 */
void esp8266_report_lock_alert(void)
{
    esp_pending_lock_alert = 1;
}

/*********************************************************************
 * @fn      esp8266_report_device_info
 * @brief   上报设备信息(固件版本+密码+用户数) — 仅在task中调用
 * @note    v1.1 扩展: 包含开门密码/管理员密码/指纹数/卡数
 *          帧格式: AA 55 07 10 FW_MAJ FW_MIN PW[6] ADM_PW[6] CARDS FINGERS CSUM
 */
void esp8266_report_device_info(void)
{
    u8 data[16];
    u8 i;
    data[0] = FW_VER_MAJOR;
    data[1] = FW_VER_MINOR;
    // 开门密码 (6B)
    for (i = 0; i < 6; i++) data[2 + i] = password[i];
    // 管理员密码 (6B)
    for (i = 0; i < 6; i++) data[8 + i] = password_cmd[i];
    data[14] = rfid_password_index;           // 卡数量
    data[15] = as608_store_index - 1;          // 指纹数量 (store_index从1开始)
    esp8266_send_frame(ESP_CMD_DEV_INFO, data, 16);
}

/*********************************************************************
 * @fn      esp8266_report_rfid_fail
 * @brief   标记刷卡失败待发送 (ISR安全)
 */
void esp8266_report_rfid_fail(void)
{
    esp_pending_rfid_fail = 1;
}

/*********************************************************************
 * @fn      esp8266_report_finger_fail
 * @brief   标记指纹失败待发送 (ISR安全)
 */
void esp8266_report_finger_fail(void)
{
    esp_pending_finger_fail = 1;
}

/*==========================================================================
 *  帧命令处理 (任务上下文中执行)
 *========================================================================*/
/*********************************************************************
 * @fn      esp8266_process_frame
 * @brief   解析并执行一帧完整收到的命令(由esp8266_task调用)
 */
void esp8266_process_frame(void)
{
    u8 ack_data[2];
    u8 i;

    /*
     * 只要能收到来自ESP8266的完整有效帧(例如APP发来的查询状态/远程命令)，
     * 就说明串口链路和WiFi云端链路已经打通。
     * 这样即使启动时序错过了0xF0，也能允许本地开锁/告警事件继续上报。
     */
    if (esp_rx_cmd != ESP_CMD_WIFI_DISCONN) {
        esp_wifi_connected = 1;
    }

    switch (esp_rx_cmd) {

        // ─── 远程开锁 0x80 ──────────────────────────────────
        case ESP_CMD_REMOTE_UNLOCK:
            servo_flag = 1;
            show_flag = 1;
            USART3_SendByte('3');

            ack_data[0] = ESP_CMD_REMOTE_UNLOCK;
            ack_data[1] = ACK_OK;
            esp8266_send_frame(ESP_CMD_REMOTE_ACK, ack_data, 2);

            // 延迟上报开门 (下周期发送)
            esp_pending_open = 1;
            esp_pending_open_method = OPEN_METHOD_REMOTE;
            break;

        // ─── 远程锁门 0x81 ──────────────────────────────────
        case ESP_CMD_REMOTE_LOCK:
            servo_flag = 0;
            ack_data[0] = ESP_CMD_REMOTE_LOCK;
            ack_data[1] = ACK_OK;
            esp8266_send_frame(ESP_CMD_REMOTE_ACK, ack_data, 2);
            esp_pending_close = 1;  // 延迟发送
            break;

        // ─── 查询状态 0x82 ──────────────────────────────────
        case ESP_CMD_QUERY_STATUS:
            {
                u8 battery = 100;  // TODO: 接入ADC电池检测
                u8 door    = servo_flag ? DOOR_UNLOCKED : DOOR_LOCKED;
                u8 status_data[2] = {battery, door};
                esp8266_send_frame(ESP_CMD_HEARTBEAT, status_data, 2);
                // 同时发送设备信息(含密码), APP连接后立即同步
                esp8266_report_device_info();
            }
            break;

        // ─── 修改开门密码 0x83 ──────────────────────────────
        case ESP_CMD_ADD_PW_USER:
            if (esp_rx_data_len >= 6) {
                // 直接覆写当前开门密码(沿用既有命令字)
                for (i = 0; i < 6 && i < esp_rx_data_len; i++) {
                    password[i] = esp_rx_buffer[i];
                }
                ack_data[0] = ESP_CMD_ADD_PW_USER;
                ack_data[1] = ACK_OK;
                show_flag = 6;  // 显示"设置成功"
            } else {
                ack_data[0] = ESP_CMD_ADD_PW_USER;
                ack_data[1] = ACK_FAIL;
            }
            esp8266_send_frame(ESP_CMD_REMOTE_ACK, ack_data, 2);
            break;

        // ─── 删除用户 0x84 ──────────────────────────────────
        case ESP_CMD_DEL_USER:
            if (esp_rx_data_len >= 1) {
                u8 idx = esp_rx_buffer[0];
                ack_data[0] = ESP_CMD_DEL_USER;
                if (idx < rfid_password_index && idx < 4) {
                    // 删除卡号(将后续元素前移)
                    for (i = idx; i < rfid_password_index - 1 && i < 3; i++) {
                        string_copy(rfid[i + 1], rfid[i], 4);
                    }
                    memset(rfid[rfid_password_index - 1], 0, 4);
                    rfid_password_index--;
                    ack_data[1] = ACK_OK;
                } else {
                    ack_data[1] = ACK_FAIL;
                }
            } else {
                ack_data[0] = ESP_CMD_DEL_USER;
                ack_data[1] = ACK_FAIL;
            }
            esp8266_send_frame(ESP_CMD_REMOTE_ACK, ack_data, 2);
            break;

        // ─── 修改管理员密码 0x85 ─────────────────────────────
        case ESP_CMD_CHG_ADMIN_PW:
            if (esp_rx_data_len >= 6) {
                for (i = 0; i < 6; i++) {
                    password_cmd[i] = esp_rx_buffer[i];
                }
                ack_data[0] = ESP_CMD_CHG_ADMIN_PW;
                ack_data[1] = ACK_OK;
            } else {
                ack_data[0] = ESP_CMD_CHG_ADMIN_PW;
                ack_data[1] = ACK_FAIL;
            }
            esp8266_send_frame(ESP_CMD_REMOTE_ACK, ack_data, 2);
            break;

        // ─── 恢复出厂 0x87 ──────────────────────────────────
        case ESP_CMD_FACTORY_RESET:
            memset(password, 6, 6);
            memset(password_cmd, 0, sizeof(password_cmd));
            password_cmd[0] = 5;
            password_cmd[1] = 5;
            password_cmd[2] = 5;
            password_cmd[3] = 6;
            password_cmd[4] = 6;
            password_cmd[5] = 6;
            memset(rfid, 0, sizeof(rfid));
            rfid_password_index = 0;
            PS_Empty();
            as608_store_index = 1;
            ack_data[0] = ESP_CMD_FACTORY_RESET;
            ack_data[1] = ACK_OK;
            esp8266_send_frame(ESP_CMD_REMOTE_ACK, ack_data, 2);
            break;

        // ─── 远程重启 0x88 ──────────────────────────────────
        case ESP_CMD_REBOOT:
            ack_data[0] = ESP_CMD_REBOOT;
            ack_data[1] = ACK_OK;
            esp8266_send_frame(ESP_CMD_REMOTE_ACK, ack_data, 2);
            Delay_Ms(100);  // 等ACK发完
            NVIC_SystemReset();
            break;

        // ─── ESP8266就绪 0xF0 ───────────────────────────────
        case ESP_CMD_WIFI_READY:
            esp_wifi_connected = 1;
            show_flag = 12;  // 显示"网络已连接"
            USART3_SendByte('2');//播报网络已连接
            USART3_SendByte('2');
            // 立即上报设备信息
            esp8266_report_device_info();
            esp_first_run = 0;
            break;

        // ─── WiFi断开 0xF1 ──────────────────────────────────
        case ESP_CMD_WIFI_DISCONN:
            esp_wifi_connected = 0;
            show_flag = 13;  // 显示"网络已断开"
            USART3_SendByte('2');//播报网络已断开
            USART3_SendByte('3');
            // 立即上报设备信息
            esp8266_report_device_info();
            esp_first_run = 0;
            break;

        // ─── 通用ACK 0xFF ───────────────────────────────────
        case ESP_CMD_GENERIC_ACK:
            // ESP确认收到, 可用于重传队列管理(暂不处理)
            break;

        default:
            break;
    }
}

/*==========================================================================
 *  调度器任务 (每50ms调用一次)
 *========================================================================*/

/*********************************************************************
 * @fn      esp_flush_pending
 * @brief  处理延迟发送队列 (仅在task上下文调用, 不在ISR中)
 * @note   每个任务周期只发一条, 防止串口拥塞
 */
static void esp_flush_pending(void)
{
    if (!esp_wifi_connected) return;

    if (esp_pending_open) {
        esp_pending_open = 0;
        u8 data[2];
        data[0] = esp_pending_open_method;
        data[1] = 0xFF;
        esp8266_send_frame(ESP_CMD_OPEN, data, 2);
        return;  // 每次只发一条
    }
    if (esp_pending_close) {
        esp_pending_close = 0;
        esp8266_send_frame(ESP_CMD_CLOSE, NULL, 0);
        return;
    }
    if (esp_pending_pw_err) {
        esp_pending_pw_err = 0;
        esp8266_send_frame(ESP_CMD_PW_ERR, NULL, 0);
        return;
    }
    if (esp_pending_lock_alert) {
        esp_pending_lock_alert = 0;
        esp8266_send_frame(ESP_CMD_LOCK_ALERT, NULL, 0);
        return;
    }
    if (esp_pending_rfid_fail) {
        esp_pending_rfid_fail = 0;
        esp8266_send_frame(ESP_CMD_RFID_FAIL, NULL, 0);
        return;
    }
    if (esp_pending_finger_fail) {
        esp_pending_finger_fail = 0;
        esp8266_send_frame(ESP_CMD_FINGER_FAIL, NULL, 0);
        return;
    }
}

/*********************************************************************
 * @fn      esp8266_task
 * @brief   ESP8266调度任务: 处理延迟队列/接收帧/心跳/门状态
 * @note    在scheduler中注册, 建议周期 ≤ 50ms
 */
void esp8266_task(void)
{
    u8 current_door;

    // ── 0. 处理延迟发送队列 ──────────────────────────────────
    esp_flush_pending();

    // ── 1. 处理接收到的完整帧 ────────────────────────────────
    if (esp_rx_complete) {
        esp8266_process_frame();
        esp_rx_complete = 0;
    }

    // ── 2. 接收超时保护 (帧间隔超过50ms则重置状态机) ────────
    if (esp_rx_state != ESP_RX_IDLE &&
        (uwTick - esp_rx_last_byte) > 50) {
        esp_rx_state = ESP_RX_IDLE;
    }

    // ── 3. WiFi连接后首次上报设备信息 ─────────────────────────
    if (esp_first_run && esp_wifi_connected &&
        (uwTick - esp_last_heartbeat) > 2000) {
        esp8266_report_device_info();
        esp_first_run = 0;
    }

    // // ── 4. 心跳包 (每10秒) ───────────────────────────────────
    // if (esp_wifi_connected &&
    //     (uwTick - esp_last_heartbeat) >= 10000) 
    // {
    //     esp_last_heartbeat = uwTick;

    //     u8 battery = 100;
    //     u8 door    = servo_flag ? DOOR_UNLOCKED : DOOR_LOCKED;
    //     u8 hb_data[2] = {battery, door};
    //     esp8266_send_frame(ESP_CMD_HEARTBEAT, hb_data, 2);
    // }

    // ── 5. 门状态变化检测 (变化时立即上报心跳) ───────────────
    current_door = servo_flag ? 1 : 0;
    if (current_door != esp_last_door) {
        esp_last_door = current_door;
        // 立即发送状态更新(不等心跳定时器)
        {
            u8 battery = 100;
            u8 hb_data[2] = {battery, current_door};
            esp8266_send_frame(ESP_CMD_HEARTBEAT, hb_data, 2);
            esp_last_heartbeat = uwTick;  // 重置心跳倒计时
        }
        if (current_door == 0) {
            esp_pending_close = 1;  // 用延迟队列发送关门事件
        }
    }
}
