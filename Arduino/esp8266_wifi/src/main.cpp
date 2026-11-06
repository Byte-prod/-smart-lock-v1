/*
 * 智能门锁 ESP8266 WiFi网桥 v3.1 — 调试增强版
 * ===============================================
 * CH32(UART6) ↔ Serial ↔ WiFi+TLS:8883 ↔ EMQX 国内版 ↔ 手机H5
 * Broker: 见 secrets.h（已抽离敏感配置，不会上传 GitHub）
 * 依赖: ESP8266WiFi(内置) + PubSubClient(platformio.ini 的 lib_deps 自动装)
 * 已从 Arduino IDE 移植到 PlatformIO (board=nodemcuv2)
 *
 * 调试方法:
 *   1. ESP8266 通过 USB-TTL 连接 PC（不要连CH32）
 *   2. 串口监视器 115200 波特率
 *   3. 观察日志: WiFi连接 → MQTT连接 → 收发数据
 *   4. 调通后设 DEBUG 0，重新烧录，再接回 CH32
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// 前向声明：PlatformIO 的 main.cpp 不会像 .ino 那样自动生成函数原型，
// 而 wifiGo() 里调用了定义在其后的 sFrame()，所以需要手动声明。
void sFrame(uint8_t cmd, uint8_t* data, uint8_t len);

/* ═══════════════════════════════════════════════════
 *  调试开关 (调通后改0, 否则会干扰CH32串口通信!)
 * ═══════════════════════════════════════════════════ */
#define DEBUG 0

#if DEBUG
  #define DBG(x)     Serial.print(x)
  #define DBGLN(x)   Serial.println(x)
  #define DBGF(f,v)  Serial.printf(f,v)
#else
  #define DBG(x)
  #define DBGLN(x)
  #define DBGF(f,v)
#endif

/* ═══════════════════════════════════════════════════
 *  用户配置（敏感信息已抽到 secrets.h，不会上传 GitHub）
 *  首次编译前：把 secrets.h.example 复制为 secrets.h 并填入真实值
 * ═══════════════════════════════════════════════════ */
#include "secrets.h"

/* ═══════════════════════════════════════════════════
 *  AA55 帧协议
 * ═══════════════════════════════════════════════════ */
#define H1  0xAA
#define H2  0x55
#define RXB 128

/* ═══════════════════════════════════════════════════
 *  全局对象
 * ═══════════════════════════════════════════════════ */
#define LED_PIN 2   // ESP-12F内置LED (GPIO2, 低电平亮)

BearSSL::WiFiClientSecure tls;
PubSubClient mqtt(tls);

uint32_t lastRecon=0, lastPing=0, lastCh32Data=0, lastReadySent=0;
bool wifiOk=false;
uint32_t lastLedBlink=0;

// 串口接收
uint8_t rBuf[RXB], rIdx=0, rCmd=0, rDL=0, rSt=0;
uint16_t rSum=0;

// 下行缓冲
uint8_t  dnBuf[RXB];
uint16_t dnLen=0;
bool     dnRdy=false;

/* ═══════════════════════════════════════════════════
 *  工具函数
 * ═══════════════════════════════════════════════════ */
void printHex(uint8_t* data, uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    if (data[i] < 0x10) Serial.print('0');
    Serial.print(data[i], HEX);
    Serial.print(' ');
  }
}

const char* cmdName(uint8_t cmd) {
  switch (cmd) {
    case 0x01: return "开门事件";
    case 0x02: return "关门事件";
    case 0x03: return "密码错误";
    case 0x04: return "锁定告警";
    case 0x05: return "心跳包";
    case 0x07: return "设备信息";
    case 0x08: return "远程ACK";
    case 0x80: return "远程开锁";
    case 0x81: return "远程锁门";
    case 0x82: return "查询状态";
    case 0x83: return "添加密码";
    case 0x84: return "删除用户";
    case 0x85: return "修改密码";
    case 0x87: return "恢复出厂";
    case 0xF0: return "WiFi就绪";
    case 0xF1: return "WiFi断开";
    case 0xFF: return "通用ACK";
    default:   return "未知";
  }
}

/* ═══════════════════════════════════════════════════
 *  WiFi
 * ═══════════════════════════════════════════════════ */
