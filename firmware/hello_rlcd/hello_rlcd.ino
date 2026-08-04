/*
 * hello_rlcd — Waveshare ESP32-S3-RLCD-4.2 smoke test
 *
 * Board: ESP32S3 Dev Module
 *   USB CDC On Boot: Enabled
 *   Flash Size: 16MB
 *   Partition: Huge APP (3MB)
 *   PSRAM: OPI PSRAM
 *
 * Display: ST7305 300x400 RLCD, SPI pins from Waveshare docs
 *   SCK=11 MOSI=12 DC=5 CS=40 RST=41
 *
 * Requires: U8g2 >= 2.36.19, arduino-esp32 >= 3.3.0
 */

#include <U8g2lib.h>
#include <SPI.h>

#define RLCD_SCK  11
#define RLCD_MOSI 12
#define RLCD_DC   5
#define RLCD_CS   40
#define RLCD_RST  41

// U8G2_R1 -> logical 400x300 landscape (same as ClaudeSlate / Waveshare examples)
U8G2_ST7305_300X400_F_4W_HW_SPI u8g2(U8G2_R1, RLCD_CS, RLCD_DC, RLCD_RST);

static void drawHello() {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);

  u8g2.drawFrame(4, 4, 392, 292);

  u8g2.setFont(u8g2_font_helvB12_tf);
  u8g2.drawStr(16, 28, "ESP32-S3-RLCD-4.2");

  u8g2.setFont(u8g2_font_logisoso32_tf);
  u8g2.drawStr(60, 120, "Hello, RLCD!");

  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.drawStr(16, 170, "Arduino + U8g2  |  ST7305 400x300");
  u8g2.drawStr(16, 190, "SCK11 MOSI12 DC5 CS40 RST41");
  u8g2.drawStr(16, 210, "Reflective LCD: needs ambient light");

  // Simple shapes to verify pixels
  u8g2.drawBox(16, 230, 40, 24);
  u8g2.drawFrame(70, 230, 40, 24);
  u8g2.drawCircle(150, 242, 14);
  u8g2.drawLine(180, 254, 260, 230);

  u8g2.setFont(u8g2_font_helvR08_tf);
  u8g2.drawStr(16, 280, "If you can read this, flash OK.");

  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("hello_rlcd: boot");

  // Route hardware SPI to board pins (MISO unused)
  SPI.begin(RLCD_SCK, -1, RLCD_MOSI, RLCD_CS);

  u8g2.begin();
  u8g2.setBusClock(24000000);
  Serial.println("hello_rlcd: display begin OK");

  drawHello();
  Serial.println("hello_rlcd: frame sent — check screen under room light");
}

void loop() {
  // Heartbeat for serial monitor only
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last >= 5000) {
    last = now;
    Serial.printf("hello_rlcd: alive %lu ms\n", (unsigned long)now);
  }
  delay(20);
}
