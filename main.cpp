#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <ArduinoJson.h>

const char* WIFI_SSID   = "Wokwi-GUEST";
const char* WIFI_PASS   = "";
const char* MQTT_SERVER = "broker.hivemq.com";
const int   MQTT_PORT   = 1883;

#define LED_PIN    4
#define BUZZER_PIN 5

struct SensorData {
  float accel_x, accel_y, accel_z;
  float total_accel;
  float pitch, roll;
};

struct AlertState {
  bool alertActive;
  bool manualMode;
};

QueueHandle_t dataQueue;
QueueHandle_t dataAlert;

Adafruit_MPU6050 mpu;
WiFiClient espClient;
PubSubClient client(espClient);

volatile bool manualLedState = false;
volatile bool manualBuzzerState = false;
volatile bool manualOverride = false;

void TaskDataPublish(void *pvParameters);
void TaskDataRead(void *pvParameters);
void TaskAlertControl(void *pvParameters);
void mqttCallback(char* topic, byte* payload, unsigned int length);

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(21, 22);
  if (!mpu.begin()) {
    Serial.println("Lỗi: Không tìm thấy cảm biến MPU6050!\r\n");
    while (1) { vTaskDelay(100 / portTICK_PERIOD_MS); }
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  dataQueue = xQueueCreate(10, sizeof(SensorData));
  dataAlert = xQueueCreate(10, sizeof(AlertState));

  if (dataQueue == NULL || dataAlert == NULL) {
    Serial.println("Lỗi tạo Queue!\r\n");
    return;
  }

  xTaskCreatePinnedToCore(TaskDataPublish, "TaskPublish", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(TaskDataRead, "TaskRead", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(TaskAlertControl, "TaskAlert", 2048, NULL, 2, NULL, 1);
}

void loop() {
  vTaskDelete(NULL);
}

void TaskDataPublish(void *pvParameters) {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(mqttCallback);

  SensorData data;

  for (;;) {
    if (!client.connected()) {
      String clientId = "ESP32_FallDetect_" + String(random(0xffff), HEX);
      if (client.connect(clientId.c_str())) {
        client.subscribe("falldetect/control/led");
        client.subscribe("falldetect/control/buzzer");
      } else {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        continue;
      }
    }
    client.loop();

    if (xQueueReceive(dataQueue, &data, portMAX_DELAY) == pdTRUE) {
      StaticJsonDocument<256> doc;
      doc["ax"] = data.accel_x;
      doc["ay"] = data.accel_y;
      doc["az"] = data.accel_z;
      doc["gia_toc"] = data.total_accel;
      doc["goc_pitch"] = data.pitch;
      doc["goc_roll"] = data.roll;

      char buffer[256];
      serializeJson(doc, buffer);
      client.publish("falldetect/telemetry", buffer);
    }
  }
}

void TaskDataRead(void *pvParameters) {
  sensors_event_t a, g, temp;
  SensorData sData;
  AlertState aState;

  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(200);

  for (;;) {
    mpu.getEvent(&a, &g, &temp);

    float ax = a.acceleration.x / 9.81;
    float ay = a.acceleration.y / 9.81;
    float az = a.acceleration.z / 9.81;
    float totalAccel = sqrt(ax * ax + ay * ay + az * az);

    float pitch = atan2(ay, sqrt(ax * ax + az * az)) * 180.0 / M_PI;
    float roll  = atan2(-ax, az) * 180.0 / M_PI;

    sData.accel_x = ax;
    sData.accel_y = ay;
    sData.accel_z = az;
    sData.total_accel = totalAccel;
    sData.pitch = pitch;
    sData.roll = roll;

    Serial.printf("[CAM_BIEN] Gia toc: %.2fg | Pitch: %.2f độ | Roll: %.2f độ\r\n", totalAccel, pitch, roll);

    bool isFallen = (totalAccel > 2.5 || abs(pitch) > 60.0 || abs(roll) > 60.0);

    aState.alertActive = isFallen;
    aState.manualMode = manualOverride;

    xQueueSend(dataQueue, &sData, 0);
    xQueueSend(dataAlert, &aState, 0);

    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void TaskAlertControl(void *pvParameters) {
  AlertState aState;

  for (;;) {
    if (xQueueReceive(dataAlert, &aState, portMAX_DELAY) == pdTRUE) {
      if (aState.manualMode) {
        digitalWrite(LED_PIN, manualLedState ? HIGH : LOW);
        digitalWrite(BUZZER_PIN, manualBuzzerState ? HIGH : LOW);
      } else {
        if (aState.alertActive) {
          digitalWrite(LED_PIN, HIGH);
          digitalWrite(BUZZER_PIN, HIGH);
        } else {
          digitalWrite(LED_PIN, LOW);
          digitalWrite(BUZZER_PIN, LOW);
        }
      }
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (String(topic) == "falldetect/control/led") {
    manualOverride = true;
    manualLedState = (message == "ON");
  } else if (String(topic) == "falldetect/control/buzzer") {
    manualOverride = true;
    manualBuzzerState = (message == "ON");
  }
}