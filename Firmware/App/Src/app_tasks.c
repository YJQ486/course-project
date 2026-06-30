/**
 * @file    app_tasks.c
 * @brief   FreeRTOS 四大任务与系统集成实现
 *
 * 任务划分（对齐《系统设计文档》）：
 *   Task_Power(4)  : 500ms 周期检查空闲，超时进 STOP；EXTI 唤醒
 *   Task_Sensor(3) : 20ms 采样 + 计步 + 低频落盘 Flash
 *   Task_UI(2)     : 编码器/按键输入 + 菜单渲染 + 读 RTC
 *   Task_Comm(1)   : 蓝牙文本协议收发（UART 中断 + 信号量）
 *
 * 同步机制：g_ui 由互斥锁 s_mtx 保护（跨任务读写的共享结构体）；
 *           UART 收到完整一行用二值信号量 s_sem_uart 通知 Task_Comm。
 */
#include "app_tasks.h"
#include "app_config.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "bsp_oled.h"
#include "bsp_mpu6050.h"
#include "bsp_encoder.h"
#include "bsp_rtc.h"
#include "bsp_flash.h"
#include "pedometer.h"
#include "menu.h"
#include "protocol.h"

#include <string.h>

extern UART_HandleTypeDef huart2;        /* CubeMX 生成 */
extern void SystemClock_Config(void);    /* CubeMX 生成，STOP 唤醒后重配时钟 */

/* ---------------- 共享状态 ---------------- */
static UiData_t          g_ui;
static SemaphoreHandle_t s_mtx;          /* 保护 g_ui */
static SemaphoreHandle_t s_sem_uart;     /* UART 整行就绪通知 */
static volatile uint32_t g_last_activity_ms;
static uint8_t           s_uart_rx;      /* UART 单字节接收缓冲 */
static uint16_t          s_last_day;

static inline void mark_activity(void) { g_last_activity_ms = HAL_GetTick(); }

/* ====================== 初始化 ====================== */
void App_Init(void)
{
    OLED_Init();
    OLED_ShowString(3, 18, "SmartWatch...");
    OLED_Display();

    MPU6050_Init();
    MPU6050_EnableMotionInt(20, 1);      /* 抬腕唤醒：阈值/持续可现场标定 */
    Encoder_Init();

    /* 从 Flash 载入步数历史 */
    StepStore_t st;
    if (Flash_LoadSteps(&st)) {
        Pedometer_Init(st.total_steps, st.today_steps);
    } else {
        Pedometer_Init(0, 0);
    }
    s_last_day = RTC_GetDayIndex();

    Menu_Init();
    memset(&g_ui, 0, sizeof(g_ui));
    g_ui.total_steps = Pedometer_GetTotal();
    g_ui.today_steps = Pedometer_GetToday();

    /* 启动 UART 单字节中断接收 */
    HAL_UART_Receive_IT(&huart2, &s_uart_rx, 1);

    mark_activity();
}

/* ====================== 低功耗 ====================== */
static void enter_stop_mode(void)
{
    OLED_DisplayOff();
    HAL_SuspendTick();
    /* 进入 STOP；RTC(LSE) 仍走时，仅 EXTI（按键/MPU抬腕）可唤醒 */
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
    /* —— 被唤醒 —— STOP 后时钟回到 HSI，需重配 */
    SystemClock_Config();
    HAL_ResumeTick();
    OLED_DisplayOn();
    mark_activity();
    /* 注：严格低功耗应启用 FreeRTOS tickless idle，此处给出基础框架 */
}

/* ====================== 任务实现 ====================== */
static void Task_Power(void *arg)
{
    (void)arg;
    for (;;) {
        if ((HAL_GetTick() - g_last_activity_ms) > PWR_IDLE_TIMEOUT_MS) {
            enter_stop_mode();
        }
        vTaskDelay(pdMS_TO_TICKS(PWR_CHECK_PERIOD_MS));
    }
}

