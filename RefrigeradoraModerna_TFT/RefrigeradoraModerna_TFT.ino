// ============================================================
//  RefrigeradoraModerna TFT v5.0 — Arduino Mega 2560
//  Display: ILI9341 TFT 240x320 via SPI (Adafruit)
//  Sin parpadeo: actualización parcial de pantalla
//  PINES:
//    TFT_SCK  → 52    TFT_MOSI → 51  (SPI Mega)
//    TFT_CS   → 10    TFT_DC   → 9     TFT_RST → 8
//    TempPot  → A0    DoorSW   → A1 (pull-up)
//    SetPot   → A2    HumPot   → A3
//    Relay    → 30    Buzzer   → 31
//    Luz      → 32    CompLED  → 33   FreezeLED → 34
//    BTN_UP   → 40    BTN_DW   → 41
//    BTN_OK   → 42    BTN_BK   → 43
//
//  LIBRERÍAS: Adafruit_GFX, Adafruit_ILI9341, SPI
// ============================================================

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#define TFT_CS   10
#define TFT_DC    9
#define TFT_RST   8
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// ─── Colores RGB565 ────────────────────────────────────────
#define BLACK    0x0000
#define WHITE    0xFFFF
#define RED      0xF800
#define GREEN    0x07E0
#define BLUE     0x001F
#define CYAN     0x07FF
#define YELLOW   0xFFE0
#define ORANGE   0xFD20
#define MAGENTA  0xF81F
#define DARKGREY 0x4208
#define LGREY    0xC618
#define DBLUE    0x0811
#define LBLUE    0x5D9C
#define SKYBLUE  0x867F
#define GROUND   0x5460
#define GROUND2  0x6B8D
#define SAND     0xD6B0

// ─── Pines ─────────────────────────────────────────────────
const int TEMP_POT_PIN = A0, DOOR_SENSOR = A1;
const int SETPOINT_PIN = A2, HUMIDITY_PIN = A3;
const int RELAY_PIN = 30, BUZZER_PIN = 31;
const int LIGHT_PIN = 32, COMP_LED = 33, FREEZE_LED = 34;
const int BTN_UP = 40, BTN_DOWN = 41, BTN_OK = 42, BTN_BACK = 43;

// ─── Parámetros ────────────────────────────────────────────
const float HISTERESIS = 0.5;
const unsigned long DEB_MS = 180, LOG_MS = 1000, DOOR_WARN_MS = 10000;

// ─── Estado ────────────────────────────────────────────────
float  targetTemp = 5.0, currentTemp = 0.0, humidity = 50.0;
bool   doorOpen = false, cooling = false, freezeMode = false;

// ─── Reloj ─────────────────────────────────────────────────
uint8_t clkH = 12, clkM = 0, clkS = 0;
unsigned long lastClockTick = 0;

// ─── Temporizador ──────────────────────────────────────────
uint16_t timerSet = 300, timerLeft = 0;
bool timerOn = false, timerDone = false;
unsigned long lastTimerTick = 0;

// ─── Menú ──────────────────────────────────────────────────
uint8_t menuPage = 0;
bool    inSub = false;
uint8_t editStep = 0;

// ─── Otros ─────────────────────────────────────────────────
unsigned long lastBtn[4] = {0, 0, 0, 0};
unsigned long doorOpenSince = 0, lastDoorBeep = 0, lastSerial = 0;
bool buzzerDoor = false, buzzerTimer = false;

// ─── Estado previo para actualizar sin parpadeo ────────────
uint8_t  prevClkH = 255, prevClkM = 255, prevClkS = 255;
float    prevTemp = -999, prevTarget = -999, prevHumidity = -99;
bool     prevCooling = false, prevFreeze = false, prevDoor = false;
uint16_t prevTimerLeft = 0xFFFF;
bool     prevTimerOn = false, prevTimerDone = false;
uint8_t  prevMenuPage = 255;
int      prevBarTemp = -1, prevBarHum = -1;
bool     prevBlink = false;

