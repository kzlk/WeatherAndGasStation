//********************MODULE SETTINGS****************************
//Check your sensor address using i2c_scanner.ino or datasheet
//In this example sensor address:
// aht20 0x38
// bmp280 0x77

//uncomment this line if you use RGB led strip
 //#define ADRESSING_RGB 

//uncomment this line if you use DHT11(22) sensor
// #define DHT11_SENSOR

//uncomment this line if you use AHT20 sensor with I2C bus
 #define AHT20_SENSOR

//uncomment this line if you use BMP280 sensor with I2C bus
 #define BMP280_SENSOR

//uncomment this line if you want to see debug messages in serial monitor
 //#define SERIAL_DEBUG 
//*************************************************************

#ifdef ADRESSING_RGB
  #define NUM_LEDS 16 // Number of controlled LEDs
  #define LED_PIN 4   // LED strip connection pin
  #include <FastLED.h>
  CRGB leds[NUM_LEDS];
#endif

#ifdef DHT11_SENSOR
  #include <SimpleDHT.h>
  SimpleDHT11 dht11 //if you use DHT22 sensor, change to SimpleDHT22 dht11;
#endif

#ifdef AHT20_SENSOR
  #include <AHT20.h>
  AHT20 aht20;
#endif

#ifdef BMP280_SENSOR
  #include <GyverBME280.h>
  GyverBME280 bme;
#endif

#include <Adafruit_PCD8544.h>
#include <Adafruit_GFX.h>
#include <stdio.h>
#include <DS1302.h>
#include <EEPROMex.h>
#include "tones.h"

/**************CONSTANTS*****************/
#define BTN_PROTECT 100 // Button debounce protection
#define LCD_RENEW 250   // Screen refresh rate
#define HEATING 60000   // Smoke sensor warm-up time (60 seconds)
#define MQ2_DEFAULT 300 // Initial alarm level (during the first firmware)

#ifdef SERIAL_DEBUG
  #define SERIAL_SPEED 9600
  #define vers "SmokeClock 1.3"
 #endif

/**************PIN SETTINGS*****************/
// Control button pins
#define BTN_UP 10   // Increase button
#define BTN_DOWN 9  // Decrease button
#define BTN_SET 11  // Set button
#define BTN_RESET 12 // Reset button

// Clock module connection pins
#define kCePin 0   // RST
#define kIoPin 1   // DAT
#define kSclkPin 2 // CLK

// Smoke sensor connection pins
#define MQ2_ANALOG_PIN A3       // A0 to  А3

#ifdef DHT11_SENSOR
  #define DHT22_PIN 2 // Humidity sensor (DHT11/22) connection pin
#endif

#define FOTORES A1  // Photoresistor connection pin
#define LCD_LED 3  // PWM pin for LCD backlight

#define BUZZER_PIN A2  // Speaker connection pin

//Nokia 5110 display
#define SCLK_PIN 4
#define DIN_PIN 5
#define DC_PIN 6
#define SCE_PIN 8
#define RST_PIN 7     
         
/**************VARIABLES*****************/
Adafruit_PCD8544 display = Adafruit_PCD8544(SCLK_PIN, DIN_PIN, DC_PIN, SCE_PIN, RST_PIN);
DS1302 rtc(kCePin, kIoPin, kSclkPin);
Time t = rtc.time();

int bright, btn_up_val, btn_down_val, btn_set_val, now_year, mq2, mq2_alarm; 
int btn_reset_val;
long btn_reset_millis;
float pressure, now_temp, now_hum;
byte now_disp, now_month, now_date, now_hour, now_min, now_sec, now_week_day, alarm_hour, alarm_min;
long now_millis, lcd_millis, time_millis, btn_up_millis, btn_down_millis, btn_set_millis, disp_millis, horn_millis, mq2_start_alarm;
boolean dot, blnk, alarm, horn, horn_smoke, note, time_changed;
uint8_t gHue = 0;
byte set_time;
char sep;

