# StaySafe 🌊

> **Real-time IoT flood monitoring and early warning system**
> STM32F401 · FreeRTOS · LoRa 433MHz · ESP32 · MQTT · Firebase · Android

---

## What is StaySafe?

StaySafe is an end-to-end flood detection system designed for autonomous field deployment near canals, rivers, and drainage infrastructure. A waterproof ultrasonic sensor continuously measures water distance while a rain sensor detects precipitation. All safety decisions are made on the STM32 microcontroller running FreeRTOS — the rest of the chain (ESP32, cloud, mobile app) only forwards and displays those decisions.

When the water level reaches a danger threshold, the system simultaneously triggers local LEDs and a buzzer, transmits an alert over LoRa, publishes to MQTT, and automatically calls the registered user's phone number via the Android app.

The system is built for low power — it wakes every 10 minutes, reads sensors for 30 seconds, then returns to STOP mode using the STM32 RTC wakeup timer.

---

## System Architecture

```
┌──────────────────────────────────────────────┐
│              STM32F401 Nucleo-64              │
│                                              │
│  Sensors:                                    │
│    AJ-SR04M ──► TIM3 Input Capture (PB5)     │
│    FC-37     ──► ADC1 CH0 (PA0) + GPIO (PA1) │
│                                              │
│  FreeRTOS CMSIS-V2 Tasks:                    │
│  ┌─────────────────┬──────────────────────┐  │
│  │ readingSensor   │ Normal priority       │  │
│  │ alertTask       │ High priority         │  │
│  │ LoraTask        │ AboveNormal priority  │  │
│  │ LcdTask         │ BelowNormal priority  │  │
│  └─────────────────┴──────────────────────┘  │
│                                              │
│  IPC: 3 Queues + 1 EventGroup                │
│  Power: RTC wakeup → STOP mode (LSI clock)   │
│  SX1278 Ra-02 ──► SPI1 (PA5/PA6/PA7/PB6)    │
└──────────────────┬───────────────────────────┘
                   │ LoRa 433 MHz
                   │ SF7 · BW125 · CR4/5
                   ▼
┌──────────────────────────────────────────────┐
│                   ESP32                      │
│                                              │
│  FreeRTOS (Arduino):                         │
│  ┌────────────┬───────────────────────────┐  │
│  │ taskLoRa   │ Core 1 — LoRa RX, parse   │  │
│  │ taskMqtt   │ Core 0 — WiFi + MQTT TX   │  │
│  └────────────┴───────────────────────────┘  │
│                                              │
│  Queue depth 20 — burst tolerant             │
│  Auto WiFi + MQTT reconnect every 5s         │
└──────────────────┬───────────────────────────┘
                   │ MQTT (mqtt-dashboard.com:1883)
                   ▼
┌──────────────────────────────────────────────┐
│         Firebase + Android App               │
│                                              │
│  Topics consumed:                            │
│    safty/state      → SAFE/WARNING/DANGER    │
│    safty/waterlevel → NORMAL/HIGH/CRITICAL   │
│    safty/rainfall   → NONE/LIGHT/HEAVY       │
│                                              │
│  Features:                                   │
│    Firebase Auth (login/signup)              │
│    Firebase Realtime DB (phone number)       │
│    Auto emergency call on DANGER             │
│    Manual SOS button                         │
└──────────────────────────────────────────────┘
```

---

## Hardware

| Component | Role | Connection |
|-----------|------|-----------|
| STM32F401 Nucleo-64 | Main controller | — |
| AJ-SR04M | Waterproof ultrasonic, water distance | TRIG→PB4, ECHO→PB5 (TIM3 CH2 AF2) |
| FC-37 | Rain sensor (analog + digital) | AO→PA0 (ADC1), DO→PA1 |
| Ra-02 SX1278 | LoRa 433MHz TX/RX | SPI1: PA5/PA6/PA7, NSS→PB6, RST→PC7, DIO0→PA10 |
| ESP32 DevKit | WiFi + LoRa gateway | NSS→5, RST→14, DIO0→2 |
| I2C LCD 16×2 | Engineer diagnostic display | I2C3, address 0x4E |
| Red LED | CRITICAL alert indicator | PB7 |
| Yellow LED | WARNING alert indicator | PD2 |
| Buzzer | CRITICAL audio alert | PC11 |
| Green LED | LoRa TX success indicator | PB0 |