// ─── Botones ───────────────────────────────────────────────
bool btnPushed(int pin, int idx) {
  if (digitalRead(pin) == LOW && (millis() - lastBtn[idx] > DEB_MS)) {
    lastBtn[idx] = millis();
    return true;
  }
  return false;
}

// ─── Helpers ───────────────────────────────────────────────
void tftCenter(const char* text, int y, int color, int sz) {
  tft.setTextSize(sz);
  tft.setTextColor(color);
  int w = strlen(text) * 6 * sz;
  tft.setCursor((240 - w) / 2, y);
  tft.print(text);
}

void tftRect(int x, int y, int w, int h, int col) {
  tft.fillRoundRect(x, y, w, h, 4, col);
}

// ─── Paisaje de fondo ──────────────────────────────────────
void drawBackground() {
  // Cielo
  tft.fillRect(0, 0, 240, 180, LBLUE);

  // Sol
  tft.fillCircle(210, 40, 28, YELLOW);
  tft.fillCircle(210, 40, 20, 0xFE60);

  // Nubes
  tft.fillCircle(50, 35, 14, WHITE);
  tft.fillCircle(70, 30, 18, WHITE);
  tft.fillCircle(90, 35, 14, WHITE);
  tft.fillCircle(160, 55, 10, WHITE);
  tft.fillCircle(175, 50, 14, WHITE);
  tft.fillCircle(190, 55, 10, WHITE);

  // Montañas lejanas
  for (int i = 0; i < 240; i++) {
    int h = 155 + (int)(15.0 * sin(i * 0.025)) + (int)(8.0 * sin(i * 0.06));
    tft.drawFastVLine(i, h, 25, 0x39E0);
  }

  // Suelo
  tft.fillRect(0, 175, 240, 5, GROUND);
  tft.fillRect(0, 180, 240, 140, GROUND2);

  // Camino / baldosa
  tft.fillRect(0, 230, 240, 3, SAND);
  tft.fillRect(0, 270, 240, 3, SAND);

  // Césped decorativo
  for (int i = 0; i < 240; i += 12) {
    tft.fillTriangle(i, 175, i + 4, 162, i + 8, 175, GREEN);
  }
}

// ─── Reloj y temporizador ──────────────────────────────────
void tickClock() {
  if (millis() - lastClockTick >= 1000) {
    lastClockTick = millis();
    if (++clkS >= 60) {
      clkS = 0;
      if (++clkM >= 60) { clkM = 0; if (++clkH >= 24) clkH = 0; }
    }
  }
}

void tickTimer() {
  if (!timerOn || timerLeft == 0) return;
  if (millis() - lastTimerTick >= 1000) {
    lastTimerTick = millis();
    if (--timerLeft == 0) { timerOn = false; timerDone = true; }
  }
}

// ═══════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════
void setup() {
  Serial.begin(9600);
  Serial.println(F("=== RefrigeradoraModerna TFT v5.0 ==="));

  tft.begin();
  tft.setRotation(1);

  // Splash
  tft.fillScreen(BLACK);
  tftRect(20, 30, 200, 50, BLUE);
  tftCenter("REFRIGERADORA", 42, WHITE, 2);
  tftRect(60, 95, 120, 30, DARKGREY);
  tftCenter("Moderna TFT", 102, CYAN, 1);
  tftCenter("v5.0", 140, YELLOW, 2);
  delay(1400);
  tft.fillScreen(BLACK);
  tftCenter("Arduino Mega", 50, GREEN, 2);
  tftCenter("2560", 80, WHITE, 3);
  tftCenter("ILI9341 TFT", 120, CYAN, 2);
  tftCenter("Ready!", 160, YELLOW, 2);
  delay(1200);

  pinMode(RELAY_PIN,  OUTPUT); digitalWrite(RELAY_PIN, LOW);
  pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);
  pinMode(LIGHT_PIN,  OUTPUT); digitalWrite(LIGHT_PIN, LOW);
  pinMode(COMP_LED,   OUTPUT); digitalWrite(COMP_LED, LOW);
  pinMode(FREEZE_LED, OUTPUT); digitalWrite(FREEZE_LED, LOW);
  pinMode(DOOR_SENSOR, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP); pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP); pinMode(BTN_BACK, INPUT_PULLUP);

  drawBackground();
  prevMenuPage = 255;
}

