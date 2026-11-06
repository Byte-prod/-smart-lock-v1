#ifndef __ESP8266_H
#define __ESP8266_H


/*==========================================================================
 *  CH32V307 ↔ ESP8266 串口通信协议 v1.0
 *  物理层: UART6, 115200-8-N-1
 *  帧格式: AA 55 | CMD | LEN | DATA... | CSUM
 *========================================================================*/

// ── 帧头 ──────────────────────────────────────────────────────
#define ESP_FRAME_HEAD1  0xAA
#define ESP_FRAME_HEAD2  0x55

// ── CH32 → ESP8266 命令集 (上行) ───────────────────────────────
#define ESP_CMD_OPEN         0x01  // 开门事件   [方式1B][用户ID 0~4B]
#define ESP_CMD_CLOSE        0x02  // 关门事件   无数据
#define ESP_CMD_PW_ERR       0x03  // 密码错误   无数据
#define ESP_CMD_LOCK_ALERT   0x04  // 锁定告警(3次错误) 无数据
#define ESP_CMD_HEARTBEAT    0x05  // 心跳包     [电量1B][门状态1B]
#define ESP_CMD_LOG          0x06  // 操作日志   [操作类型1B][结果1B]
#define ESP_CMD_DEV_INFO     0x07  // 设备信息   [主版本1B][次版本1B][开门密码6B][管理员密码6B][卡数1B][指纹数1B]
#define ESP_CMD_REMOTE_ACK   0x08  // 远程ACK    [原命令字1B][结果码1B]
#define ESP_CMD_RFID_FAIL     0x09  // 刷卡失败   无数据
#define ESP_CMD_FINGER_FAIL   0x0A  // 指纹失败   无数据

// ── ESP8266 → CH32 命令集 (下行) ───────────────────────────────
#define ESP_CMD_REMOTE_UNLOCK  0x80  // 远程开锁    [超时秒1B]
#define ESP_CMD_REMOTE_LOCK    0x81  // 远程锁门    无数据
#define ESP_CMD_QUERY_STATUS   0x82  // 查询状态    无数据
#define ESP_CMD_ADD_PW_USER    0x83  // 修改开门密码 [密码6B]
#define ESP_CMD_DEL_USER       0x84  // 删除用户    [用户索引1B]
#define ESP_CMD_CHG_ADMIN_PW   0x85  // 修改管理员密码 [新密码6B]
#define ESP_CMD_SYNC_TIME      0x86  // 同步时间    [年1B][月1B][日1B][时1B][分1B][秒1B]
#define ESP_CMD_FACTORY_RESET  0x87  // 恢复出厂    无数据
#define ESP_CMD_REBOOT         0x88  // 远程重启    无数据
#define ESP_CMD_WIFI_READY     0xF0  // ESP就绪     无数据
#define ESP_CMD_WIFI_DISCONN   0xF1  // WiFi断开    无数据
#define ESP_CMD_GENERIC_ACK    0xFF  // 通用ACK     [原命令字1B][0x00]

// ── 开门方式枚举 ───────────────────────────────────────────────
#define OPEN_METHOD_PASSWORD   0x01
#define OPEN_METHOD_RFID       0x02
#define OPEN_METHOD_FINGER     0x03
#define OPEN_METHOD_REMOTE     0x04
#define OPEN_METHOD_KEY        0x05

// ── 门状态 ─────────────────────────────────────────────────────
#define DOOR_LOCKED     0x00
#define DOOR_UNLOCKED   0x01
#define DOOR_ABNORMAL   0x02

// ── ACK结果码 ──────────────────────────────────────────────────
#define ACK_OK     0x00
#define ACK_FAIL   0x01
#define ACK_BUSY   0x02

// ── 固件版本 ───────────────────────────────────────────────────
#define FW_VER_MAJOR  1
#define FW_VER_MINOR  0

// ── 接收缓冲区大小 ─────────────────────────────────────────────
#define ESP_RX_BUF_SIZE  64
#define ESP_TX_BUF_SIZE  64

// ── 接收状态机状态 ─────────────────────────────────────────────
typedef enum {
    ESP_RX_IDLE = 0,        // 等待帧头1 (0xAA)
    ESP_RX_WAIT_HEAD2,      // 等待帧头2 (0x55)
    ESP_RX_WAIT_CMD,        // 等待命令字
    ESP_RX_WAIT_LEN,        // 等待数据长度
    ESP_RX_WAIT_DATA,       // 接收数据段
    ESP_RX_WAIT_CSUM        // 等待校验和
} esp_rx_state_t;

// ── 全局变量声明 ───────────────────────────────────────────────
extern u8  esp_rx_buffer[ESP_RX_BUF_SIZE];
extern u8  esp_rx_cmd;
extern u8  esp_rx_data_len;
extern u8  esp_rx_index;
extern u8  esp_rx_complete;       // 完整帧接收标志
extern u8  esp_wifi_connected;    // WiFi连接状态
extern u16 esp_heartbeat_timer;   // 心跳倒计时(ms)
extern u8  esp_open_method;       // 当前开门方式(供验证模块设置)

// ── API 函数声明 ───────────────────────────────────────────────

// 基础通信
void esp8266_send_frame(u8 cmd, u8 *data, u8 len);
void esp8266_rx_byte(u8 byte);                     // 喂入一个字节(由UART6 ISR调用)
void esp8266_process_frame(void);                   // 处理一帧完整命令

// 事件上报 (对CH32其他模块暴露)
void esp8266_report_open(u8 method, u8 user_index);
void esp8266_report_close(void);
void esp8266_report_pw_error(void);
void esp8266_report_lock_alert(void);
void esp8266_report_device_info(void);
void esp8266_report_rfid_fail(void);
void esp8266_report_finger_fail(void);

// 调度器任务
void esp8266_task(void);

#endif
