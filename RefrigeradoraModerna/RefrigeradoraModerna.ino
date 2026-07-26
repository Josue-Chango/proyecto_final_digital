// ============================================================
//  RefrigeradoraModerna v3.1 — Arduino Mega 2560
//  PINES:
//    LCD      → 22(RS) 23(EN) 24-27(D4-D7)
//    TempPot  → A0    DoorSW  → A1 (pull-up)
//    SetPot   → A2    HumPot  → A3
//    Relay    → 30    Buzzer  → 31
//    Luz      → 32    CompLED → 33   FreezeLED → 34
//    BTN_UP   → 40    BTN_DW  → 41
//    BTN_OK   → 42    BTN_BK  → 43
// ============================================================

#include <LiquidCrystal.h>

// ─── LCD ────────────────────────────────────────────────────
LiquidCrystal lcd(22, 23, 24, 25, 26, 27);

// ─── Pines analógicos ────────────────────────────────────────
const int TEMP_POT_PIN  = A0;   // Simula sensor de temperatura
const int DOOR_SENSOR   = A1;   // Sensor de puerta
const int SETPOINT_PIN  = A2;   // Potenciómetro setpoint
const int HUMIDITY_PIN  = A3;   // Potenciómetro humedad

// ─── Pines de actuadores ─────────────────────────────────────
const int RELAY_PIN     = 30;
const int BUZZER_PIN    = 31;
const int LIGHT_PIN     = 32;
const int COMP_LED      = 33;
const int FREEZE_LED    = 34;

// ─── Botones (activo LOW + pull-up interno) ──────────────────
const int BTN_UP        = 40;
const int BTN_DOWN      = 41;
const int BTN_OK        = 42;
const int BTN_BACK      = 43;

// ─── Parámetros ──────────────────────────────────────────────
const float  HISTERESIS    = 0.5;
const unsigned long DEB_MS = 180;
const unsigned long LOG_MS = 1000;
const unsigned long DOOR_WARN_MS = 10000;

// ─── Estado ──────────────────────────────────────────────────
float  targetTemp  = 5.0;
float  currentTemp = 0.0;
float  humidity    = 50.0;
bool   doorOpen    = false;
bool   cooling     = false;
bool   freezeMode  = false;

// ─── Reloj software ──────────────────────────────────────────
uint8_t  clkH = 12, clkM = 0, clkS = 0;
unsigned long lastClockTick = 0;

// ─── Temporizador ────────────────────────────────────────────
uint16_t timerSet  = 300;  // seg configurados (5 min por defecto)
uint16_t timerLeft = 0;
bool     timerOn   = false;
bool     timerDone = false;
unsigned long lastTimerTick = 0;

// ─── Menú ─────────────────────────────────────────────────────
//  Páginas:   0=Temp  1=Reloj  2=Timer  3=Humedad
//  Submenús:  4=SetSP 5=SetClk 6=SetTmr
uint8_t menuPage = 0;
bool    inSub    = false;
uint8_t editStep = 0;

// ─── Debounce botones ────────────────────────────────────────
unsigned long lastBtn[4] = {0, 0, 0, 0};

// ─── Alarma puerta ────────────────────────────────────────────
unsigned long doorOpenSince = 0;
unsigned long lastDoorBeep  = 0;
unsigned long lastSerial    = 0;

// ─── Buzzer: prioridad ────────────────────────────────────────
bool buzzerDoor  = false;   // alarma puerta activa
bool buzzerTimer = false;   // alarma timer activa

// ─── Caracteres personalizados ───────────────────────────────
byte degCh[8]  = {0b00110,0b01001,0b01001,0b00110,0b00000,0b00000,0b00000,0b00000};
byte snowCh[8] = {0b00100,0b10101,0b01110,0b11111,0b01110,0b10101,0b00100,0b00000};
byte dropCh[8] = {0b00100,0b00100,0b01010,0b10001,0b10001,0b10001,0b01110,0b00000};
byte bellCh[8] = {0b00100,0b01110,0b01110,0b01110,0b11111,0b00000,0b00100,0b00000};
byte clkCh[8]  = {0b00000,0b01110,0b10101,0b10111,0b10001,0b01110,0b00000,0b00000};
byte upCh[8]   = {0b00100,0b01110,0b11111,0b00100,0b00100,0b00100,0b00100,0b00000};
byte dnCh[8]   = {0b00100,0b00100,0b00100,0b00100,0b11111,0b01110,0b00100,0b00000};
byte compCh[8] = {0b01110,0b10001,0b10001,0b01010,0b00100,0b01010,0b10001,0b00000};