> **Note:** The AJ-SR04M is powered at **3.3V** to keep ECHO output within STM32 GPIO safe limits. TRIG accepts 3.3V logic (above the 2.4V threshold). Effective range at 3.3V is ~250cm, sufficient for flood monitoring applications.

---

## STM32 Firmware

### FreeRTOS Task Design

The firmware uses **FreeRTOS CMSIS-V2** running on the STM32F401 at 84MHz. Four tasks run concurrently, synchronized through message queues and an event group.

```
readingSensorTask  ──QueueData──►  LoraTask
        │          ──QueueLCD──►   LcdTask
        │          ──TaskNotify──► alertTask
        │
        └── xEventGroupWaitBits(ALL_DONE_BITS)
                ▲           ▲           ▲
          LORA_DONE    LCD_DONE    ALERT_DONE
```

**readingSensorTask (Normal priority)**
- Collects 5 ultrasonic samples using TIM3 input capture
- Applies insertion sort + median filter to reject outliers
- Reads rain sensor (ADC + GPIO)
- Evaluates alert level (0/1/2) — the only place safety decisions are made
- Sends `SensorData` struct to both `QueueData` and `QueueLCD`
- Sends alert level via `xTaskNotify` to `alertTask`
- Waits on event group for all consumer tasks before next cycle

**alertTask (High priority)**
- Wakes on task notification from `readingSensorTask`
- Controls LEDs and buzzer based on alert level
- Uses `lastAlertLevel` to avoid redundant GPIO writes
- Level 2 (CRITICAL): Red LED + Buzzer ON
- Level 1 (WARNING): Yellow LED ON
- Level 0 (SAFE): All outputs OFF

**LoraTask (AboveNormal priority)**
- Receives `SensorData` from queue (500ms timeout)
- Formats LoRa payload including alert level and rain category:
  ```
  D:45.2 R:1823 W:1 A:1 RF:1
  ```
- Transmits via SX1278 using Domski HAL library
- Green LED on TX success, red LED on failure

**LcdTask (BelowNormal priority)**
- Receives `SensorData` from queue (portMAX_DELAY)
- Displays engineer-readable raw values:
  ```
  Line 1: level:45.2cm
  Line 2: rain:1.47V YES
  ```
- Uses integer arithmetic only — no float printf

### Sensor Data Struct

```c
typedef struct {
    float    distancecm;   // median-filtered ultrasonic reading
    uint16_t rainIn;       // raw ADC value 0–4095
    uint8_t  rainexistence;// 1 = rain detected (digital pin)
    uint8_t  rainCategory; // 0=NONE, 1=LIGHT, 2=HEAVY
    uint8_t  alertLevel;   // 0=SAFE, 1=WARNING, 2=CRITICAL
} SensorData;
```

### Alert Logic

All thresholds evaluated on STM32 — ESP32 never re-evaluates:

```c
if (distancecm <= 50)
    → alertLevel = 2  // CRITICAL — water dangerously close
                      // fires regardless of rain

else if (distancecm <= 70 && rainexistence && rainADC < 2000)
    → alertLevel = 1  // WARNING — rising water with active rain

else
    → alertLevel = 0  // SAFE
```

Rain category classification:
```c
rainADC < 500   → HEAVY (2)
rainADC < 2000  → LIGHT (1)
otherwise       → NONE  (0)
```

### Ultrasonic Measurement (TIM3 Input Capture)

The AJ-SR04M echo signal is captured on **PB5 configured as TIM3 CH2 AF2**. The ISR records rising and falling edge timestamps and computes pulse width with 16-bit overflow handling:

```c
if (IC_Value2 >= IC_Value1)
    pulseWidth_us = IC_Value2 - IC_Value1;
else
    pulseWidth_us = (65535 - IC_Value1) + IC_Value2 + 1;

distancecm = pulseWidth_us / 58.0f;
```

**5-sample median filter** applied each cycle:
1. Collect 5 samples with 10ms guard between pings
2. Reject invalid pulses (< 150µs or > 23200µs)
3. Insertion sort valid samples
4. Average middle values (discard min and max)

This reduces noise from surface irregularities and reflections by ~80% compared to single-sample reading.

### RTC Low-Power Sleep

The STM32 implements a **work/sleep duty cycle** using the internal RTC and STOP mode:

```
Active 30s → STOP 10min → Active 30s → STOP 10min → ...
```

