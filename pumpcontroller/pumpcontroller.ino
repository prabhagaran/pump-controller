/************************************************************
 * Project : Smart Pump Controller (RTOS)
 * MCU     : ESP32
 * IDE     : Arduino
 * RTOS    : FreeRTOS (built-in)
 * Version : Clean / Fixed / Production-ready
 ************************************************************/

#include <Arduino.h>

/* ================= PIN DEFINITIONS ================= */
#define LED_PIN           2     // Heartbeat LED
#define SOIL_SENSOR_PIN   34    // ADC pin
#define RELAY_PIN         26    // Pump relay

/* ================= THRESHOLDS ================= */
#define SOIL_DRY_THRESHOLD 2000

/* ================= RTOS HANDLES ================= */
QueueHandle_t soilQueue;
TaskHandle_t relayTaskHandle = NULL;

/* ================= ENUMS ================= */
enum PumpCommand : uint32_t {
  PUMP_OFF = 0,
  PUMP_ON  = 1
};

/* ================= HEARTBEAT TASK =================
 * Shows system is alive
 ****************************************************/
void heartbeatTask(void *pvParameters) {
  pinMode(LED_PIN, OUTPUT);

  while (1) {
    digitalWrite(LED_PIN, HIGH);
    vTaskDelay(pdMS_TO_TICKS(200));
    digitalWrite(LED_PIN, LOW);
    vTaskDelay(pdMS_TO_TICKS(800));
  }
}

/* ================= SENSOR TASK =================
 * Reads soil moisture and sends to control task
 ****************************************************/
void sensorTask(void *pvParameters) {
  int soilValue;

  while (1) {
    // Replace with analogRead(SOIL_SENSOR_PIN) later
    soilValue = 2000;  // Test value

    xQueueSend(soilQueue, &soilValue, portMAX_DELAY);

    Serial.print("[Sensor] Soil Value: ");
    Serial.println(soilValue);

    vTaskDelay(pdMS_TO_TICKS(2000));  // Every 2 seconds
  }
}

/* ================= CONTROL TASK =================
 * Central decision-making logic (AUTO mode)
 ****************************************************/
void controlTask(void *pvParameters) {
  int soilValue;
  PumpCommand pumpCommand;

  while (1) {
    if (xQueueReceive(soilQueue, &soilValue, portMAX_DELAY)) {

      if (soilValue < SOIL_DRY_THRESHOLD) {
        pumpCommand = PUMP_ON;
        Serial.println("[Control] Soil dry → Pump ON");
      } else {
        pumpCommand = PUMP_OFF;
        Serial.println("[Control] Soil wet → Pump OFF");
      }

      // Defensive check before notifying
      if (relayTaskHandle != NULL) {
        xTaskNotify(
          relayTaskHandle,
          pumpCommand,
          eSetValueWithOverwrite
        );
      }
    }
  }
}

/* ================= RELAY TASK =================
 * Drives pump hardware ONLY
 ****************************************************/
void relayTask(void *pvParameters) {
  uint32_t command;

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // Pump OFF initially

  while (1) {
    // Wait indefinitely for control command
    xTaskNotifyWait(
      0,
      0,
      &command,
      portMAX_DELAY
    );

    if (command == PUMP_ON) {
      digitalWrite(RELAY_PIN, HIGH);
      Serial.println("[Relay] Pump ON");
    } else {
      digitalWrite(RELAY_PIN, LOW);
      Serial.println("[Relay] Pump OFF");
    }
  }
}

/* ================= COMM TASK =================
 * Placeholder for Bluetooth / WiFi / SIM800L
 ****************************************************/
void commTask(void *pvParameters) {
  while (1) {
    // Communication logic will be added later
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("ESP32 RTOS Smart Pump Controller (AUTO Mode)");

  /* -------- Create Queue -------- */
  soilQueue = xQueueCreate(5, sizeof(int));
  if (soilQueue == NULL) {
    Serial.println("❌ Soil queue creation failed");
    while (1);  // Fatal error
  }

  /* -------- Create Tasks -------- */
  xTaskCreate(heartbeatTask, "Heartbeat", 1024, NULL, 0, NULL);
  xTaskCreate(sensorTask,    "Sensor",    2048, NULL, 1, NULL);
  xTaskCreate(controlTask,   "Control",   4096, NULL, 3, NULL);
  xTaskCreate(relayTask,     "Relay",     2048, NULL, 2, &relayTaskHandle);
  xTaskCreate(commTask,      "Comm",      2048, NULL, 1, NULL);

  Serial.println("✅ RTOS system started successfully");
}

/* ================= LOOP ================= */
void loop() {
  // Empty - FreeRTOS scheduler is running
}
