# STM32CubeMX 配置清单（智能手表）

> 目标芯片：**STM32F103C8T6**。新建工程后按本清单逐项配置，生成代码后再把
> `Firmware/App/` 下的源码加入工程即可（集成步骤见《集成说明.md》）。
> 引脚 User Label 必须与 `App/Inc/app_config.h` 中的宏一致。

## 0. 快速开始：直接用 `SmartWatch.ioc`（推荐）

本目录已提供预配置工程 **`Firmware/SmartWatch.ioc`**，用 **STM32CubeMX 6.11+**（或
STM32CubeIDE 内置 CubeMX）打开即可，省去大半手动点击。

> ⚠️ 该 `.ioc` 为预填模板，CubeMX 打开时会重算时钟树派生值与 NVIC 优先级。
> **打开后请逐项核对以下 9 点，确认无误再 Generate Code：**

1. **Clock Configuration 页**：HSE 8MHz → PLL ×9 → **SYSCLK = 72MHz**、APB1 = 36MHz；
   若顶部提示时钟冲突，点 **“Resolve Clock Issues”** 自动修正。
2. **System Core → RCC**：High Speed Clock (HSE) = *Crystal/Ceramic Resonator*，
   Low Speed Clock (LSE) = *Crystal/Ceramic Resonator*。
3. **Timers → RTC**：已勾 *Activate Clock Source* + *Activate Calendar*，时钟源 = LSE。
4. **Middleware → FREERTOS**：已启用；Interface = CMSIS_V2（本项目代码用原生 API，V1 亦可）。
5. **System Core → SYS → Timebase Source = TIM4**（务必不是 SysTick）。
6. **Connectivity → USART2**：Asynchronous，**9600** 8-N-1；NVIC 勾 USART2 global interrupt。
7. **Timers → TIM2**：Combined Channels = **Encoder Mode**（PA0/PA1）。
8. **GPIO/NVIC**：PA4 = EXTI4 上升沿、PB12 = EXTI12 下降沿+上拉，二者中断已使能；
   PA8 = GPIO 输出（PWR_HOLD）；PB10/PB11 = GPIO 输出（软件 I2C）。
9. 全部确认 → **Generate Code**，再按《集成说明.md》接入 `App/` 源码。

> 若 CubeMX 打开 `.ioc` 报错、或某外设未被识别，**按下方第 1～9 节手动配置**（完全等效的兜底方案）。

---

## 1. RCC / 时钟树
- **RCC → High Speed Clock (HSE)**：Crystal/Ceramic Resonator（板载 8MHz 晶振）
- **RCC → Low Speed Clock (LSE)**：Crystal/Ceramic Resonator（板载 **32.768kHz** 晶振，供 RTC）
- **Clock Configuration**：HSE 8MHz → PLL ×9 → **SYSCLK = 72MHz**；APB1 = 36MHz
- 若批次板卡未焊 LSE，可改 RTC 时钟源为 LSI（精度略低，需软件校正）

## 2. RTC（走时核心）
- **Timers → RTC → Activate Clock Source** 勾选
- **Activate Calendar** 勾选（HAL 用日历模拟日期）
- RTC Clock 源选 **LSE**
- 用途：休眠（STOP）期间持续走时，避免时间丢失

## 3. I2C1（MPU6050，硬件 I2C）
- **Connectivity → I2C1**：I2C 模式
- 引脚：**PB6 = I2C1_SCL，PB7 = I2C1_SDA**
- Speed Mode：Standard / Fast（100k 或 400k 均可）

## 4. USART2（JDY-31 蓝牙）
- **Connectivity → USART2**：Asynchronous
- 引脚：**PA2 = USART2_TX，PA3 = USART2_RX**
- 参数：**9600** 8-N-1（JDY-31 出厂波特率）
- **NVIC**：勾选 USART2 global interrupt（接收用中断）

## 5. TIM2（EC11 编码器，硬件正交解码）
- **Timers → TIM2 → Combined Channels = Encoder Mode**
- 引脚：**PA0 = TIM2_CH1，PA1 = TIM2_CH2**
- Encoder Mode：TI1 and TI2；Counter Period = 65535；两通道 Polarity = Rising

## 6. GPIO / EXTI
| 功能 | 引脚 | 模式 |
| :-- | :-- | :-- |
| OLED 软件 I2C SCL | PB10 | Output Open-Drain，Pull-Up（代码也会再 Init） |
| OLED 软件 I2C SDA | PB11 | Output Open-Drain，Pull-Up |
| MPU6050 INT（抬腕唤醒） | PA4 | GPIO_EXTI4，**Rising**，Pull-Down |
| EC11 微动按键 | PB12 | GPIO_EXTI12，**Falling**，Pull-Up |
| 一键开机维持 | PA8 | Output Push-Pull，**上电后置高保持自锁** |

- **NVIC**：勾选 **EXTI line4 interrupt**（PA4）与 **EXTI line[15:10] interrupt**（PB12）
- EXTI 同时作为 STOP 模式的唤醒源

## 7. FreeRTOS（中间件）
- **Middleware → FREERTOS**：Interface 选 **CMSIS_V2**（或 V1）
- 可在 `Tasks and Queues` 留空，统一由本项目 `App_CreateTasks()` 用原生 API 创建
- **重要：SYS → Timebase Source 改为 TIM4**（或除 SysTick 外的任意定时器），
  避免 FreeRTOS 占用 SysTick 与 HAL 时基冲突
- Heap：heap_4；TOTAL_HEAP_SIZE 建议 ≥ 8KB

## 8. 电源 / 低功耗
- PWR 时钟默认开启；代码中调用 `HAL_PWR_EnterSTOPMode()` 进入 STOP
- 唤醒后调用 CubeMX 生成的 `SystemClock_Config()` 重配时钟（已在 app_tasks.c 中处理）

## 9. 生成代码
- Project Manager → Toolchain：**STM32CubeIDE**
- 勾选 “Generate peripheral initialization as a pair of .c/.h files”
- 生成后按《集成说明.md》接入 `App/` 源码
