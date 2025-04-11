#include "SmartCore_LED.h"

QueueHandle_t flashQueue;
SemaphoreHandle_t ledMutex = nullptr;
TaskHandle_t flashLEDTaskHandle = nullptr;
TaskHandle_t provisioningBlinkTaskHandle = nullptr;
volatile LEDMode currentLEDMode = LEDMODE_OFF;
DebugColor currentDebugColor = DEBUG_COLOR_YELLOW;
ProvisioningState currentProvisioningState = PROVISIONING_NONE;

void initSmartCoreLED() {
    pinMode(RGB_r, OUTPUT);
    pinMode(RGB_g, OUTPUT);
    pinMode(RGB_b, OUTPUT);

    ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_BIT);
    ledcSetup(LEDC_CHANNEL_1, LEDC_BASE_FREQ, LEDC_TIMER_BIT);
    ledcSetup(LEDC_CHANNEL_2, LEDC_BASE_FREQ, LEDC_TIMER_BIT);

    ledcAttachPin(RGB_r, LEDC_CHANNEL_0);
    ledcAttachPin(RGB_g, LEDC_CHANNEL_1);
    ledcAttachPin(RGB_b, LEDC_CHANNEL_2);

    setRGBColor(0, 255, 0);  // Green on init

    ledMutex = xSemaphoreCreateMutex();
    if (!ledMutex) Serial.println("⚠️ Failed to create LED mutex");

    flashQueue = xQueueCreate(5, sizeof(String));
    if (!flashQueue) Serial.println("⚠️ Failed to create flash queue");

    xTaskCreatePinnedToCore(flashLEDTask, "Flash LED", 2048, NULL, 1, &flashLEDTaskHandle, 1);
    xTaskCreatePinnedToCore(provisioningBlinkTask, "Provision Blink", 2048, NULL, 1, &provisioningBlinkTaskHandle, 1);
}

void setRGBColor(uint8_t red, uint8_t green, uint8_t blue) {
    float rAdj = 0.8;
    float gAdj = 1.0;
    float bAdj = 1.0;

    uint32_t duty_r = map(255 - (uint8_t)(red * rAdj), 0, 255, 0, (1 << LEDC_TIMER_BIT) - 1);
    uint32_t duty_g = map(255 - (uint8_t)(green * gAdj), 0, 255, 0, (1 << LEDC_TIMER_BIT) - 1);
    uint32_t duty_b = map(255 - (uint8_t)(blue * bAdj), 0, 255, 0, (1 << LEDC_TIMER_BIT) - 1);

    ledcWrite(LEDC_CHANNEL_0, duty_r);
    ledcWrite(LEDC_CHANNEL_1, duty_g);
    ledcWrite(LEDC_CHANNEL_2, duty_b);
}

void triggerFlashPattern(const String& pattern, uint32_t colorHex) {
    if (flashQueue) {
        static String safePattern;
        safePattern = pattern;
        xQueueSend(flashQueue, &safePattern, portMAX_DELAY);
    }
}

void flashLEDTask(void* parameter) {
    String pattern;
    while (true) {
        if (flashQueue && xQueueReceive(flashQueue, &pattern, portMAX_DELAY) == pdPASS) {
            Serial.printf("💡 Flashing pattern: %s\n", pattern.c_str());
            for (char c : pattern) {
                if (c == '.') {
                    setRGBColor(255, 255, 255);
                    delay(200);
                    setRGBColor(0, 0, 0);
                    delay(200);
                } else if (c == '-') {
                    setRGBColor(255, 255, 255);
                    delay(500);
                    setRGBColor(0, 0, 0);
                    delay(200);
                }
            }
        }
        delay(200);
    }
}

void provisioningBlinkTask(void* parameter) {
    while (true) {
        if (currentLEDMode == LEDMODE_WAITING) {
            setRGBColor(0, 255, 255);  // Cyan blink
            vTaskDelay(pdMS_TO_TICKS(200));
            setRGBColor(0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(800));
        } else {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

void turnOffRGBLEDs() {
    ledcDetachPin(RGB_r);
    ledcDetachPin(RGB_g);
    ledcDetachPin(RGB_b);

    pinMode(RGB_r, OUTPUT);
    pinMode(RGB_g, OUTPUT);
    pinMode(RGB_b, OUTPUT);

    digitalWrite(RGB_r, HIGH);
    digitalWrite(RGB_g, HIGH);
    digitalWrite(RGB_b, HIGH);
}