// ─── Debounce función ────────────────────────────────────────
bool btnPushed(int pin, int idx) {
  if (digitalRead(pin) == LOW && (millis() - lastBtn[idx] > DEB_MS)) {
    lastBtn[idx] = millis();
    return true;
  }
  return false;
}

// ─── Helper: imprimir 2 dígitos ──────────────────────────────
void p2(uint8_t n) {
  if (n < 10) lcd.print('0');
  lcd.print(n);
}

// ─── Helper: imprimir fila LCD rellena a 16 chars ────────────
void lcdRow(uint8_t row, const char* s) {
  lcd.setCursor(0, row);
  uint8_t len = 0;
  while (s[len] && len < 16) { lcd.print(s[len++]); }
  while (len++ < 16) lcd.print(' ');
}

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  Serial.println(F("=== RefrigeradoraModerna v3.1 — Mega 2560 ==="));

  lcd.begin(16, 2);
  lcd.createChar(0, degCh);
  lcd.createChar(1, snowCh);
  lcd.createChar(2, dropCh);
  lcd.createChar(3, bellCh);
  lcd.createChar(4, clkCh);
  lcd.createChar(5, upCh);
  lcd.createChar(6, dnCh);
  lcd.createChar(7, compCh);

  // Actuadores (evitado range-for por compatibilidad)
  pinMode(RELAY_PIN,  OUTPUT); digitalWrite(RELAY_PIN,  LOW);
  pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);
  pinMode(LIGHT_PIN,  OUTPUT); digitalWrite(LIGHT_PIN,  LOW);
  pinMode(COMP_LED,   OUTPUT); digitalWrite(COMP_LED,   LOW);
  pinMode(FREEZE_LED, OUTPUT); digitalWrite(FREEZE_LED, LOW);

  // Sensores (pull-up)
  pinMode(DOOR_SENSOR, INPUT_PULLUP);
  pinMode(BTN_UP,      INPUT_PULLUP);
  pinMode(BTN_DOWN,    INPUT_PULLUP);
  pinMode(BTN_OK,      INPUT_PULLUP);
  pinMode(BTN_BACK,    INPUT_PULLUP);

  // Splash
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("  Refrigeradora ");
  lcd.setCursor(0,1); lcd.print("  Moderna v3.1  ");
  delay(1400);
  lcd.clear();
  lcd.setCursor(0,0); lcd.print(" Arduino Mega   ");
  lcd.setCursor(0,1); lcd.print("  2560  Ready!  ");
  delay(1200);
  lcd.clear();
}

// ─── Reloj software ──────────────────────────────────────────
void tickClock() {
  if (millis() - lastClockTick >= 1000) {
    lastClockTick = millis();
    if (++clkS >= 60) {
      clkS = 0;
      if (++clkM >= 60) { clkM = 0; if (++clkH >= 24) clkH = 0; }
    }
  }
}

// ─── Temporizador ────────────────────────────────────────────
void tickTimer() {
  if (!timerOn || timerLeft == 0) return;
  if (millis() - lastTimerTick >= 1000) {
    lastTimerTick = millis();
    if (--timerLeft == 0) { timerOn = false; timerDone = true; }
  }
}

// ─── Pantallas de lectura ─────────────────────────────────────
void showTemp() {
  // Fila 0: Temperatura actual
  lcd.setCursor(0, 0);
  if (freezeMode) {
    lcd.write(1); lcd.print("CONGELADOR ");
    lcd.print(currentTemp, 1); lcd.write(byte(0)); lcd.print(" ");
  } else {
    lcd.print("T:"); lcd.print(currentTemp, 1); lcd.write(byte(0));
    lcd.print("C "); lcd.write(byte(7)); lcd.print(cooling ? "ON " : "OFF");
    lcd.print("     ");
  }

  // Fila 1: Setpoint o alarma puerta
  lcd.setCursor(0, 1);
  if (doorOpen) {
    lcd.print("!PUERTA ABIERTA!");
  } else {
    lcd.print("SP:"); lcd.print(targetTemp, 1); lcd.write(byte(0));
    lcd.print("C "); lcd.print(cooling ? "[ENFR]" : "[OK]  ");
    lcd.print("  ");
  }
}

void showClock() {
  // Fila 0: etiqueta
  lcd.setCursor(0, 0);
  lcd.write(4); lcd.print("    RELOJ       ");
  // Fila 1: HH:MM:SS centrado
  lcd.setCursor(0, 1);
  lcd.print("    ");
  p2(clkH); lcd.print(':');
  p2(clkM); lcd.print(':');
  p2(clkS);
  lcd.print("    ");
}