**RTC configuration:**
- Clock source: LSI (32kHz internal, survives STOP mode)
- Wakeup clock: `RTC_WAKEUPCLOCK_CK_SPRE_16BITS` for long durations (1Hz ticks)
- Wakeup clock: `RTC_WAKEUPCLOCK_RTCCLK_DIV16` for short durations (0.5ms ticks)
- Backup register `RTC_BKP_DR0` prevents time reset on warm reboot

**Sleep entry sequence:**
```c
HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
SetRtcWakeup_ms(SLEEP_DURATION_MS);          
__HAL_RTC_WAKEUPTIMER_EXTI_ENABLE_IT();
HAL_SuspendTick();
HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
```

**Wake sequence (clock restoration required after STOP):**
```c
SystemClock_Config();    // PLL must be reconfigured after STOP
HAL_ResumeTick();
HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
MX_SPI1_Init();          // SPI peripheral state lost in STOP
MX_I2C3_Init();
MX_TIM3_Init();
SX1278_init(...);        // LoRa module must be re-initialized
```

**Sensor boot protection:** PB4 (TRIG) is forced LOW via direct register access before `HAL_Init()` to prevent the AJ-SR04M from locking up if TRIG is high during MCU reset:
```c
__HAL_RCC_GPIOB_CLK_ENABLE();
GPIOB->MODER |= (1 << (4*2));
GPIOB->ODR   &= ~(1 << 4);
```

---

## ESP32 Gateway

The ESP32 runs two FreeRTOS tasks pinned to separate cores, communicating via a depth-20 queue for burst tolerance.

### taskLoRa (Core 1, priority 2)
- Polls `LoRa.parsePacket()` every 2ms
- Parses two packet formats from STM32:
  - `D:45.2 R:1823 W:1 A:1 RF:2` — full sensor data
  - `ALERT:2` — priority alert-only packet
- Extracts `A:` (alert level) and `RF:` (rain category) fields
- Enqueues translated MQTT messages (non-blocking, drops if full)

### taskMqtt (Core 0, priority 1)
- Manages WiFi and MQTT connection with 5s keep-alive check
- Drains queue with 0ms timeout so `mqttClient.loop()` runs continuously
- Calls `mqttClient.loop()` between each publish to keep TCP stack alive
- Auto-restarts WiFi on disconnect, auto-reconnects MQTT

### Translation table (STM32 → MQTT)

| STM32 field | Value | MQTT topic | Published value |
|-------------|-------|-----------|----------------|
| `A:` | 0 | `safty/state` | `SAFE` |
| `A:` | 1 | `safty/state` | `WARNING` |
| `A:` | 2 | `safty/state` | `DANGER` |
| `A:` | 0 | `safty/waterlevel` | `NORMAL` |
| `A:` | 1 | `safty/waterlevel` | `HIGH` |
| `A:` | 2 | `safty/waterlevel` | `CRITICAL` |
| `RF:` | 0 | `safty/rainfall` | `NONE` |
| `RF:` | 1 | `safty/rainfall` | `LIGHT` |
| `RF:` | 2 | `safty/rainfall` | `HEAVY` |

> The ESP32 never re-evaluates thresholds. All safety decisions originate from the STM32.

---

## Android Application

Built in **Java** for Android, the app provides a clean user-facing interface requiring no technical knowledge.

### Screens
- **Login / Signup / Forgot Password** — Firebase Authentication
- **Account** — user profile and phone number management
- **Home** — live flood status dashboard

### Home Screen

The main screen subscribes to three MQTT topics and updates in real time:

**Status card (top)** — full-width colored card:
- 🔵 Blue `#BFDBF7` → `SAFE` — "No flood risk in your area right now"
- 🟠 Orange `#F4A261` → `WARNING` — "Water level rising, stay alert"
- 🔴 Dark red `#800E13` → `DANGER` — "Flood risk detected in your area!"

**Water Level card** — displays `NORMAL` / `HIGH` / `CRITICAL`

**Rainfall card** — displays `NONE` / `LIGHT` / `HEAVY`

**Emergency SOS button** — manual dial to civil protection number

**Automatic emergency call** — triggered when `safty/state` receives `DANGER`. Phone number loaded per-user from Firebase Realtime Database at login.

### Firebase Structure
```
Users/
  {uid}/
    phoneNumber: "+216XXXXXXXX"
```