// ═══════════════════════════════════════════════════════════
//  PANTALLA TEMPERATURA — actualización parcial
// ═══════════════════════════════════════════════════════════
void enterTemp() {
  tft.fillRect(0, 0, 240, 28, freezeMode ? BLUE : DBLUE);
  tft.setTextSize(1);
  tft.setTextColor(WHITE);
  tftCenter(freezeMode ? "MODO CONGELADOR" : "REFRIGERADOR", 10, WHITE, 1);
  prevTemp = -999; prevTarget = -999;
  prevCooling = !cooling; prevFreeze = !freezeMode;
  prevDoor = !doorOpen; prevBarTemp = -1;
}

void updateTemp() {
  // Título solo si cambia modo
  if (freezeMode != prevFreeze) {
    tft.fillRect(0, 0, 240, 28, freezeMode ? BLUE : DBLUE);
    tftCenter(freezeMode ? "MODO CONGELADOR" : "REFRIGERADOR", 10, WHITE, 1);
    prevFreeze = freezeMode;
  }

  // Temperatura — solo si cambió
  if (currentTemp != prevTemp) {
    char ts[8];
    dtostrf(currentTemp, 4, 1, ts);
    int tw = (strlen(ts) + 2) * 30;
    int x = (240 - tw) / 2;
    tft.fillRect(x, 45, tw + 10, 40, BLACK);
    tft.setTextSize(5);
    tft.setTextColor(cooling ? RED : GREEN);
    tft.setCursor(x + 5, 50);
    tft.print("T:"); tft.print(ts); tft.print("C");
    prevTemp = currentTemp;
  }

  // Compresor — solo si cambió
  if (cooling != prevCooling) {
    tftRect(30, 100, 80, 24, cooling ? RED : DARKGREY);
    tft.setTextSize(1);
    tft.setTextColor(WHITE);
    tft.setCursor(40, 108);
    tft.print("COMP: "); tft.print(cooling ? "ON" : "OFF");
    prevCooling = cooling;
  }

  // Modo — solo si cambió
  if (freezeMode != prevFreeze) {
    tftRect(130, 100, 80, 24, freezeMode ? BLUE : GREEN);
    tft.setTextSize(1);
    tft.setTextColor(WHITE);
    tft.setCursor(140, 108);
    tft.print(freezeMode ? "CONGEL" : "FRIDGE");
  }

  // Setpoint — solo si cambió
  if (targetTemp != prevTarget) {
    tft.fillRect(20, 135, 120, 20, BLACK);
    tft.setTextSize(2);
    tft.setTextColor(CYAN);
    tft.setCursor(20, 137);
    tft.print("SP:"); tft.print(targetTemp, 1); tft.print("C");
    prevTarget = targetTemp;
  }

  // Estado — solo si cambió
  if (cooling != prevCooling) {
    tft.fillRect(20, 165, 100, 14, BLACK);
    tft.setTextSize(1);
    tft.setTextColor(cooling ? RED : GREEN);
    tft.setCursor(20, 167);
    tft.print(cooling ? "[ENFRIANDO]" : "[TEMP OK]");
  }

  // Alarma puerta — solo si cambió
  if (doorOpen != prevDoor) {
    if (doorOpen) {
      tftRect(0, 185, 240, 24, RED);
      tftCenter("! PUERTA ABIERTA !", 192, WHITE, 1);
    } else {
      tft.fillRect(0, 185, 240, 24, BLACK);
      drawBackground();
    }
    prevDoor = doorOpen;
  }

  // Barra inferior — solo si cambia estado
  if (cooling != prevCooling || freezeMode != prevFreeze || doorOpen != prevDoor) {
    tftRect(0, 218, 80, 18, freezeMode ? BLUE : DARKGREY);
    tft.setTextSize(1); tft.setTextColor(WHITE);
    tft.setCursor(10, 224); tft.print("FREEZE");
    tftRect(80, 218, 80, 18, cooling ? RED : DARKGREY);
    tft.setCursor(90, 224); tft.print("COMP");
    tftRect(160, 218, 80, 18, doorOpen ? ORANGE : DARKGREY);
    tft.setCursor(170, 224); tft.print("PUERTA");
    prevDoor = doorOpen;
  }
}

