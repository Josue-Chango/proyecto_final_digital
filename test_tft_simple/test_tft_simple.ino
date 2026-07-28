#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#define TFT_CS 10
#define TFT_DC  9
#define TFT_RST 8

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

#define BLACK   ILI9341_BLACK
#define WHITE   ILI9341_WHITE
#define RED     ILI9341_RED
#define GREEN   ILI9341_GREEN
#define BLUE    ILI9341_BLUE
#define CYAN    ILI9341_CYAN
#define YELLOW  ILI9341_YELLOW
#define MAGENTA ILI9341_MAGENTA

uint16_t colors[] = {RED, GREEN, BLUE, CYAN, YELLOW, MAGENTA, WHITE};
int numColors = 7;
int colorIdx = 0;

void setup() {
  Serial.begin(9600);
  Serial.println("=== TFT TEST Adafruit ===");

  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(BLACK);

  Serial.println("TFT init OK");
}

void loop() {
  uint16_t c = colors[colorIdx];

  tft.fillScreen(c);
  delay(800);

  tft.fillScreen(BLACK);

  tft.setTextSize(3);
  tft.setTextColor(c);
  tft.setCursor(30, 40);
  tft.print("Hola!");

  tft.setTextSize(2);
  tft.setTextColor(WHITE);
  tft.setCursor(20, 100);
  tft.print("TFT ILI9341");

  tft.setTextSize(1);
  tft.setTextColor(c);
  tft.setCursor(50, 140);
  tft.print("SimulIDE Test");

  delay(1500);

  colorIdx = (colorIdx + 1) % numColors;
}
