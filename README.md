# 智能门锁 V1 版

基于 **CH32V307 主控 + ESP8266 WiFi 网桥 + EMQX Cloud** 的远程智能门锁系统，支持密码、刷卡、指纹、远程开锁，手机 H5 远程控制。

## 项目介绍

```
┌─────────────┐  UART   ┌──────────────┐  MQTT/TLS  ┌────────────┐  WSS  ┌──────────┐
│  CH32V307   │────────▶│   ESP8266    │───────────▶│ EMQX Cloud │◀─────│ 手机 H5   │
│  （主控）    │◀────────│  （WiFi网桥） │◀───────────│            │      │          │
└─────────────┘         └──────────────┘            └────────────┘      └──────────┘
```

- **CH32V307**：主控，处理密码/刷卡/指纹输入、舵机开锁、LCD 显示、语音播报
- **ESP8266**：WiFi 网桥，把 CH32 串口帧转成 MQTT 消息上云，并接收云端下行命令
- **EMQX Cloud**：MQTT 消息服务器（国内杭州节点，TLS 加密）
- **手机 H5**：远程开锁、查看开门记录

### 功能

- 🔑 密码开锁（开门密码 + 管理员密码，3 次错误锁定 30 秒）
- 💳 IC 卡开锁（最多 4 张卡）
- 👆 指纹开锁（AS608 模块）
- 📱 远程开锁 / 锁门

### 仓库内容

| 内容 | 说明 |
|------|------|
| `CH32V307VCT/` | 主控固件（MounRiver Studio 工程） |
| `Arduino/esp8266_wifi/` | ESP8266 网桥固件（PlatformIO 工程） |
| `天问51/` | 51 单片机程序（天问编程） |
| `远程控制/_智能门锁_手机安装包.zip` | 手机 APP 安装包 |

---

## MQTT 部署（EMQX Cloud）

MQTT 服务器使用 [EMQX Cloud](https://cloud.emqx.com) 免费 Serverless 部署（国内杭州节点）：

1. 注册并登录 [EMQX Cloud](https://cloud.emqx.com)
2. 创建一个 **Serverless 部署**，地域选**杭州（国内）**
3. 部署创建后，进入「**认证**」页面，添加两个认证用户（用户名和密码自定）：
   - 一个给 ESP8266 固件用（如 `smartlock`）
   - 一个给手机 H5 用（如 `admin`）
4. 进入「**监听器**」页面，确认以下端口已开启：
   - **TLS 端口 8883**（ESP8266 使用）
   - **WebSocket WSS 端口 8084**，路径 `/mqtt`（手机 H5 使用）
5. 记下部署的 **Broker 地址**（形如 `xxxxxx.ala.cn-hangzhou.emqxsl.cn`）

## 怎么用

### 1. 配置 ESP8266 网桥

进入 `Arduino/esp8266_wifi/src/` 目录：

1. 复制 `secrets.h.example`，改名为 `secrets.h`
2. 在 `secrets.h` 里填入你自己的配置：

```c
#define WIFI_SSID       "你的WiFi名称"
#define WIFI_PASSWORD   "******"      // 你的WiFi密码
#define MQTT_BROKER     "你的MQTT服务器地址"
#define MQTT_PORT       8883
#define MQTT_USER       "你的MQTT用户名"
#define MQTT_PASS       "******"      // 你的MQTT密码
#define DEVICE_ID       "你的设备ID"
```

各配置项含义：

| 配置项 | 含义 |
|--------|------|
| WIFI_SSID | 你家 WiFi 名称 |
| WIFI_PASSWORD | 你家 WiFi 密码 |
| MQTT_BROKER | EMQX Cloud 的 Broker 地址 |
| MQTT_PORT | 8883（TLS 加密端口） |
| MQTT_USER | 在 EMQX「认证」里创建的用户名 |
| MQTT_PASS | 对应的密码 |
| DEVICE_ID | 设备 ID（主题为 `/lock/<DEVICE_ID>/up` 和 `/down`） |

> `secrets.h` 已被 `.gitignore` 排除，**不会上传到仓库**，密码只有你自己知道。

### 2. 烧录固件

- **ESP8266**：用 PlatformIO 打开 `Arduino/esp8266_wifi/`，编译烧录（板型 `nodemcuv2`）
- **CH32V307**：用 MounRiver Studio 打开 `CH32V307VCT/`，编译烧录
- **门锁密码**：在 `CH32V307VCT/User/Drivers/key.c` 里改成你自己的：

```c
u8 password[6]     = {6,6,6,6,6,6};   // 开门密码
u8 password_cmd[6] = {5,5,5,6,6,6};   // 管理员密码
```

### 3. 使用

1. CH32 + ESP8266 上电，语音播报「启动成功」→「网络已连接」
2. 安装手机 APP（`远程控制/_智能门锁_手机安装包.zip`）
3. APP 里填入 MQTT Broker 地址、用户名、密码、设备 ID，点击连接
4. 即可远程开锁、查看开门记录

---

## 安全提醒

- ⚠️ 请勿使用出厂默认密码，生产使用务必改成你自己的强密码
- ⚠️ ESP8266 当前跳过 TLS 证书验证，生产环境请启用证书校验
- ⚠️ 免费 EMQX Cloud 有连接数和流量限制