void wifiGo(){
  DBGLN("══════════════════════════════════");
  DBGLN("[WiFi] 开始连接...");
  DBG("  SSID: "); DBGLN(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int n=0;
  while(WiFi.status()!=WL_CONNECTED && n<30){
    delay(500); n++;
    DBG(".");
  }
  DBGLN("");

  wifiOk=(WiFi.status()==WL_CONNECTED);

  if(wifiOk) {
    DBGLN("[WiFi] ✅ 连接成功!");
    DBG("  IP地址: "); DBGLN(WiFi.localIP().toString());
    sFrame(0xF0, NULL, 0);  // 通知CH32: WiFi就绪
  } else {
    DBGLN("[WiFi] ❌ 连接失败! 请检查:");
    DBGLN("  1. WiFi名称是否正确");
    DBGLN("  2. WiFi密码是否正确");
    DBGLN("  3. 路由器是否正常工作");
  }
  DBGLN("══════════════════════════════════");
}

/* ═══════════════════════════════════════════════════
 *  MQTT 回调 — 收到云端下行消息
 * ═══════════════════════════════════════════════════ */
void mqttCB(char* topic, byte* payload, unsigned int len){
  if(len==0) return;

  DBG("[MQTT] ⬇ 收到云端下行 topic=");
  DBG(topic);
  DBG(" len=");
  DBG(len);
  DBG(" data=[");
  printHex(payload, len>16?16:len);
  DBGLN("]");

  // 转存到下行缓冲, loop中转发给CH32
  if(len>RXB) len=RXB;
  memcpy(dnBuf, payload, len);
  dnLen=len; dnRdy=true;
  digitalWrite(LED_PIN, LOW);  // LED亮(收到云端数据)
  lastLedBlink=millis();
}

/* ═══════════════════════════════════════════════════
 *  AA55帧发送 (到CH32)
 * ═══════════════════════════════════════════════════ */
void sFrame(uint8_t cmd, uint8_t* data, uint8_t len){
  uint16_t sum=cmd+len;
  for(uint8_t i=0;i<len;i++) sum+=data[i];

  DBG("[串口→CH32] 发送帧: cmd=0x");
  if(cmd<0x10) DBG("0");
  DBGF("%x", cmd);
  DBG(" (");
  DBG(cmdName(cmd));
  DBG(") len=");
  DBG(len);
  if(len>0){
    DBG(" data=[");
    printHex(data, len);
    DBG("]");
  }
  DBGLN("");

  Serial.write(H1); Serial.write(H2);
  Serial.write(cmd); Serial.write(len);
  if(len) Serial.write(data,len);
  Serial.write((uint8_t)(sum&0xFF));
  Serial.flush();
}

/* ═══════════════════════════════════════════════════
 *  AA55帧接收 (从CH32来)
 * ═══════════════════════════════════════════════════ */
void pFrame(uint8_t b){
  switch(rSt){
    case 0: if(b==H1) rSt=1; break;
    case 1: if(b==H2) rSt=2; else if(b!=H1) rSt=0; break;
    case 2: rCmd=b; rSt=3; break;
    case 3: rDL=b;
      if(rDL>RXB) rSt=0;
      else if(rDL==0){rSum=rCmd;rSt=5;}
      else{rIdx=0;rSum=rCmd+rDL;rSt=4;}
      break;
    case 4: rSum+=b; rBuf[rIdx++]=b; if(rIdx>=rDL) rSt=5; break;
    case 5: if((uint8_t)(rSum&0xFF)==b){
      // ——— 帧解析成功 ———
      lastCh32Data=millis();  // 记录收到CH32数据的时间
      DBG("[CH32→串口] 收到帧: cmd=0x");
      if(rCmd<0x10) DBG("0");
      DBGF("%x", rCmd);
      DBG(" (");
      DBG(cmdName(rCmd));
      DBG(") len=");
      DBG(rDL);
      if(rDL>0){
        DBG(" data=[");
        printHex(rBuf, rDL);
        DBG("]");
      }

      // 重建完整帧发布到MQTT
      uint8_t out[RXB+5]; uint16_t ol=0;
      out[ol++]=H1;out[ol++]=H2;out[ol++]=rCmd;out[ol++]=rDL;
      for(uint8_t i=0;i<rDL;i++) out[ol++]=rBuf[i];
      out[ol++]=b;

      if(mqtt.connected()) {
        mqtt.publish(TOPIC_UP, out, ol);
        digitalWrite(LED_PIN, LOW);  // LED亮
        lastLedBlink=millis();
        DBG(" → 已发布到MQTT");
      } else {
        DBG(" → ⚠ MQTT未连接, 丢弃");
      }
      DBGLN("");

      // ACK
      uint8_t ak[2]={rCmd,0};
      sFrame(0xFF,ak,2);
    } else {
      DBGLN("[CH32→串口] ⚠ 校验失败, 丢弃");
    }
    rSt=0; break;
    default: rSt=0;
  }
}

/* ═══════════════════════════════════════════════════
 *  setup
 * ═══════════════════════════════════════════════════ */
void setup(){
  Serial.begin(115200);
  Serial.setTimeout(5);

  DBGLN("");
  DBGLN("╔══════════════════════════════════╗");
  DBGLN("║  智能门锁 ESP8266 WiFi网桥 v3.1 ║");
  DBGLN("║  Broker: EMQX Cloud 杭州国内版  ║");
  DBGLN("╚══════════════════════════════════╝");
  DBGLN("");
  DBG("[系统] 芯片ID: "); DBGLN(String(ESP.getChipId(), HEX));
  DBG("[系统] Flash大小: "); DBG(ESP.getFlashChipSize()/1024); DBGLN("KB");
  DBGLN("[系统] 调试模式: 开启 (调通后设DEBUG=0)");
  DBGLN("");

  // LED指示灯
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // 灭(低电平亮)

  // TLS 跳过证书验证
  DBGLN("[TLS] 设置跳过证书验证...");
  tls.setInsecure();

  DBG("[MQTT] 目标服务器: "); DBG(MQTT_BROKER); DBG(":"); DBGLN(MQTT_PORT);
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCB);
  mqtt.setKeepAlive(60);

  wifiGo();

  DBGLN("[系统] setup完成, 进入loop");
  DBGLN("");
}