void showTimer() {
  lcd.setCursor(0, 0);
  lcd.write(3); lcd.print(" TEMPORIZADOR   ");
  lcd.setCursor(0, 1);
  if (timerDone) {
    lcd.print("  ** LISTO! **  ");
  } else if (timerOn) {
    uint16_t m = timerLeft / 60, s = timerLeft % 60;
    lcd.print("  "); p2(m); lcd.print(':'); p2(s);
    lcd.print(" Corriendo  ");
  } else {
    uint16_t m = timerSet / 60, s = timerSet % 60;
    lcd.print("  "); p2(m); lcd.print(':'); p2(s);
    lcd.print(" [Parado]   ");
  }
}

void showHumidity() {
  lcd.setCursor(0, 0);
  lcd.write(2); lcd.print("Hum:"); lcd.print((int)humidity);
  lcd.print("% "); lcd.print(freezeMode ? "[FREEZE]" : "[FRIDGE]");
  lcd.setCursor(0, 1);
  lcd.print(cooling ? "COMP:ON " : "COMP:OFF");
  lcd.print(doorOpen ? " PTA:ABT" : " PTA:CER");
  lcd.print(timerOn ? " T" : "  ");
}

// ─── Sub-menús de configuración ───────────────────────────────
void menuSetpoint() {
  lcdRow(0, "AJUSTAR SETPOINT");
  lcd.setCursor(0, 1);
  lcd.write(6); lcd.print(" ");
  lcd.print(targetTemp, 1); lcd.write(byte(0)); lcd.print("C");
  lcd.print("  "); lcd.write(5); lcd.print(" OK=OK    ");

  if (btnPushed(BTN_UP,   0)) { targetTemp = min(15.0f, targetTemp + 0.5f); lcd.clear(); }
  if (btnPushed(BTN_DOWN, 1)) { targetTemp = max(-10.0f,targetTemp - 0.5f); lcd.clear(); }
  if (btnPushed(BTN_OK,   2) || btnPushed(BTN_BACK, 3)) {
    inSub = false; menuPage = 0; lcd.clear();
  }
}

void menuSetClock() {
  lcd.setCursor(0, 0);
  if (editStep == 0) lcdRow(0, "Ajustar HORA:   ");
  else               lcdRow(0, "Ajustar MINUTO: ");
  lcd.setCursor(0, 1);
  lcd.write(6); lcd.print(" ");
  p2(clkH); lcd.print(':'); p2(clkM);
  lcd.print("  "); lcd.write(5); lcd.print(" OK=Sig  ");

  if (btnPushed(BTN_UP,   0)) {
    if (editStep == 0) clkH = (clkH + 1) % 24;
    else               clkM = (clkM + 1) % 60;
    clkS = 0; lcd.clear();
  }
  if (btnPushed(BTN_DOWN, 1)) {
    if (editStep == 0) clkH = (clkH + 23) % 24;
    else               clkM = (clkM + 59) % 60;
    lcd.clear();
  }
  if (btnPushed(BTN_OK, 2)) {
    if (editStep == 0) { editStep = 1; lcd.clear(); }
    else { editStep = 0; inSub = false; menuPage = 1; lcd.clear(); }
  }
  if (btnPushed(BTN_BACK, 3)) { editStep = 0; inSub = false; menuPage = 1; lcd.clear(); }
}

void menuSetTimer() {
  uint16_t m = timerSet / 60, s = timerSet % 60;
  lcdRow(0, "AJUSTAR TIMER   ");
  lcd.setCursor(0, 1);
  lcd.write(6); lcd.print(" "); p2(m); lcd.print(':'); p2(s);
  lcd.print(" "); lcd.write(5); lcd.print(" OK=Inic ");

  if (btnPushed(BTN_UP,   0)) { timerSet = min((uint16_t)5940, (uint16_t)(timerSet + 60)); lcd.clear(); }
  if (btnPushed(BTN_DOWN, 1)) { if (timerSet >= 60) timerSet -= 60; lcd.clear(); }
  if (btnPushed(BTN_OK,   2)) {
    timerLeft = timerSet; timerOn = (timerLeft > 0);
    timerDone = false; lastTimerTick = millis();
    inSub = false; menuPage = 2; lcd.clear();
  }
  if (btnPushed(BTN_BACK, 3)) { inSub = false; menuPage = 2; lcd.clear(); }
}

