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
/* MPU 运动中断计数：显示在 SENSOR 页，用于现场标定抬腕阈值（晃动模块看它是否增长） */
volatile uint32_t        g_mpu_int_count;
static UiData_t          g_ui;
static SemaphoreHandle_t s_mtx;          /* 保护 g_ui */
static SemaphoreHandle_t s_sem_uart;     /* UART 整行就绪通知 */
static volatile uint32_t g_last_activity_ms;
static uint8_t           s_uart_rx;      /* UART 单字节接收缓冲 */
static uint16_t          s_last_day;

/* 记录最近一次落盘的步数，避免无变化时反复擦写 Flash（延长寿命） */
static uint32_t          s_saved_total = 0xFFFFFFFFu;
static uint32_t          s_saved_today = 0xFFFFFFFFu;

static inline void mark_activity(void) { g_last_activity_ms = HAL_GetTick(); }

/* 若步数较上次落盘有变化则写入 Flash；force=1 用于熄屏/休眠前强制落盘。
 * 仅在“有变化”时擦写，把 F1 内部 Flash 的擦写次数从“每 30s 一次”降到
 * “仅步数变动时”，避免几天就写穿末页（擦写寿命约 1 万次）。 */
static void persist_steps(uint8_t force)
{
    uint32_t total = Pedometer_GetTotal();
    uint32_t today = Pedometer_GetToday();
    if (!force && total == s_saved_total && today == s_saved_today) {
        return;                          /* 无变化，跳过擦写 */
    }
    StepStore_t st = {0};
    st.total_steps = total;
    st.today_steps = today;
    st.day_index   = s_last_day;
    if (Flash_SaveSteps(&st) == 0) {     /* 0=成功 */
        s_saved_total = total;
        s_saved_today = today;
    }
}

/* ====================== 初始化 ====================== */
void App_Init(void)
{
    OLED_Init();
    OLED_ShowString(3, 18, "SmartWatch...");
    OLED_Display();

    MPU6050_Init();
    /* 抬腕唤醒：阈值 15(≈15mg)/持续 3ms——对抬腕足够灵敏、又能滤掉瞬时噪声。
     * 用 SENSOR 页的 INT 计数现场标定：误触发多就调大 thr/dur，抬腕不触发就调小。 */
    MPU6050_EnableMotionInt(15, 3);
    Encoder_Init();

    /* 从 Flash 载入步数历史 */
    StepStore_t st;
    if (Flash_LoadSteps(&st)) {
        Pedometer_Init(st.total_steps, st.today_steps);
    } else {
        Pedometer_Init(0, 0);
    }
    s_last_day = RTC_GetDayIndex();
    /* 以载入值作为落盘基线，避免开机后立刻重写一遍未变化的 Flash */
    s_saved_total = Pedometer_GetTotal();
    s_saved_today = Pedometer_GetToday();

    Menu_Init();
    memset(&g_ui, 0, sizeof(g_ui));
    g_ui.total_steps = Pedometer_GetTotal();
    g_ui.today_steps = Pedometer_GetToday();

    /* 注意：UART 中断接收在 App_CreateTasks() 里、信号量创建之后才启动，
     * 避免开机瞬间蓝牙来字节时对尚未创建的 s_sem_uart 做 GiveFromISR。 */

    mark_activity();
}