// ═══════════════════════════════════════════════════════════
//  PANTALLA RELOJ — solo cambian los dígitos que cambian
// ═══════════════════════════════════════════════════════════
void enterClock() {
  tft.fillRect(0, 0, 240, 28, DARKGREY);
  tftCenter("RELOJ", 10, WHITE, 1);
  prevClkH = 255; prevClkM = 255; prevClkS = 255;
  prevBlink = !prevBlink;

  // Líneas decorativas
  tft.fillRect(30, 105, 180, 2, CYAN);
  tft.fillRect(30, 155, 180, 2, CYAN);
}

void updateClock() {
  // Posiciones x para cada dígito (textSize=4, cada char=24px)
  // "HH:MM:SS" = 8 chars * 24 = 192px, startX = 24
  int startX = 24;
  int y = 45;

  // Horas — solo si cambiaron
  if (clkH != prevClkH) {
    char buf[3]; sprintf(buf, "%02d", clkH);
    tft.setTextSize(4); tft.setTextColor(WHITE);
    tft.fillRect(startX, y, 48, 32, BLACK);
    tft.setCursor(startX, y); tft.print(buf);
    prevClkH = clkH;
  }

  // Dos puntos horas (siempre)
  if (prevClkH == 255) {
    tft.setTextSize(4); tft.setTextColor(WHITE);
    tft.setCursor(startX + 48, y); tft.print(":");
  }

  // Minutos — solo si cambiaron
  if (clkM != prevClkM) {
    char buf[3]; sprintf(buf, "%02d", clkM);
    tft.setTextSize(4); tft.setTextColor(WHITE);
    tft.fillRect(startX + 72, y, 48, 32, BLACK);
    tft.setCursor(startX + 72, y); tft.print(buf);
    prevClkM = clkM;
  }

  // Dos puntos minutos (siempre)
  if (prevClkM == 255) {
    tft.setTextSize(4); tft.setTextColor(WHITE);
    tft.setCursor(startX + 120, y); tft.print(":");
  }

  // Segundos — solo si cambiaron
  if (clkS != prevClkS) {
    char buf[3]; sprintf(buf, "%02d", clkS);
    tft.setTextSize(4); tft.setTextColor(WHITE);
    tft.fillRect(startX + 144, y, 48, 32, BLACK);
    tft.setCursor(startX + 144, y); tft.print(buf);
    prevClkS = clkS;
  }

  // Indicador parpadeo — solo cambia cada segundo
  bool blinkState = (clkS % 2 == 0);
  if (blinkState != prevBlink) {
    tft.fillCircle(120, 130, 5, blinkState ? GREEN : DARKGREY);
    prevBlink = blinkState;
  }
}

// ═══════════════════════════════════════════════════════════
//  PANTALLA TEMPORIZADOR
// ═══════════════════════════════════════════════════════════
void enterTimer() {
  tft.fillRect(0, 0, 240, 28, DARKGREY);
  tftCenter("TEMPORIZADOR", 10, WHITE, 1);
  prevTimerLeft = 0xFFFF; prevTimerOn = !timerOn; prevTimerDone = !timerDone;
  prevBarTemp = -1;
}

