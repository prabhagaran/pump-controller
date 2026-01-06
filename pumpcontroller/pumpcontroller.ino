/************************************************************
 * Project : Smart Pump Controller (RTOS)
 * Mode    : AUTO + MANUAL via SERIAL
 * MCU     : ESP32
 ************************************************************/

#include <Arduino.h>

/* ================= PIN DEFINITIONS ================= */
#define LED_PIN           2
#define SOIL_SENSOR_PIN   34
#define RELAY_PIN         26

#define SOIL_DRY_THRESHOLD 2000

/* ================= RTOS HANDLES ================= */
QueueHandle_t soilQueue;
QueueHandle_t commandQueue;
TaskHandle_t relayTaskHandle = NULL;

/* ================= ENUMS ================= */
enum PumpCommand : uint32_t {
  CMD_PUMP_OFF = 0,
  CMD_PUMP_ON  = 1,
  CMD_BACK_TO_AUTO
};

enum ControlMode {
  MODE_AUTO,
  MODE_MANUAL
};

enum PumpState {
  AUTO_OFF,
  AUTO_ON,
  MANUAL_OFF,
  MANUAL_ON
};

/* ================= HEARTBEAT TASK ================= */
void heartbeatTask(void *pvParameters) {
  pinMode(LED_PIN, OUTPUT);
  while (1) {
    digitalWrite(LED_PIN, HIGH);
    vTaskDelay(pdMS_TO_TICKS(200));
    digitalWrite(LED_PIN, LOW);
    vTaskDelay(pdMS_TO_TICKS(800));
  }
}

/* ================= SENSOR TASK ================= */
void sensorTask(void *pvParameters) {
  int soilValue;
  while (1) {
    soilValue = 2000;  // REAL sensor now
    xQueueSend(soilQueue, &soilValue, portMAX_DELAY);

    Serial.print("[Sensor] Soil Value: ");
    Serial.println(soilValue);

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

/* ================= SERIAL MANUAL CONTROL TASK =================
 * Commands:
 * ON   -> Manual ON
 * OFF  -> Manual OFF
 * AUTO -> Back to AUTO
 ************************************************************/
void serialTask(void *pvParameters) {
  String cmdStr;
  PumpCommand cmd;

  while (1) {
    if (Serial.available()) {
      cmdStr = Serial.readStringUntil('\n');
      cmdStr.trim();
      cmdStr.toUpperCase();

      if (cmdStr == "ON") {
        cmd = CMD_PUMP_ON;
        Serial.println("[Serial] MANUAL ON");
        xQueueSend(commandQueue, &cmd, portMAX_DELAY);
      }
      else if (cmdStr == "OFF") {
        cmd = CMD_PUMP_OFF;
        Serial.println("[Serial] MANUAL OFF");
        xQueueSend(commandQueue, &cmd, portMAX_DELAY);
      }
      else if (cmdStr == "AUTO") {
        cmd = CMD_BACK_TO_AUTO;
        Serial.println("[Serial] BACK TO AUTO");
        xQueueSend(commandQueue, &cmd, portMAX_DELAY);
      }
      else {
        Serial.println("[Serial] Invalid command (ON / OFF / AUTO)");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

/* ================= CONTROL TASK ================= */
void controlTask(void *pvParameters) {
  int soilValue;
  PumpCommand cmd;
  ControlMode mode = MODE_AUTO;
  PumpState state = AUTO_OFF;

  while (1) {

    /* ---- Handle MANUAL commands ---- */
    if (xQueueReceive(commandQueue, &cmd, 0)) {

      if (cmd == CMD_BACK_TO_AUTO) {
        mode = MODE_AUTO;
        Serial.println("[Control] Mode → AUTO");
      } else {
        mode = MODE_MANUAL;
        state = (cmd == CMD_PUMP_ON) ? MANUAL_ON : MANUAL_OFF;
        xTaskNotify(relayTaskHandle, cmd, eSetValueWithOverwrite);
        Serial.println("[Control] Mode → MANUAL");
      }
    }

    /* ---- AUTO mode logic ---- */
    if (mode == MODE_AUTO) {
      if (xQueueReceive(soilQueue, &soilValue, portMAX_DELAY)) {

        if (soilValue < SOIL_DRY_THRESHOLD && state != AUTO_ON) {
          state = AUTO_ON;
          xTaskNotify(relayTaskHandle, CMD_PUMP_ON, eSetValueWithOverwrite);
          Serial.println("[Control] AUTO → Pump ON");
        }
        else if (soilValue >= SOIL_DRY_THRESHOLD && state != AUTO_OFF) {
          state = AUTO_OFF;
          xTaskNotify(relayTaskHandle, CMD_PUMP_OFF, eSetValueWithOverwrite);
          Serial.println("[Control] AUTO → Pump OFF");
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

/* ================= RELAY TASK ================= */
void relayTask(void *pvParameters) {
  uint32_t cmd;
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  while (1) {
    xTaskNotifyWait(0, 0, &cmd, portMAX_DELAY);
    digitalWrite(RELAY_PIN, (cmd == CMD_PUMP_ON));
    Serial.println(cmd == CMD_PUMP_ON ? "[Relay] Pump ON" : "[Relay] Pump OFF");
  }
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);
  delay(1000);

  soilQueue    = xQueueCreate(5, sizeof(int));
  commandQueue = xQueueCreate(5, sizeof(PumpCommand));

  if (!soilQueue || !commandQueue) {
    Serial.println("❌ Queue creation failed");
    while (1);
  }

  xTaskCreate(heartbeatTask, "Heartbeat", 1024, NULL, 0, NULL);
  xTaskCreate(sensorTask,    "Sensor",    2048, NULL, 1, NULL);
  xTaskCreate(serialTask,    "Serial",    2048, NULL, 1, NULL);
  xTaskCreate(controlTask,   "Control",   4096, NULL, 3, NULL);
  xTaskCreate(relayTask,     "Relay",     2048, NULL, 2, &relayTaskHandle);

  Serial.println("✅ Pump Controller Ready (AUTO + SERIAL MANUAL)");
}

/* ================= LOOP ================= */
void loop() {}