---

## LoRa Configuration

| Parameter | Value |
|-----------|-------|
| Frequency | 433 MHz |
| Spreading Factor | SF7 |
| Signal Bandwidth | 125 kHz |
| Coding Rate | 4/5 |
| CRC | Enabled |
| Sync Word | Default (0x12) |
| Max payload | 64 bytes |
| TX Power | 11 dBm |

Antenna: U.FL/IPEX 433MHz connected to Ra-02 module. **Never transmit without antenna connected** — the RF amplifier will be damaged.

---

## MQTT Topics

| Topic | Direction | Values |
|-------|-----------|--------|
| `safty/state` | ESP32 → App | `SAFE` / `WARNING` / `DANGER` |
| `safty/waterlevel` | ESP32 → App | `NORMAL` / `HIGH` / `CRITICAL` |
| `safty/rainfall` | ESP32 → App | `NONE` / `LIGHT` / `HEAVY` |

Broker: `mqtt-dashboard.com:1883` (public, no auth)

---

## Tech Stack

| Layer | Technology |
|-------|-----------|
| MCU | STM32F401RE, STM32 HAL, CubeIDE |
| RTOS | FreeRTOS CMSIS-V2 (STM32), Arduino FreeRTOS (ESP32) |
| LoRa (STM32) | Domski SX1278 HAL library |
| LoRa (ESP32) | sandeepmistry Arduino-LoRa |
| MQTT (ESP32) | Joel Gaehwiler arduino-mqtt |
| Cloud broker | mqtt-dashboard.com (public) |
| Auth & DB | Firebase Authentication, Firebase Realtime Database |
| Mobile | Android Java, Eclipse Paho MQTT |

---

## Project Structure

```
StaySafe-Project/
├── STM32/
│   ├── Core/
│   │   ├── Src/
│   │   │   ├── main.c              # All FreeRTOS tasks + RTC sleep logic
│   │   │   └── stm32f4xx_it.c      # TIM3 input capture ISR
│   │   └── Inc/
│   │       └── main.h
│   ├── Drivers/
│   │   ├── SX1278/                 # Domski LoRa HAL driver
│   │   ├── ultrasonic/             # TIM3 IC capture callback
│   │   └── i2c_lcd/                # HD44780 I2C driver
│   └── ...
├── ESP32/
│   └── esp32Lora/
│       └── esp32Lora.ino       # Dual-core LoRa+MQTT gateway
└── Android/
    └── app/src/main/
        ├── java/com/example/staysafe/
        │   ├── Home.java           # Main dashboard + MQTT + emergency call
        │   ├── account.java        # User profile
        │   ├── Login.java
        │   ├── Signup.java
        │   └── MqttClientHelper.java
        └── res/layout/
            └── activity_home.xml
```

---

## Wiring Reference

### STM32 → SX1278 Ra-02

| SX1278 Pin | STM32 Pin | Notes |
|-----------|-----------|-------|
| VCC | 3.3V (CN6) | |
| GND | GND (CN6) | |
| SCK | PA5 | SPI1 CLK |
| MISO | PA6 | SPI1 MISO |
| MOSI | PA7 | SPI1 MOSI |
| NSS | PB6 | Software CS |
| RST | PC7 | Hardware reset |
| DIO0 | PA10 | TX/RX done IRQ |

### STM32 → AJ-SR04M

| Sensor Pin | STM32 Pin | Notes |
|-----------|-----------|-------|
| VCC | 3.3V | Not 5V — keeps ECHO safe |
| GND | GND | |
| TRIG | PB4 | GPIO output |
| ECHO | PB5 | TIM3 CH2 AF2 input capture |

---

## Known Limitations

- MQTT broker is public (`mqtt-dashboard.com`) — no authentication or encryption. Suitable for academic demonstration only.
- AJ-SR04M at 3.3V has ~250cm maximum range vs 400cm at 5V.
- LSI oscillator accuracy is ±5% — RTC wakeup timing may drift slightly over long deployments. An external 32.768kHz crystal (LSE) would improve accuracy.
- Emergency call triggers on every CRITICAL MQTT message. A cooldown timer should be added before production use.

---

## Academic Context

**Degree:** Embedded Systems Engineering (Licence)
**Institution:** ISIMG, Tunisia
**Academic Year:** 2025–2026

---

## License

MIT License — free to use, modify, and distribute with attribution.