void updateTimer() {
  uint16_t m = timerLeft / 60, s = timerLeft % 60;
  char ts[10]; sprintf(ts, "%02d:%02d", m, s);

  if (timerDone) {
    if (prevTimerDone != timerDone) {
      tft.fillRect(0, 30, 240, 200, BLACK);
      tftCenter("** LISTO! **", 50, YELLOW, 3);
      tftCenter("Tiempo completado", 100, WHITE, 1);
      tftRect(40, 130, 160, 28, GREEN);
      tftCenter("Presione OK para reset", 138, WHITE, 1);
      prevTimerDone = timerDone;
    }
  } else if (timerOn) {
    // Actualizar tiempo solo si cambió
    if (timerLeft != prevTimerLeft) {
      tft.setTextSize(4);
      tft.setTextColor(GREEN);
      int tw = 8 * 24;
      int x = (240 - tw) / 2;
      tft.fillRect(x, 45, tw + 10, 35, BLACK);
      tft.setCursor(x + 5, 50); tft.print(ts);
      prevTimerLeft = timerLeft;

      // "Corriendo..." solo la primera vez
      if (prevTimerOn != timerOn) {
        tftCenter("Corriendo...", 100, GREEN, 2);
      }

      // Barra de progreso
      int total = timerSet;
      int elapsed = total - timerLeft;
      int barW = (total > 0) ? map(elapsed, 0, total, 0, 200) : 0;
      if (barW != prevBarTemp) {
        tftRect(20, 145, 200, 14, DARKGREY);
        if (barW > 0) tftRect(20, 145, barW, 14, GREEN);
        prevBarTemp = barW;
      }
    }
    prevTimerOn = timerOn;
  } else {
    if (timerLeft != prevTimerLeft || prevTimerOn != timerOn) {
      tft.fillRect(0, 30, 240, 200, BLACK);
      tft.setTextSize(4); tft.setTextColor(CYAN);
      int tw = 8 * 24;
      int x = (240 - tw) / 2;
      tft.setCursor(x + 5, 50); tft.print(ts);
      tftCenter("Presione OK para iniciar", 110, LGREY, 1);
      prevTimerLeft = timerLeft; prevTimerOn = timerOn;
    }
  }
}

// ═══════════════════════════════════════════════════════════
//  PANTALLA HUMEDAD
// ═══════════════════════════════════════════════════════════
void enterHumidity() {
  tft.fillRect(0, 0, 240, 28, DARKGREY);
  tftCenter("HUMEDAD Y ESTADO", 10, WHITE, 1);
  prevHumidity = -99; prevBarHum = -1;
  prevCooling = !cooling; prevDoor = !doorOpen;
  prevTimerOn = !timerOn;
}