uint16_t disp[5] = {25000, 3000, 3000, 3000, 3000}; // Main screen timing 25 seconds, others 3 seconds
//****************************
uint16_t melody[] = {NOTE_D7, NOTE_D8, NOTE_D7, NOTE_D8, NOTE_D7, NOTE_D8, NOTE_D7, NOTE_D8}; // Melody

uint8_t noteDurations[] = {4, 4, 4, 4, 4, 4, 4, 4};
//****************************
char *week_day[7] = {
    "Mon",
    "Tue",
    "Wed",
    "Thu",
    "Fri",
    "Sat",
    "Sun",
};
//****************************
void writeBigString(char *str, uint8_t x, uint8_t y, uint8_t textSize = 2);

void setup()
{

#ifdef SERIAL_DEBUG
   Serial.begin(SERIAL_SPEED);
   Serial.println(vers);
#endif

   // Control buttons
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SET, INPUT_PULLUP);
  pinMode(BTN_RESET, INPUT_PULLUP);
  pinMode(FOTORES, INPUT);
  pinMode(MQ2_ANALOG_PIN, INPUT);
  pinMode(LCD_LED, OUTPUT);
  analogWrite(LCD_LED, 255);
  pinMode(BUZZER_PIN, OUTPUT);

  alarm_hour = EEPROM.readByte(0);
  alarm_min = EEPROM.readByte(1);
  alarm = EEPROM.readByte(2);
  mq2_alarm = EEPROM.readInt(3);

#ifdef SERIAL_DEBUG
  Serial.print("Alarm level: ");
  Serial.println(mq2_alarm);
#endif

  // Set the default level if not specified otherwise
  if ((mq2_alarm < 0) or (mq2_alarm > 1000))
    mq2_alarm = MQ2_DEFAULT;

  rtc.writeProtect(false);
  rtc.halt(false);

  // Initial time setting if required from the program
  // Time t(2023, 9, 9, 19, 44, 10, 6); // year-month-date-hour-minute-second-day-of-the-week
  // rtc.time(t);

  // Check if the AHT20 will acknowledge
  if (!aht20.begin())
  {
    #ifdef SERIAL_DEBUG
      Serial.println("AHT20 not detected. Please check wiring. Freezing.");
    #endif
  }
  else
    #ifdef SERIAL_DEBUG
  Serial.println("AHT20 acknowledged.");
    #endif

  bme.setTempOversampling(MODULE_DISABLE);
  bme.setHumOversampling(MODULE_DISABLE); 
  bme.setMode(FORCED_MODE);                     
  bme.setStandbyTime(STANDBY_1000MS);     
  bme.begin(0x77);

#ifdef ADRESSING_RGB
  FastLED.addLeds<WS2811, LED_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    // LED initialization
  FastLED.setBrightness(100);
  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = CRGB::Blue;
    FastLED.show();
    FastLED.delay(50);
  }