// ─────────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  // ── Leer sensores ────────────────────────────────────────
  // TempPot mapea 0-5V → -10°C a +40°C para simulación cómoda
  currentTemp = -10.0f + (analogRead(TEMP_POT_PIN) / 1023.0f) * 50.0f;
  humidity    = (analogRead(HUMIDITY_PIN) / 1023.0f) * 100.0f;
  doorOpen    = (digitalRead(DOOR_SENSOR) == LOW);  // LOW = puerta abierta (interruptor N.O.)
  freezeMode  = (targetTemp <= 0.0f);

  // Setpoint desde potenciómetro, EXCEPTO cuando el submenú de setpoint está activo
  if (!(inSub && menuPage == 4)) {
    targetTemp = -10.0f + (analogRead(SETPOINT_PIN) / 1023.0f) * 25.0f;
  }

  // ── Control compresor ─────────────────────────────────────
  if (currentTemp > targetTemp + HISTERESIS) {
    cooling = true;
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(COMP_LED,  HIGH);
  } else if (currentTemp < targetTemp - HISTERESIS) {
    cooling = false;
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(COMP_LED,  LOW);
  }
  digitalWrite(FREEZE_LED, freezeMode ? HIGH : LOW);

  // ── Luz interna ───────────────────────────────────────────
  digitalWrite(LIGHT_PIN, doorOpen ? HIGH : LOW);

  // ── Gestión buzzer (prioridad: timer > puerta) ────────────
  buzzerTimer = (timerDone && (now % 3000 < 400));
  buzzerDoor  = false;

  if (doorOpen) {
    if (doorOpenSince == 0) doorOpenSince = now;
    unsigned long t = now - doorOpenSince;
    unsigned long interval = (t > DOOR_WARN_MS) ? 600 : 2500;
    if (now - lastDoorBeep > interval) {
      buzzerDoor = true;
      lastDoorBeep = now;
    }
  } else {
    doorOpenSince = 0;
    lastDoorBeep  = 0;
  }

  // Activar buzzer según prioridad
  if (buzzerTimer) {
    tone(BUZZER_PIN, 1760, 300);
  } else if (buzzerDoor) {
    unsigned long t = doorOpenSince ? (now - doorOpenSince) : 0;
    tone(BUZZER_PIN, (t > DOOR_WARN_MS) ? 1400 : 880, 100);
  } else if (!doorOpen && !timerDone) {
    noTone(BUZZER_PIN);
  }

  // ── Reloj y temporizador ──────────────────────────────────
  tickClock();
  tickTimer();

  // ── Menú / Navegación ─────────────────────────────────────
  if (inSub) {
    switch (menuPage) {
      case 4: menuSetpoint(); break;
      case 5: menuSetClock(); break;
      case 6: menuSetTimer(); break;
      default: inSub = false; break;
    }
  } else {
    if (btnPushed(BTN_UP,   0)) { menuPage = (menuPage + 3) % 4; lcd.clear(); }
    if (btnPushed(BTN_DOWN, 1)) { menuPage = (menuPage + 1) % 4; lcd.clear(); }

    if (btnPushed(BTN_OK, 2)) {
      switch (menuPage) {
        case 0: inSub = true; menuPage = 4; lcd.clear(); break;
        case 1: inSub = true; menuPage = 5; editStep = 0; lcd.clear(); break;
        case 2:
          if (!timerOn && !timerDone) { inSub = true; menuPage = 6; lcd.clear(); }
          else { timerOn = false; timerDone = false; timerLeft = timerSet; lcd.clear(); }
          break;
        case 3: /* sin sub-menú */ break;
      }
    }

    switch (menuPage) {
      case 0: showTemp();     break;
      case 1: showClock();    break;
      case 2: showTimer();    break;
      case 3: showHumidity(); break;
    }
  }

  // ── Log serial ────────────────────────────────────────────
  if (now - lastSerial > LOG_MS) {
    lastSerial = now;
    Serial.print(F("["));
    if (clkH < 10) Serial.print('0'); Serial.print(clkH);
    Serial.print(':');
    if (clkM < 10) Serial.print('0'); Serial.print(clkM);
    Serial.print(':');
    if (clkS < 10) Serial.print('0'); Serial.print(clkS);
    Serial.print(F("] T=")); Serial.print(currentTemp, 1);
    Serial.print(F(" SP=")); Serial.print(targetTemp, 1);
    Serial.print(F(" H=")); Serial.print((int)humidity);
    Serial.print(F("% Puerta=")); Serial.print(doorOpen ? 'A' : 'C');
    Serial.print(F(" Comp=")); Serial.print(cooling ? '1' : '0');
    Serial.print(F(" Timer=")); Serial.print(timerOn ? "ON" : "OFF");
    Serial.print(' '); Serial.println(timerLeft);
  }

  delay(80);
}