void updateHumidity() {
  // Humedad — solo si cambió
  if (humidity != prevHumidity) {
    char hs[8]; sprintf(hs, "%d%%", (int)humidity);
    int hw = strlen(hs) * 24;
    int x = (240 - hw) / 2;
    tft.fillRect(x - 5, 40, hw + 20, 35, BLACK);
    tft.setTextSize(4); tft.setTextColor(CYAN);
    tft.setCursor(x, 45); tft.print(hs);
    prevHumidity = humidity;

    // Barra de humedad
    int barW = map((int)humidity, 0, 100, 0, 200);
    if (barW != prevBarHum) {
      tftRect(20, 85, 200, 12, DARKGREY);
      if (barW > 0) tftRect(20, 85, barW, 12, CYAN);
      prevBarHum = barW;
    }
  }

  // Modo — solo si cambió
  if (freezeMode != prevFreeze) {
    tftRect(20, 110, 200, 22, freezeMode ? BLUE : GREEN);
    tft.setTextSize(1); tft.setTextColor(WHITE);
    tft.setCursor(50, 117);
    tft.print(freezeMode ? "MODO: CONGELADOR" : "MODO: REFRIGERADOR");
    prevFreeze = freezeMode;
  }

  // Compresor — solo si cambió
  if (cooling != prevCooling) {
    tftRect(20, 140, 95, 22, cooling ? RED : DARKGREY);
    tft.setTextSize(1); tft.setTextColor(WHITE);
    tft.setCursor(30, 147);
    tft.print(cooling ? "COMP: ON" : "COMP: OFF");
    prevCooling = cooling;
  }

  // Puerta — solo si cambió
  if (doorOpen != prevDoor) {
    tftRect(125, 140, 95, 22, doorOpen ? ORANGE : GREEN);
    tft.setTextSize(1); tft.setTextColor(WHITE);
    tft.setCursor(135, 147);
    tft.print(doorOpen ? "PTA:ABT" : "PTA:CER");
    prevDoor = doorOpen;
  }

  // Timer — solo si cambió
  if (timerOn != prevTimerOn) {
    tftRect(20, 172, 95, 22, timerOn ? YELLOW : DARKGREY);
    tft.setTextSize(1);
    tft.setTextColor(timerOn ? BLACK : WHITE);
    tft.setCursor(30, 179);
    tft.print(timerOn ? "TIMER: ON" : "TIMER: OFF");
    prevTimerOn = timerOn;
  }
}

// ═══════════════════════════════════════════════════════════
//  SUB-MENÚS
// ═══════════════════════════════════════════════════════════
void menuSetpoint() {
  if (prevMenuPage != 6) {
    tft.fillRect(0, 0, 240, 240, BLACK);
    tftRect(0, 0, 240, 28, YELLOW);
    tftCenter("AJUSTAR SETPOINT", 10, BLACK, 1);
    tftCenter("UP/DOWN: Ajustar", 130, LGREY, 1);
    tftCenter("OK/BACK: Salir", 150, LGREY, 1);
    prevTarget = -999; prevMenuPage = 6;
  }
  if (targetTemp != prevTarget) {
    char ts[8]; dtostrf(targetTemp, 4, 1, ts);
    int tw = (strlen(ts) + 2) * 24;
    int x = (240 - tw) / 2;
    tft.fillRect(x, 60, tw + 10, 35, BLACK);
    tft.setTextSize(4); tft.setTextColor(CYAN);
    tft.setCursor(x + 5, 65);
    tft.print("T:"); tft.print(ts); tft.print("C");
    prevTarget = targetTemp;
  }
  if (btnPushed(BTN_UP, 0)) { targetTemp = min(15.0f, targetTemp + 0.5f); }
  if (btnPushed(BTN_DOWN, 1)) { targetTemp = max(-10.0f, targetTemp - 0.5f); }
  if (btnPushed(BTN_OK, 2) || btnPushed(BTN_BACK, 3)) {
    inSub = false; menuPage = 0; prevMenuPage = 255; enterTemp();
  }
}

