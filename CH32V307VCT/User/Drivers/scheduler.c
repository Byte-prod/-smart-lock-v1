/**
 * scheduler.c — 简易协作式任务调度器
 *
 * 调度周期由TIM3硬件定时器驱动(uwTick 每1ms递增)
 * 每个任务有独立的执行周期(rate_ms), 到期自动调用
 */

#include "driver_all.h"

/** 任务描述结构体 */
typedef struct
{
    void (*task_func)(void);    // 任务函数指针
    uint32_t rate_ms;           // 执行周期(毫秒)
    uint32_t last_run;          // 上次执行时间戳
} scheduler_t;

/** 任务列表 — 按需添加新任务 */
scheduler_t tasks[] =
{
    {led_task,      1,  0},    // LED闪烁, 每1ms
    {key_task,     10,  0},    // 按键扫描, 每10ms
    {lcd_task,     20,  0},    // LCD刷新, 每20ms
    {as608_task,   25,  0},    // 指纹检测, 每25ms
    {servo_task,   30,  0},    // 舵机控制, 每30ms
    {esp8266_task, 50,  0},    // WiFi通信, 每50ms
};

uint8_t task_num = 0;

/** 初始化 — 计算任务数量 */
void scheduler_init(void)
{
    task_num = sizeof(tasks) / sizeof(scheduler_t);
}

/** 主调度循环 — 由main()的while(1)调用 */
void scheduler_run(void)
{
    for (int i = 0; i < task_num; i++)
    {
        uint32_t now = uwTick;
        if (now >= tasks[i].last_run + tasks[i].rate_ms)
        {
            tasks[i].last_run = now;
            tasks[i].task_func();
        }
    }
}