#endif

  display.clearDisplay();
  display.begin();
  display.setContrast(75);
  display.setTextSize(2);
  display.setTextColor(BLACK);
  display.setCursor(0, 0);
  display.println("GAS 1.3");
  display.setTextSize(1);
  display.setCursor(15, 16);
  display.println("SmokeClock");
  display.setCursor(18, 24);
  display.display();

  tone(BUZZER_PIN, NOTE_D7, 100); // Beep once at startup for buzzer check
  delay(1000);
  time_read();
  display.clearDisplay();
}
//****************************
void (*resetFunc)(void) = 0; // reset function once per 50 days
//****************************
void loop()
{
  display.setTextSize(1);
  now_millis = millis();
  btn_up_val = digitalRead(BTN_UP);
  btn_down_val = digitalRead(BTN_DOWN);
  btn_set_val = digitalRead(BTN_SET);
  btn_reset_val = digitalRead(BTN_RESET);

  /* Button processing */ 

  //Button Up
  if ((btn_up_val == LOW) & (now_millis - btn_up_millis) > BTN_PROTECT)
  { 
    horn = false;
    switch (set_time)
    {
    case 1:
      now_hour++;
      time_changed = true;
      if (now_hour >= 24)
        now_hour = 0;
      break;
    case 2:
      now_min++;
      time_changed = true;
      if (now_min >= 60)
        now_min = 0;
      break;
    case 3:
      now_sec = 0;
      time_changed = true;
      set_time_now();
      disp_millis = now_millis;
      set_time = 0;
      break;
    case 4:
      alarm_hour++;
      if (alarm_hour >= 24)
        alarm_hour = 0;
      break;
    case 5:
      alarm_min++;
      if (alarm_min >= 60)
        alarm_min = 0;
      break;
    case 6:
      alarm = !alarm;
      break;
    case 7:
      now_year++;
      time_changed = true;
      if (now_year >= 2100)
        now_year = 2000;
      break;
    case 8:
      now_month++;
      time_changed = true;
      if (now_month >= 13)
        now_month = 1;
      break;
    case 9:
      now_date++;
      time_changed = true;
      if (now_date >= 32)
        now_date = 1;
      break;
    case 10:
      now_week_day++;
      time_changed = true;
      if (now_week_day >= 7)
        now_week_day = 0;
      break;
    case 11:
      mq2_alarm += 50;
      if (mq2_alarm > 1000)
        mq2_alarm = 0;
      break;
    }

    btn_up_millis = now_millis + 300;
  }
  //Button Down
  if ((btn_down_val == LOW) & (now_millis - btn_down_millis) > BTN_PROTECT)
  { 
    horn = false;
    switch (set_time)
    {
    case 1:
      now_hour--;
      time_changed = true;
      if (now_hour == 255)
        now_hour = 23;
      break;
    case 2:
      now_min--;
      time_changed = true;
      if (now_min == 255)
        now_min = 59;
      break;
    case 3:
      now_sec = 0;
      time_changed = true;
      set_time_now();
      disp_millis = now_millis;
      set_time = 0;
      break;
    case 4:
      alarm_hour--;
      if (alarm_hour == 255)
        alarm_hour = 23;
      break;
    case 5:
      alarm_min--;
      if (alarm_min == 255)
        alarm_min = 59;
      break;
    case 6:
      alarm = !alarm;
      break;
    case 7:
      now_year--;
      time_changed = true;
      if (now_year == 2000)
        now_year = 2099;
      break;
    case 8:
      now_month--;
      time_changed = true;
      if (now_month == 0)
        now_month = 12;
      break;
    case 9:
      now_date--;
      time_changed = true;
      if (now_date == 0)
        now_date = 31;
      break;
    case 10:
      now_week_day--;
      time_changed = true;
      if (now_week_day == 255)
        now_week_day = 6;
      break;
    case 11:
      mq2_alarm -= 50;
      if (mq2_alarm < 0)
        mq2_alarm = 1000;
      break;
    }
    btn_down_millis = now_millis + 300;
  }
  //Button Reset
  if ((btn_set_val == LOW) & (now_millis - btn_set_millis) > BTN_PROTECT)
  { 
    horn = false;
    if (now_disp != 0)
    {
      now_disp = 0;
      display.clearDisplay();
    }
    set_time = (set_time + 1) % 13;
    //exit from the setting mode and write the time
    if (set_time == 12)
    { 
      set_time_now();
      set_time = 0;
      now_disp = 0;
      disp_millis = now_millis;
    }
    btn_set_millis = now_millis + 300;
  }
  //Reset Button
  if ((btn_reset_val == LOW) & (now_millis - btn_reset_millis) > BTN_PROTECT)
  {
    horn = false;
    horn_smoke = false;
    set_time = 12;
    set_time_now();
    set_time = 0;
    now_disp = 0;
    disp_millis = now_millis;
    btn_reset_millis = now_millis + 300;
  }
  //Renew time once per second
  if (now_millis - time_millis > 1000)
  {
    dot = !dot;
    if (dot)
    {
      sep = ':';
    }
    else
    {
      sep = ' ';
    };
    if (set_time == 0)
    {
      time_read();
    }
    set_lcd_led();
    if ((now_hour == alarm_hour) and (now_min == alarm_min) and (now_sec < 2) and (alarm))
    {
      horn = true;
      #ifdef SERIAL_DEBUG
      Serial.println("WAKE UP!!!");
      #endif
    }
    // check the alarm
    if ((now_hour != alarm_hour) or (now_min != alarm_min))
    {
      horn = false;
    } 
    // turn off the alarm after 1 minute
    if ((mq2 >= mq2_alarm) and (set_time == 0))
    {
      horn_smoke = true;
      mq2_start_alarm = now_millis;
      #ifdef SERIAL_DEBUG
      Serial.println("SMOKE!!!");
      #endif
    } 
    //check smoke sensor
    if ((now_millis - mq2_start_alarm > 10000) and (mq2 <= mq2_alarm))
    {
      horn_smoke = false;
    } //turn off the alarm

    // проверка переполнения millis и сброс раз в 46 суток. максимально возможно значение 4294967295, это около 50 суток.
    if (millis() > 4000000000)
    {
      resetFunc();
    }; 
    time_millis = now_millis;
  }
  //Signals
  if (set_time == 0)
  { 
    //alarm
    if ((horn) and (now_millis - horn_millis > 250))
    { 
      display.clearDisplay();
      noTone(BUZZER_PIN);
      if (note)
      {

        tone(BUZZER_PIN, NOTE_D7, 250);
        analogWrite(LCD_LED, 255);
#ifdef ADRESSING_RGB
        for (int i = 0; i < NUM_LEDS; i++)
          leds[i] = CRGB::Blue;
        FastLED.setBrightness(100);
        FastLED.show();
#endif
      }
      else
      {
        tone(BUZZER_PIN, NOTE_D6, 250);
        analogWrite(LCD_LED, 0);
#ifdef ADRESSING_RGB
        for (int i = 0; i < NUM_LEDS; i++)
          leds[i] = CRGB::Green;
        FastLED.setBrightness(100);
        FastLED.show();
#endif
      }
      writeBigString("WAKEUP", 0, 8);
      lcd_millis = now_millis;
      note = !note;
      horn_millis = now_millis;
    }
    //smoke alarm
    if ((horn_smoke) and (now_millis - horn_millis > 250) and (now_millis > HEATING))
    { 
      display.clearDisplay();
      noTone(BUZZER_PIN);
      if (note)
      {
        tone(BUZZER_PIN, NOTE_D8, 250);
        analogWrite(LCD_LED, 255);
#ifdef ADRESSING_RGB
        for (int i = 0; i < NUM_LEDS; i++)
          leds[i] = CRGB::Blue;
        FastLED.setBrightness(100);
        FastLED.show();
#endif
      }
      else
      {
        tone(BUZZER_PIN, NOTE_D7, 250);
        analogWrite(LCD_LED, 0);
#ifdef ADRESSING_RGB
        for (int i = 0; i < NUM_LEDS; i++)
          leds[i] = CRGB::Red;
        FastLED.setBrightness(100);
        FastLED.show();
#endif
      }
      writeBigString("ALARM!", 10, 8);
      lcd_millis = now_millis;
      note = !note;
      horn_millis = now_millis;
    }
  }
  //Change screens by timing
  if ((now_millis - disp_millis > disp[now_disp]) and (set_time == 0))
  {
    now_disp = (now_disp + 1) % 6;
    display.clearDisplay(); 
    disp_millis = now_millis;
  };
  //Update the screen
  if (now_millis - lcd_millis > LCD_RENEW)
  { 
    print_lcd();
#ifdef ADRESSING_RGB
    if (!horn)
    {
      fill_rainbow(leds, NUM_LEDS, gHue, 7);
      FastLED.show();
      gHue++;
    }
#endif
    lcd_millis = now_millis;
  }

  display.display();
}
//****************************
// Screen rendering
void print_lcd(void)
{ 
  display.clearDisplay();
  char time_str[6], sec_str[3], date_str[15], pres_str[11], alarm_str[6], set_str, mq_str[5], hum_str[3], temp_str[3];
  char m_clock[3], l_clock[3];

  snprintf(time_str, sizeof(time_str), "%02d%c%02d", now_hour, sep, now_min);
  snprintf(sec_str, sizeof(sec_str), "%02d", now_sec);
  snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d %s", now_year, now_month, now_date, week_day[now_week_day]);
  snprintf(temp_str, sizeof(temp_str), "%02d", now_temp);

  snprintf(m_clock, sizeof(m_clock), "%02d", now_hour);
  snprintf(l_clock, sizeof(l_clock), "%02d", now_min);

  if (now_millis < HEATING)
  {
    snprintf(mq_str, sizeof(mq_str), "HEAT");
  }
  else if (set_time == 0)
  {
    snprintf(mq_str, sizeof(mq_str), "%04d", mq2);
  }
  else
  {
    snprintf(mq_str, sizeof(mq_str), "%04d", mq2_alarm);
  }

  dtostrf((float)now_temp, 2, 0, temp_str);
  dtostrf((float)now_hum, 2, 0, hum_str);
  snprintf(pres_str, sizeof(pres_str), "T:%sC H:%s", temp_str, hum_str);
  snprintf(alarm_str, sizeof(alarm_str), "%02d:%02d", alarm_hour, alarm_min);
  if (alarm)
  {
    set_str = '+';
  }
  else
  {
    set_str = '-';
  };

  if ((set_time != 0) and (blnk) and (now_disp == 0))
  { // мигание при установке времени
    switch (set_time)
    {
    case 1:
      time_str[0] = ' ';
      time_str[1] = ' ';
      break;
    case 2:
      time_str[3] = ' ';
      time_str[4] = ' ';
      break;
    case 3:
      sec_str[0] = ' ';
      sec_str[1] = ' ';
      break;
    case 4:
      alarm_str[0] = ' ';
      alarm_str[1] = ' ';
      break;
    case 5:
      alarm_str[3] = ' ';
      alarm_str[4] = ' ';
      break;
    case 6:
      set_str = ' ';
      break;
    case 7:
      date_str[0] = ' ';
      date_str[1] = ' ';
      date_str[2] = ' ';
      date_str[3] = ' ';
      break;
    case 8:
      date_str[5] = ' ';
      date_str[6] = ' ';
      break;
    case 9:
      date_str[8] = ' ';
      date_str[9] = ' ';
      break;
    case 10:
      date_str[11] = ' ';
      date_str[12] = ' ';
      date_str[13] = ' ';
      break;
    case 11:
      mq_str[0] = ' ';
      mq_str[1] = ' ';
      mq_str[2] = ' ';
      mq_str[3] = ' ';
      break;
    }
  }
  blnk = !blnk;

  switch (now_disp)
  {
  case 0:
    // Print the date
    display.setCursor(0, 0);
    display.print(date_str);

    // Print the MQ sensor status
    display.setCursor(30, 32);
    display.print(mq_str);

    // Print the pressure
    display.setCursor(0, 8);
    display.setTextSize(1);
    display.setCursor(12, 24);
    display.println(pres_str);

    // Print the time
    writeBigString(time_str, 10, 8);

    // Print the alarm
    display.setCursor(0, 40);
    display.println(alarm_str);

    // Print the seconds
    display.setCursor(71, 8);
    display.println(sec_str);

    // Print the set
    display.setCursor(30, 40);
    display.println(set_str);

#ifdef BMP280_SENSOR
    // Print the pressure
    display.setCursor(41, 40);
    display.println((int)(pressure));
    display.setCursor(60, 40);
    display.println("mmHg");
#endif
    break;
  case 1:
    writeBigString(hum_str, 7, 10, 4);
    writeBigString("%", 60, 16, 3);
    break;
  case 2:
    // Print the high part clock
    display.setTextSize(3);
    display.setCursor(0, 10);
    display.print(m_clock);

    // Print the separator
    display.setCursor(30, 10);
    display.print(sep);

    // Print the low part clock
    display.setCursor(45, 10);
    display.print(l_clock);
    break;
  case 3:
    writeBigString(temp_str, 7, 10, 4);
    writeBigString("C", 60, 10, 3);
    break;

#ifdef BMP280_SENSOR    
  case 4:
    display.setTextSize(3);
    display.setCursor(15, 10);
    display.println((int)pressure);
    display.setTextSize(1);
    display.setCursor(28,35);
    display.println("mmHg");
    break;
  }
#endif
}
//****************************
void writeBigString(char *str, uint8_t x, uint8_t y, uint8_t textSize = 2)
{
  display.setTextSize(textSize);
  display.setCursor(x, y);
  display.print(str);
  display.setTextSize(1);
}
//****************************
// Read the time and data from sensors and store the values in variables for operation
void time_read()
{ 

  mq2 = analogRead(MQ2_ANALOG_PIN);
  
#ifdef BMP280_SENSOR
  // Read the pressure
  pressure = bme.readPressure();

  if (pressure != 0)
  {
    // Convert pressure from hPa to mmHg
    pressure = pressureToMmHg(pressure);
  }
  else
  {
    // Try again
    bme.begin(0x77); 
  }
#endif  

#ifdef DHT11_SENSOR
   if(dht11.read2(DHT22_PIN, &now_temp, &now_hum, NULL) != SimpleDHTErrSuccess)
    now_hum = 0, now_temp = 0;
    #ifdef SERIAL_DEBUG
     Serial.print("DHT: ");
  #endif
#endif

#ifdef AHT20_SENSOR
  // If the sensor is connected, read the temperature and humidity
  if (aht20.isConnected())
  {
    // If the sensor has new data available, read it
    if (aht20.available())
    {
      now_temp = aht20.getTemperature();
      now_hum = aht20.getHumidity();

    #ifdef SERIAL_DEBUG
      Serial.print("AHT20: "); 
    #endif
    }
    else
    {
      // If the sensor has no new data available, keep using the last value
    }
  }
  else
  {
    // If the sensor is not connected, use zero for the temperature and humidity
    now_temp = 0;
    now_hum = 0;
  }
#endif

#ifdef SERIAL_DEBUG
    Serial.print((float)now_temp); Serial.print("C, ");
    Serial.print((float)now_hum); Serial.println("%");
    Serial.print("MQ-2: "); Serial.print(mq2);
#endif

  t = rtc.time();
  now_year = t.yr;
  now_month = t.mon;
  now_date = t.date;
  now_hour = t.hr;
  now_min = t.min;
  now_sec = t.sec;
  now_week_day = t.day;
}
//****************************
// Set the screen backlight brightness level
void set_lcd_led()
{ 
  bright = map(analogRead(FOTORES), 320, 1024, 0, 5);
  if (bright < 1)
    bright = 1;
  analogWrite(LCD_LED, bright * 51);
#ifdef ADRESSING_RGB
  FastLED.setBrightness(100);
#endif
  // FastLED.setBrightness(bright*51);  // адаптивная подсветка светодиодов
}
//****************************
// Set the time and write the alarm and smoke sensor threshold levels to non-volatile memory
void set_time_now()
{ 
  if (time_changed)
  {
    Time tt(now_year, now_month, now_date, now_hour, now_min, now_sec, now_week_day);
    rtc.time(tt);
  };
  time_changed = false;
  EEPROM.writeByte(0, alarm_hour);
  EEPROM.writeByte(1, alarm_min);
  EEPROM.writeByte(2, alarm);
  EEPROM.writeInt(3, mq2_alarm);
}
//****************************