void menuSetClock() {
  if (prevMenuPage != 7) {
    tft.fillRect(0, 0, 240, 240, BLACK);
    tftRect(0, 0, 240, 28, YELLOW);
    tftCenter(editStep == 0 ? "Ajustar HORA" : "Ajustar MINUTO", 10, BLACK, 1);
    tftCenter("UP/DOWN: Ajustar", 130, LGREY, 1);
    tftCenter("OK: Siguiente  BACK: Salir", 150, LGREY, 1);
    prevClkH = 255; prevClkM = 255; prevMenuPage = 7;
  }
  // Actualizar solo dígitos que cambian
  int startX = 24;
  if (clkH != prevClkH) {
    char buf[3]; sprintf(buf, "%02d", clkH);
    tft.setTextSize(4); tft.setTextColor(WHITE);
    tft.fillRect(startX, 60, 48, 32, BLACK);
    tft.setCursor(startX, 60); tft.print(buf);
    prevClkH = clkH;
  }
  tft.setTextSize(4); tft.setTextColor(WHITE);
  tft.setCursor(startX + 48, 60); tft.print(":");
  if (clkM != prevClkM) {
    char buf[3]; sprintf(buf, "%02d", clkM);
    tft.fillRect(startX + 72, 60, 48, 32, BLACK);
    tft.setCursor(startX + 72, 60); tft.print(buf);
    prevClkM = clkM;
  }

  if (btnPushed(BTN_UP, 0)) {
    if (editStep == 0) clkH = (clkH + 1) % 24;
    else clkM = (clkM + 1) % 60;
    clkS = 0;
  }
  if (btnPushed(BTN_DOWN, 1)) {
    if (editStep == 0) clkH = (clkH + 23) % 24;
    else clkM = (clkM + 59) % 60;
  }
  if (btnPushed(BTN_OK, 2)) {
    if (editStep == 0) {
      editStep = 1;
      tft.fillRect(0, 0, 240, 28, YELLOW);
      tftCenter("Ajustar MINUTO", 10, BLACK, 1);
    } else {
      editStep = 0; inSub = false; menuPage = 1; prevMenuPage = 255; enterClock();
    }
  }
  if (btnPushed(BTN_BACK, 3)) {
    editStep = 0; inSub = false; menuPage = 1; prevMenuPage = 255; enterClock();
  }
}

void menuSetTimer() {
  if (prevMenuPage != 8) {
    tft.fillRect(0, 0, 240, 240, BLACK);
    tftRect(0, 0, 240, 28, YELLOW);
    tftCenter("AJUSTAR TIMER", 10, BLACK, 1);
    tftCenter("UP/DOWN: Ajustar (1 min)", 130, LGREY, 1);
    tftCenter("OK: Iniciar  BACK: Salir", 150, LGREY, 1);
    prevTimerLeft = 0xFFFF; prevMenuPage = 8;
  }
  uint16_t m = timerSet / 60, s = timerSet % 60;
  char ts[10]; sprintf(ts, "%02d:%02d", m, s);
  tft.setTextSize(4); tft.setTextColor(CYAN);
  int tw = 8 * 24;
  int x = (240 - tw) / 2;
  tft.fillRect(x, 60, tw + 10, 35, BLACK);
  tft.setCursor(x + 5, 65); tft.print(ts);
  prevTimerLeft = timerSet;

  if (btnPushed(BTN_UP, 0)) {
    timerSet = min((uint16_t)5940, (uint16_t)(timerSet + 60));
  }
  if (btnPushed(BTN_DOWN, 1)) {
    if (timerSet >= 60) timerSet -= 60;
  }
  if (btnPushed(BTN_OK, 2)) {
    timerLeft = timerSet; timerOn = (timerLeft > 0);
    timerDone = false; lastTimerTick = millis();
    inSub = false; menuPage = 2; prevMenuPage = 255; enterTimer();
  }
  if (btnPushed(BTN_BACK, 3)) {
    inSub = false; menuPage = 2; prevMenuPage = 255; enterTimer();
  }
}