static void Task_Sensor(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    uint32_t save_cnt = 0;
    for (;;) {
        MPU_Data_t d;
        if (MPU6050_Read(&d) == 0) {
            Pedometer_Update(&d, HAL_GetTick());

            /* 跨天检测：当天步数清零，累计保留 */
            uint16_t day = RTC_GetDayIndex();
            if (day != s_last_day) { Pedometer_NewDay(); s_last_day = day; }

            if (xSemaphoreTake(s_mtx, pdMS_TO_TICKS(10)) == pdTRUE) {
                g_ui.sensor      = d;
                g_ui.total_steps = Pedometer_GetTotal();
                g_ui.today_steps = Pedometer_GetToday();
                xSemaphoreGive(s_mtx);
            }

            /* 低频落盘：约每 30s 写一次 Flash（1500 * 20ms） */
            if (++save_cnt >= 1500) {
                save_cnt = 0;
                StepStore_t st = {0};
                st.total_steps = Pedometer_GetTotal();
                st.today_steps = Pedometer_GetToday();
                st.day_index   = s_last_day;
                Flash_SaveSteps(&st);
            }
        }
        vTaskDelayUntil(&last, pdMS_TO_TICKS(PED_SAMPLE_MS));
    }
}

static void Task_UI(void *arg)
{
    (void)arg;
    for (;;) {
        int8_t  delta = Encoder_GetDelta();
        uint8_t key   = Encoder_KeyPressed();
        if (delta != 0 || key) mark_activity();

        Menu_Handle(delta, key);

        UiData_t snap;
        if (xSemaphoreTake(s_mtx, pdMS_TO_TICKS(10)) == pdTRUE) {
            snap = g_ui;
            xSemaphoreGive(s_mtx);
        }
        RTC_GetTime(&snap.time);        /* 时间实时从 RTC 取 */
        Menu_Render(&snap);

        vTaskDelay(pdMS_TO_TICKS(30));  /* ~33Hz 刷新与输入扫描 */
    }
}

static void Task_Comm(void *arg)
{
    (void)arg;
    char buf[64];
    TickType_t last_push = xTaskGetTickCount();
    for (;;) {
        /* 等待一整行命令（最多 1s），超时则主动推送一次状态 */
        if (xSemaphoreTake(s_sem_uart, pdMS_TO_TICKS(1000)) == pdTRUE) {
            uint8_t h, m, s;
            ProtoCmd_t cmd = Protocol_ParseLine(&h, &m, &s);
            if (cmd == CMD_SET_TIME) { RTC_SetTime(h, m, s); mark_activity(); }
            /* CMD_SYNC 落到下面统一推送 */
        }

        if ((xTaskGetTickCount() - last_push) >= pdMS_TO_TICKS(1000)) {
            last_push = xTaskGetTickCount();
            UiData_t snap;
            if (xSemaphoreTake(s_mtx, pdMS_TO_TICKS(10)) == pdTRUE) {
                snap = g_ui;
                xSemaphoreGive(s_mtx);
            }
            RTC_GetTime(&snap.time);
            int n = Protocol_BuildStatus(buf, sizeof(buf), &snap);
            if (n > 0) HAL_UART_Transmit(&huart2, (uint8_t *)buf, n, 100);
        }
    }
}

/* ====================== 创建 ====================== */
void App_CreateTasks(void)
{
    s_mtx      = xSemaphoreCreateMutex();
    s_sem_uart = xSemaphoreCreateBinary();

    xTaskCreate(Task_Power,  "Power",  STK_POWER,  NULL, PRIO_POWER,  NULL);
    xTaskCreate(Task_Sensor, "Sensor", STK_SENSOR, NULL, PRIO_SENSOR, NULL);
    xTaskCreate(Task_UI,     "UI",     STK_UI,     NULL, PRIO_UI,     NULL);
    xTaskCreate(Task_Comm,   "Comm",   STK_COMM,   NULL, PRIO_COMM,   NULL);
}

/* ====================== 中断回调（HAL weak 覆写） ====================== */
/* UART 接收完成：喂入协议解析，整行就绪则通知 Task_Comm，并重新 arm */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == huart2.Instance) {
        BaseType_t hpw = pdFALSE;
        if (Protocol_FeedRxByte((char)s_uart_rx)) {
            xSemaphoreGiveFromISR(s_sem_uart, &hpw);
        }
        HAL_UART_Receive_IT(&huart2, &s_uart_rx, 1);
        portYIELD_FROM_ISR(hpw);
    }
}

/* EXTI：编码器按键 / MPU6050 抬腕中断 -> 记录活动（兼作 STOP 唤醒源） */
void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    if (pin == ENC_KEY_PIN || pin == MPU_INT_PIN) {
        mark_activity();
    }
}