/* ═══════════════════════════════════════════════════
 *  loop
 * ═══════════════════════════════════════════════════ */
void loop(){
  uint32_t n=millis();
  static uint32_t lastStatus=0;

  // LED自动熄灭 (200ms后)
  if(lastLedBlink && (n-lastLedBlink>200)){
    digitalWrite(LED_PIN, HIGH);
    lastLedBlink=0;
  }

  // 每隔5秒打印一次状态摘要
  if(n-lastStatus>5000){
    lastStatus=n;
    bool cw=(WiFi.status()==WL_CONNECTED);
    DBG("[状态] WiFi:");
    DBG(cw?"✅":"❌");
    DBG(" MQTT:");
    DBG(mqtt.connected()?"✅":"❌");
    DBG(" 运行时间:");
    DBG(n/1000);
    DBGLN("s");
  }

  // WiFi检测
  bool cw=(WiFi.status()==WL_CONNECTED);
  if(cw!=wifiOk){
    wifiOk=cw;
    if(cw){
      DBGLN("[WiFi] ✅ 重新连接!");
      sFrame(0xF0,NULL,0);
    } else {
      DBGLN("[WiFi] ❌ 断开!");
      sFrame(0xF1,NULL,0);
    }
  }

  // 定期重发WIFI_READY(每3秒), 直到收到CH32数据为止
  // 修复: CH32可能因启动时序错过首次0xF0, 导致esp_wifi_connected永远为0
  if(wifiOk && lastCh32Data==0 && (n-lastReadySent>3000)){
    lastReadySent=n;
    sFrame(0xF0,NULL,0);
  }

  // MQTT连接
  if(wifiOk && !mqtt.connected()){
    if(n-lastRecon>5000){
      lastRecon=n;
      String cid=String(DEVICE_ID)+"_"+String(ESP.getChipId(),HEX);
      DBG("[MQTT] 尝试连接... ClientID=");
      DBGLN(cid);

      if(mqtt.connect(cid.c_str(), MQTT_USER, MQTT_PASS)){
        DBGLN("[MQTT] ✅ 连接成功!");
        DBG("[MQTT] 订阅主题: ");
        DBGLN(TOPIC_DOWN);
        mqtt.subscribe(TOPIC_DOWN);
        lastPing=n;
      } else {
        DBG("[MQTT] ❌ 连接失败! rc=");
        DBGLN(mqtt.state());
        DBGLN("  错误码含义:");
        DBGLN("  -4: 连接超时(检查端口是否正确, 是否开启了TLS)");
        DBGLN("  -2: 网络不可达(检查Broker地址)");
        DBGLN("   5: 认证失败(检查用户名密码)");
      }
    }
  }

  // MQTT心跳
  if(mqtt.connected()) mqtt.loop();

  // CH32串口数据 → MQTT
  while(Serial.available()) {
    pFrame(Serial.read());
  }

  // MQTT → CH32串口 (下发命令)
  if(dnRdy){
    DBG("[MQTT→CH32] 转发下行命令: len=");
    DBG(dnLen);
    DBG(" data=[");
    printHex(dnBuf, dnLen>16?16:dnLen);
    DBGLN("]");

    if(dnLen>=5 && dnBuf[0]==H1 && dnBuf[1]==H2){
      // 已经AA55帧, 直接转发
      DBGLN("[MQTT→CH32] 完整AA55帧, 直接转发");
      Serial.write(dnBuf, dnLen);
      Serial.flush();
    } else if(dnLen>0 && dnBuf[0]>=0x80 && dnBuf[0]<=0x88){
      // 裸命令, 包成帧
      DBG("[MQTT→CH32] 裸命令0x");
      DBGF("%x", dnBuf[0]);
      DBG(", 包装为AA55帧发送");
      DBGLN("");
      sFrame(dnBuf[0], dnBuf+1, dnLen-1);
    } else {
      DBGLN("[MQTT→CH32] ⚠ 未知格式, 丢弃");
    }
    dnRdy=false;
  }
}
