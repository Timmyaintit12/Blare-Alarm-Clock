#include <Adafruit_GFX.h> // graphics library
#include <Adafruit_ST7789.h> // driver for the ST7789 screen
#include <SPI.h> 
#include <WiFi.h>
#include "time.h"

//Setup Wifi and Time
const char* ssid = "wifi";
const char* password = "password";


// Defining pins for the display, change according to your setup!!! Uses the white numbers on the ESP
#define TFT_SCLK 0 // labeled SCL on the screen
#define TFT_MOSI 1 // labeled SDA on the screen
#define TFT_RST 2
#define TFT_DC 3
#define TFT_CS 4
#define TFT_BL 5

//Other pins
#define BTN_MENU 4
#define BTN_UP 3
#define BTN_DOWN 2
#define BTN_SNOOZE 1
#define BUZZER 8

// Fix setColRowStart() by exposing it via a subclass
class MyST7789 : public Adafruit_ST7789 {
public:
  MyST7789(int8_t cs, int8_t dc, int8_t mosi, int8_t sclk, int8_t rst)
    : Adafruit_ST7789(cs, dc, mosi, sclk, rst) {}
  void setOffsets(uint8_t col, uint8_t row) {
    _colstart = _colstart2 = col;
    _rowstart = _rowstart2 = row;
  }
};

MyST7789 tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

int hour = 12, minute = 0, second = 0;
int alarmHour = 7, alarmMinute = 0;
bool alarmOn = false;
unsigned long lastSecond = 0;

enum State { CLOCK, SET_ALARM, SET_TIME, RINGING };
State state = CLOCK;

void setupTime(){
  WiFi.begin(ssid, password);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500);
    tries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    configTime(43200, 3600, "pool.ntp.org");
    struct tm t;
    if (getLocalTime(&t, 5000)) {
      hour = t.tm_hour;
      minute = t.tm_min;
      second = t.tm_sec;
      return;
    }
  }
  state = SET_TIME;
}

void draw() {
  char buf[16];
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(5);

  if (state == CLOCK) sprintf(buf, "%02d:%02d%s", hour, minute, alarmOn ? "*" : "");
  else if (state == SET_ALARM) sprintf(buf, "AL%02d:%02d", alarmHour, alarmMinute);
  else if (state == SET_TIME) sprintf(buf, "TM%02d:%02d", hour, minute);
  else sprintf(buf, "WAKE UP");

  tft.print(buf);
}

bool pressed(int pin) {
  static unsigned long last[20] = {0};
  if (digitalRead(pin) == LOW && millis() - last[pin] > 250) {
    last[pin] = millis();
    return true;
  }
  return false;
}

// setup() runs ONCE when the board powers on
void setup() {
  Serial.begin(115200); // lets the board talk to your computer
  pinMode(TFT_BL, OUTPUT); // Set the backlight pin mode, or just wire it to 3.3V
  digitalWrite(TFT_BL, LOW); // Turns the backlight ON, for some reason this screen is active Low, so setting it to LOW is really HIGH
  tft.init(76, 284); // Our panel size (portrait)
  tft.setOffsets(82, 18); // Offsets for the weird resolution
  tft.invertDisplay(false); // Invert the colors (This display is flipped from normal)
  tft.setRotation(1); // Landscape, if it's upside down use 3!
  Serial.println("TFT Initialized!");

  pinMode(BTN_MENU, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SNOOZE, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);

  setupTime();
  draw();

}

// loop() runs OVER and OVER, forever
void loop() {
  if (state != SET_TIME && millis() - lastSecond >= 1000) {
    lastSecond = millis();
    second++;
    if (second >= 60) {
      second = 0;
      minute = (minute + 1) % 60;
      if (minute == 0) hour = (hour + 1) % 24;

      if (state == CLOCK && alarmOn && hour == alarmHour && minute == alarmMinute) {
        state = RINGING;
      }
      draw();
    }
  }

  if (pressed(BTN_MENU)) {
    if (state == CLOCK) state = SET_ALARM;
    else if (state == SET_ALARM) state = SET_TIME;
    else if (state == SET_TIME) state = CLOCK;
    else if (state == RINGING) { noTone(BUZZER); state = CLOCK; }
    draw();
  }

  if (pressed(BTN_UP)) {
    if (state == CLOCK) alarmOn = !alarmOn;
    else if (state == SET_ALARM) alarmMinute = (alarmMinute + 1) % 60;
    else if (state == SET_TIME) minute = (minute + 1) % 60;
    draw();
  }

  if (pressed(BTN_DOWN)) {
    if (state == SET_ALARM) alarmHour = (alarmHour + 1) % 24;
    else if (state == SET_TIME) hour = (hour + 1) % 24;
    draw();
  }

  if (pressed(BTN_SNOOZE) && state == RINGING) {
    noTone(BUZZER);
    int totalMin = (alarmHour * 60 + alarmMinute + 5) % 1440;
    alarmHour = totalMin / 60;
    alarmMinute = totalMin % 60;
    state = CLOCK;
    draw();
  }

  if (state == RINGING) {
    tone(BUZZER, 1000, 200);
    delay(400);
  }
}