/* ====================== 低功耗 ====================== */
static void enter_stop_mode(void)
{
    persist_steps(1);            /* 休眠前强制落盘，防止睡眠期间断电丢步数 */
    OLED_DisplayOff();
    HAL_SuspendTick();

    /* 清残留中断，带着“干净”的状态进 STOP：
     * 1) 读 MPU INT_STATUS 复位运动中断，保证 PA4 为低（否则抬腕无新上升沿）；
     * 2) 清 EXTI 挂起位与 NVIC 挂起，否则旧标志会让 WFI 立刻返回、根本睡不下去。 */
    MPU6050_ReadIntStatus();

    /* 关中断做“清挂起 -> WFI”原子操作：SysTick 每 1ms 一次，若在清挂起和
     * WFI 之间来一次 tick，WFI 直接穿透，表现为“该熄屏时屏又亮回来”。
     * PRIMASK 置位不影响 WFI 被 EXTI 唤醒（标准 disable-WFI-enable 序列）。 */
    __disable_irq();
    __HAL_GPIO_EXTI_CLEAR_IT(MPU_INT_PIN);
    __HAL_GPIO_EXTI_CLEAR_IT(ENC_KEY_PIN);
    NVIC_ClearPendingIRQ(EXTI4_IRQn);
    NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;

    /* 进入 STOP；RTC(LSE) 仍走时，仅 EXTI（按键/MPU抬腕）可唤醒 */
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
    /* —— 被唤醒 —— STOP 后时钟回到 HSI，需重配 */
    SystemClock_Config();
    __enable_irq();
    HAL_ResumeTick();

    /* STOP 期间 USART2 无时钟，期间到达的字节会留下帧错误/半字节状态，
     * 不清掉的话 RX 中断可能永久失效（蓝牙“只能收不能发/收发全无”常见根因之一） */
    HAL_UART_AbortReceive(&huart2);
    HAL_UART_Receive_IT(&huart2, &s_uart_rx, 1);

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
    uint8_t  i2c_fail = 0;
    for (;;) {
        MPU_Data_t d;
        if (MPU6050_Read(&d) == 0) {
            i2c_fail = 0;
            /* 读 INT_STATUS 重新“布防”锁存的运动中断：清醒时把锁存的高电平清掉，
             * 使下一次动作能再产生上升沿（否则锁存高电平后 INT 计数不再增长、
             * 熄屏进 STOP 时 PA4 也带着高电平进去，抬腕再无新边沿）。 */
            MPU6050_ReadIntStatus();
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

            /* 低频落盘：约每 60s 检查一次（3000 * 20ms），且仅在步数
             * 变化时才真正擦写 Flash，兼顾掉电保存与 Flash 寿命。 */
            if (++save_cnt >= 3000) {
                save_cnt = 0;
                persist_steps(0);
            }
        } else if (i2c_fail < 5) {
            i2c_fail++;
        }

        /* I2C 连续失败退避：F1 硬件 I2C 异常时（总线卡死/接触不良），HAL 每次
         * 调用都要空转 ~25ms 等 BUSY 超时；本任务优先级高于 UI，若按 20ms 周期
         * 立即重试会把 UI 任务彻底饿死（表现为界面卡死、按键无响应）。 */
        if (i2c_fail >= 5) {
            vTaskDelay(pdMS_TO_TICKS(200));
            last = xTaskGetTickCount();     /* 重置基准，避免 DelayUntil 追赶式连发 */
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

        static UiData_t snap;   /* static：互斥锁取失败时沿用上一帧，避免渲染未初始化数据 */
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
        /* RX 看门狗：HAL 的收/发共用一把锁，中断里重挂接收若恰好撞上
         * HAL_UART_Transmit 持锁会返回 BUSY，接收从此断链；这里兜底重挂。 */
        if (huart2.RxState == HAL_UART_STATE_READY) {
            HAL_UART_Receive_IT(&huart2, &s_uart_rx, 1);
        }

        /* 等待一整行命令（最多 1s），超时则主动推送一次状态 */
        if (xSemaphoreTake(s_sem_uart, pdMS_TO_TICKS(1000)) == pdTRUE) {
            uint8_t h, m, s;
            ProtoCmd_t cmd = Protocol_ParseLine(&h, &m, &s);
            if (cmd == CMD_SET_TIME) { RTC_SetTime(h, m, s); mark_activity(); }
            /* CMD_SYNC 落到下面统一推送 */
        }

        if ((xTaskGetTickCount() - last_push) >= pdMS_TO_TICKS(1000)) {
            last_push = xTaskGetTickCount();
            static UiData_t snap;
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

    /* 信号量已就绪，再启动 UART 单字节中断接收（避免竞态） */
    HAL_UART_Receive_IT(&huart2, &s_uart_rx, 1);

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
        mark_activity();     /* 蓝牙会话进行中不休眠（STOP 下 UART 无时钟收不到任何数据） */
        if (Protocol_FeedRxByte((char)s_uart_rx)) {
            xSemaphoreGiveFromISR(s_sem_uart, &hpw);
        }
        HAL_UART_Receive_IT(&huart2, &s_uart_rx, 1);
        portYIELD_FROM_ISR(hpw);
    }
}

/* UART 错误（过载/帧错/噪声）：HAL 出错后会撤销接收中断且不再自动恢复，
 * 必须清标志并重挂，否则蓝牙接收从此永久失效。 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == huart2.Instance) {
        __HAL_UART_CLEAR_OREFLAG(huart);
        if (huart->RxState == HAL_UART_STATE_READY) {
            HAL_UART_Receive_IT(&huart2, &s_uart_rx, 1);
        }
    }
}

/* EXTI：编码器按键 -> 记录活动；MPU6050 运动中断 -> 只计数。
 * 注意：运动中断不能 mark_activity —— 佩戴状态下手一直在微动，中断以
 * 采样率反复触发，活动时间被不停刷新，手表永远不熄屏（“不响应熄屏”根因）。
 * 抬腕唤醒依然有效：STOP 下任一 EXTI 都能唤醒，唤醒后由 enter_stop_mode
 * 尾部的 mark_activity() 给足亮屏时间。 */
void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    if (pin == ENC_KEY_PIN) {
        mark_activity();
    } else if (pin == MPU_INT_PIN) {
        g_mpu_int_count++;
    }
}
