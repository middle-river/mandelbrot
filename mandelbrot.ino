// Mandelbroat Viewer with ESP32
// 2021-12-12  T. Nakagawa

#include <SPI.h>
#include "LCDClass.h"

constexpr int16_t PIN_LCD_SCL = 18;
constexpr int16_t PIN_LCD_SDA = 23;
constexpr int16_t PIN_LCD_RES = 22 ;
constexpr int16_t PIN_LCD_DC = 21;
constexpr int16_t PIN_RE_A = 17;
constexpr int16_t PIN_RE_B = 16;
constexpr int16_t PIN_RE_S = 4;

SPIClass SPI2(VSPI);
LCDClass LCD(PIN_LCD_DC, PIN_LCD_RES, &SPI2);

constexpr int PALETTE[16] = {0x0000, 0x0f00, 0xe003, 0x0078, 0xef7b, 0xef03, 0x0f78, 0xe07b, 0x1f00, 0xe007, 0x00f8, 0xf7bd, 0xff07, 0x1ff8, 0xe0ff, 0xffff};
constexpr int SIZE = 240;	// Screen size;
constexpr int ITER = 128;	// Iteration of the inner loop.
float cx = 0.0f;		// Center x.
float cy = 0.0f;		// Center y.
float sc = 4.0f / SIZE;		// Scale.
int line = 0;			// The line to be drawn next.
SemaphoreHandle_t mutex_var;
SemaphoreHandle_t mutex_lcd;
uint32_t time_bgn, time_end;

void IRAM_ATTR key_handler() {
  static int key_a0 = 1;
  static int key_b0 = 1;
  static int key_s0 = 1;
  static bool key_moved = false;
  static bool key_axis = false;
  const int key_a = digitalRead(PIN_RE_A);
  const int key_b = digitalRead(PIN_RE_B);
  const int key_s = digitalRead(PIN_RE_S);

  if (key_s == 1 && key_s0 == 0) {
    if (!key_moved) key_axis = !key_axis;
    key_moved = false;
  }
  if (key_a == 0 && key_b == 0 && !(key_a0 == 0 && key_b0 == 0)) {
    const int delta = (key_b0 == 1) ? -1 : 1;
    if (key_s == 1) {
      if (key_axis) cy += delta * sc * 4.0f; else cx += delta * sc * 4.0f;
    } else {
      if (delta > 0) sc *= 0.5f; else sc *= 2.0f;
    }
    line = 0;
  }

  key_a0 = key_a;
  key_b0 = key_b;
  key_s0 = key_s;
}

void drawTask(void *pvParameters) {
  uint16_t buf[SIZE];
  while (true) {
    int i = -1;
    xSemaphoreTake(mutex_var, 100);
    if (line < SIZE) i = line++;
    xSemaphoreGive(mutex_var);
    if (i < 0) continue;
    if (i == 0) time_bgn = millis();

    const float y = cy + sc * (i - SIZE / 2);
    float x = cx - sc * (SIZE / 2);
    for (int j = 0; j < SIZE; j++, x += sc) {
      float a = 0.0f;
      float b = 0.0f;
      int k = 0;
      do {
        const float tmp = a * a - b * b + x;
        b = 2.0f * a * b + y;
        a = tmp;
      } while (a * a + b * b < 4.0f && ++k < ITER - 1);
      buf[j] = PALETTE[(k == ITER - 1) ? 0 : (k % 15 + 1)];
    }
    xSemaphoreTake(mutex_lcd, 100);
    LCD.draw(0, i, SIZE, 1, (uint8_t *)buf);
    xSemaphoreGive(mutex_lcd);

    if (i == SIZE - 1) {
      time_end = millis();
      Serial.print("Time: ");
      Serial.println(time_end - time_bgn);
    }
  }
}

void setup() {
  pinMode(PIN_RE_A, INPUT_PULLUP);
  pinMode(PIN_RE_B, INPUT_PULLUP);
  pinMode(PIN_RE_S, INPUT_PULLUP);
  Serial.begin(115200);

  Serial.println("Mandelbroat Viewer");
  LCD.begin();

  // 1kHz interruption for key input.
  hw_timer_t *timer = timerBegin(0, getApbFrequency() / 1000000, true);
  timerAttachInterrupt(timer, key_handler, true);
  timerAlarmWrite(timer, 1000, true);
  timerAlarmEnable(timer);

  mutex_var = xSemaphoreCreateMutex();
  mutex_lcd = xSemaphoreCreateMutex();
  disableCore0WDT();
  disableCore1WDT();
  xTaskCreatePinnedToCore(drawTask, "drawTask0", 8192, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(drawTask, "drawTask1", 8192, NULL, 2, NULL, 1);
}

void loop() {
}