// ═══════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════
void loop() {
  unsigned long now = millis();

  // ── Leer sensores ──────────────────────────────────────
  currentTemp = -10.0f + (analogRead(TEMP_POT_PIN) / 1023.0f) * 50.0f;
  humidity    = (analogRead(HUMIDITY_PIN) / 1023.0f) * 100.0f;
  doorOpen    = (digitalRead(DOOR_SENSOR) == LOW);
  freezeMode  = (targetTemp <= 0.0f);

  if (!(inSub && menuPage == 4)) {
    targetTemp = -10.0f + (analogRead(SETPOINT_PIN) / 1023.0f) * 25.0f;
  }

  // ── Control compresor ───────────────────────────────────
  if (currentTemp > targetTemp + HISTERESIS) {
    cooling = true;  digitalWrite(RELAY_PIN, HIGH); digitalWrite(COMP_LED, HIGH);
  } else if (currentTemp < targetTemp - HISTERESIS) {
    cooling = false; digitalWrite(RELAY_PIN, LOW);  digitalWrite(COMP_LED, LOW);
  }
  digitalWrite(FREEZE_LED, freezeMode ? HIGH : LOW);
  digitalWrite(LIGHT_PIN, doorOpen ? HIGH : LOW);

  // ── Buzzer ─────────────────────────────────────────────
  buzzerTimer = (timerDone && (now % 3000 < 400));
  buzzerDoor = false;
  if (doorOpen) {
    if (doorOpenSince == 0) doorOpenSince = now;
    unsigned long t = now - doorOpenSince;
    unsigned long interval = (t > DOOR_WARN_MS) ? 600 : 2500;
    if (now - lastDoorBeep > interval) { buzzerDoor = true; lastDoorBeep = now; }
  } else { doorOpenSince = 0; lastDoorBeep = 0; }

  if (buzzerTimer) tone(BUZZER_PIN, 1760, 300);
  else if (buzzerDoor) {
    unsigned long t = doorOpenSince ? (now - doorOpenSince) : 0;
    tone(BUZZER_PIN, (t > DOOR_WARN_MS) ? 1400 : 880, 100);
  } else if (!doorOpen && !timerDone) noTone(BUZZER_PIN);

  tickClock();
  tickTimer();

  // ── Menú — solo dibuja fondo al cambiar de pantalla ────
  if (inSub) {
    switch (menuPage) {
      case 4: menuSetpoint(); break;
      case 5: menuSetClock();  break;
      case 6: menuSetTimer();  break;
      default: inSub = false; break;
    }
  } else {
    uint8_t oldPage = menuPage;
    if (btnPushed(BTN_UP, 0))   menuPage = (menuPage + 3) % 4;
    if (btnPushed(BTN_DOWN, 1)) menuPage = (menuPage + 1) % 4;

    if (btnPushed(BTN_OK, 2)) {
      switch (menuPage) {
        case 0: inSub = true; menuPage = 4; break;
        case 1: inSub = true; menuPage = 5; editStep = 0; break;
        case 2:
          if (!timerOn && !timerDone) { inSub = true; menuPage = 6; }
          else { timerOn = false; timerDone = false; timerLeft = timerSet; }
          break;
        case 3: break;
      }
    }

    // Si cambió de pantalla, dibujar fondo y entrar
    if (menuPage != prevMenuPage) {
      drawBackground();
      switch (menuPage) {
        case 0: enterTemp();    break;
        case 1: enterClock();   break;
        case 2: enterTimer();   break;
        case 3: enterHumidity(); break;
      }
      prevMenuPage = menuPage;
    }

    // Actualizar solo lo que cambió
    switch (menuPage) {
      case 0: updateTemp();     break;
      case 1: updateClock();    break;
      case 2: updateTimer();    break;
      case 3: updateHumidity(); break;
    }
  }

  // ── Log serial ──────────────────────────────────────────
  if (now - lastSerial > LOG_MS) {
    lastSerial = now;
    Serial.print(F("[")); if (clkH < 10) Serial.print('0'); Serial.print(clkH);
    Serial.print(':'); if (clkM < 10) Serial.print('0'); Serial.print(clkM);
    Serial.print(':'); if (clkS < 10) Serial.print('0'); Serial.print(clkS);
    Serial.print(F("] T=")); Serial.print(currentTemp, 1);
    Serial.print(F(" SP=")); Serial.print(targetTemp, 1);
    Serial.print(F(" H=")); Serial.print((int)humidity);
    Serial.print(F("% P=")); Serial.print(doorOpen ? 'A' : 'C');
    Serial.print(F(" C=")); Serial.print(cooling ? '1' : '0');
    Serial.print(F(" Tm=")); Serial.print(timerOn ? "ON" : "OFF");
    Serial.print(' '); Serial.println(timerLeft);
  }

  delay(80);
}
