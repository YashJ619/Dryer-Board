#include <SPI.h>
#include <Wire.h>
#include <max6675.h>
#include <LiquidCrystal_I2C.h>

#define DRUM_LEFT_PIN   A0
#define DRUM_RIGHT_PIN  A1
#define FAN_PIN         A2
#define HEATER_PIN      A3

#define SELECT_ROTATION_PIN    8
#define DOOR_PIN               5
#define LIMIT_SW_PIN           6
#define HEATERTEMP_SW_PIN      7

#define LOW_SET_PIN     2
#define MED_SET_PIN     3
#define HIGH_SET_PIN    4

#define BUZZER_PIN      10

#define TEMP_PIN_S0        11
#define TEMP_PIN_CS        12
#define TEMP_PIN_SCK       13

LiquidCrystal_I2C lcd(0x27, 16, 4);
MAX6675 temp(TEMP_PIN_SCK, TEMP_PIN_CS, TEMP_PIN_S0);

#define DEFAULT_HEAT_COUNTDOWN_TIME    600     //Seconds
#define DEFAULT_SET_TEMP                40     //degree Celcius
#define DEFAULT_COOL_COUNTDOWN_TIME    300     //Seconds

#define DEFAULT_FF_INTERVAL             30     //Seconds
#define DEFAULT_DELAY_BETWEEN            3     //Seconds

bool anticlockwise = true;
bool clockwise = false;
bool quit = false;

bool dooropen = true;
bool heatererror = true;
bool limitswerror = true;

bool lowpress = false;
bool midpress = false;
bool highpress = false;
bool keydead = false;
bool heattimeset = false;
bool cooltimeset = false;
bool heatblink = false;
bool coolblink = false;
bool hblink = true;
bool cblink = true;
bool limitswTimeout = false;
bool buzzer = false;

bool processcomplete = false;
bool startprocess = false;

uint8_t ffTime = 0;
uint8_t ff_interval = DEFAULT_FF_INTERVAL;
uint8_t gap = 0;
uint8_t delay_between = DEFAULT_DELAY_BETWEEN;
uint8_t count = 0;

uint32_t heatCountdownTime = DEFAULT_HEAT_COUNTDOWN_TIME;
uint32_t coolCountdownTime = DEFAULT_COOL_COUNTDOWN_TIME;

int8_t drumTemp = 0;
int8_t setTemp = DEFAULT_SET_TEMP;

uint32_t start = millis();
uint32_t check = millis();
uint32_t displaytime = millis();

enum
{
  INT,
  LEVEL_LOW,
  LEVEL_MED,
  LEVEL_HIGH
} DRYER_LEVEL;

char tempBuf[20];
uint8_t a, b;

void setup() {
  Serial.begin(9600);

  pinMode(DRUM_LEFT_PIN, OUTPUT);
  pinMode(DRUM_RIGHT_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(HEATER_PIN, OUTPUT);

  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(DOOR_PIN, INPUT_PULLUP);
  pinMode(HEATERTEMP_SW_PIN, INPUT_PULLUP);
  pinMode(LIMIT_SW_PIN, INPUT_PULLUP);
  pinMode(SELECT_ROTATION_PIN, INPUT_PULLUP);

  pinMode(LOW_SET_PIN, INPUT_PULLUP);
  pinMode(MED_SET_PIN, INPUT_PULLUP);
  pinMode(HIGH_SET_PIN, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();

  DRYER_LEVEL = INT;
}

void loop() {
  if (millis() - start > 1000 && !dooropen && !heatererror && !limitswTimeout && startprocess)
  {
    //Serial.println(heatCountdownTime);
    if (heatCountdownTime > 0)
    {
      drumstartwithheater(heatCountdownTime);
      heatCountdownTime--;
      if (heatCountdownTime == 0)
      {
        gap = 3;
        digitalWrite(HEATER_PIN, LOW);
      }
    }
    else if (coolCountdownTime > 0)
    {
      drumstart(coolCountdownTime);
      coolCountdownTime--;
      if (coolCountdownTime == 0)
      {
        stopall();
      }
    }
    else if ((heatCountdownTime == 0) && (coolCountdownTime == 0))
    {
      processcomplete = true;
      buzzer = !buzzer;
      digitalWrite(BUZZER_PIN, buzzer);
    }
    else
    {
      heatCountdownTime = DEFAULT_HEAT_COUNTDOWN_TIME;
      coolCountdownTime = DEFAULT_COOL_COUNTDOWN_TIME;
      setTemp = DEFAULT_SET_TEMP;
    }
    start = millis();
  }

  if (millis() - check > 100)
  {
    //drumTemp = temp.readCelsius();
    if (digitalRead(DOOR_PIN)) {
      dooropen = true;
      stopall();
    }
    else {
      dooropen = false;
    }
    if (digitalRead(HEATERTEMP_SW_PIN)) {
      heatererror = true;
      stopall();
    }
    else {
      heatererror = false;
    }
    if (digitalRead(LIMIT_SW_PIN)) {
      limitswerror = true;
    }
    else {
      limitswerror = false;
    }

    //current temp code here
    //check = millis();
  }

  if (millis() - check > 300)
  {
    //key board code here
    //display code here
    if (!dooropen && !heatererror)
    {
      if ((digitalRead(LOW_SET_PIN) == 0) && !keydead)
      {
        keydead = true;
        if (lowpress || midpress || highpress)
        {
          if ((heatCountdownTime > 60) && heattimeset && !cooltimeset)   //Cannot reduce countdown time to less than 60 seconds
          {
            heatCountdownTime = heatCountdownTime - 60;   //Decrement time by 60 seconds if time decrement switch pressed
          }
          else if ((coolCountdownTime > 60) && cooltimeset)
          {
            coolCountdownTime = coolCountdownTime - 60;   //Decrement time by 60 seconds if time decrement switch pressed
          }
          else;
        }
        else //(!lowpress)
        {
          setTemp = 40;
          heatCountdownTime = 600;
          coolCountdownTime = DEFAULT_COOL_COUNTDOWN_TIME;
          lowpress = true;
          DRYER_LEVEL = LEVEL_LOW;
          //startprocess = true;
        }
      }
      else if ((digitalRead(MED_SET_PIN) == 0) && !keydead)
      {
        keydead = true;
        if (startprocess)
        {
          lowpress = false;
          midpress = false;
          highpress = false;
          startprocess = false;
          heattimeset = false;
          cooltimeset = false;
          processcomplete = false;
          limitswTimeout = false;

          stopall();
          DRYER_LEVEL = INT;
        }
        else if ((DRYER_LEVEL != INT) && !heattimeset)
        {
          heattimeset = true;
          heatblink = true;
        }
        else if ((DRYER_LEVEL != INT) && !cooltimeset)
        {
          cooltimeset = true;
          heatblink = false;
          coolblink = true;
        }
        else if (cooltimeset && heattimeset)
        {
          startprocess = true;
          heattimeset = false;
          cooltimeset = false;
          heatblink = false;
          coolblink = false;
        }
        else //if (!midpress)
        {
          setTemp = 70;
          heatCountdownTime = 1200;
          coolCountdownTime = DEFAULT_COOL_COUNTDOWN_TIME;
          midpress = true;
          DRYER_LEVEL = LEVEL_MED;
          //startprocess = true;
        }
      }
      else if ((digitalRead(HIGH_SET_PIN) == 0) && !keydead)
      {
        keydead = true;
        if (lowpress || midpress || highpress)
        {
          if (lowpress && heattimeset && !cooltimeset)
          {
            heatCountdownTime = heatCountdownTime + 60;
            if (heatCountdownTime > 1800)heatCountdownTime = 1800;
          }
          else if (midpress && heattimeset && !cooltimeset)
          {
            heatCountdownTime = heatCountdownTime + 60;
            if (heatCountdownTime > 2400) heatCountdownTime = 2400;
          }
          else if (highpress && heattimeset && !cooltimeset)
          {
            heatCountdownTime = heatCountdownTime + 60;
            if (heatCountdownTime > 3000) heatCountdownTime = 3000;
          }
          else if ((coolCountdownTime < 900) && (cooltimeset))
          {
            coolCountdownTime = coolCountdownTime + 60;
          }
          else;
        }
        else
        {
          //Serial.println(heatCountdownTime);
          setTemp = 90;
          heatCountdownTime = 1800;
          coolCountdownTime = DEFAULT_COOL_COUNTDOWN_TIME;
          highpress = true;
          DRYER_LEVEL = LEVEL_HIGH;
          //startprocess = true;
        }
      }
      else if ((digitalRead(LOW_SET_PIN)) && (digitalRead(MED_SET_PIN)) && (digitalRead(HIGH_SET_PIN)))
      {
        keydead = false;
      }
      else;
    }
    else;
    drumTemp = temp.readCelsius();
    check = millis();
  }

  if (millis() - displaytime > 750)
  {
    //drumTemp = temp.readCelsius();
    if (dooropen)
    {
      dooropenscreen();
    }
    else if (heatererror)
    {
      errorheaterscreen();
    }
    else if (limitswTimeout)
    {
      errorairscreen();
    }
    else if (DRYER_LEVEL == INT)
    {
      welcomescreen();
    }
    else if (processcomplete)
    {
      processcompscreen();
    }
    else if (DRYER_LEVEL != INT)
    {
      mainscreen();
    }
    else;
    displaytime = millis();
  }
}
void stopall()
{
  stopdrum();
  stopbuzzer();
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(HEATER_PIN, LOW);
}

void stopbuzzer()
{
  buzzer = false;
  digitalWrite(BUZZER_PIN, buzzer);
}
void stopdrum()
{
  digitalWrite(DRUM_LEFT_PIN, LOW);
  digitalWrite(DRUM_RIGHT_PIN, LOW);
}

void drumstartwithheater(uint32_t htime)
{
  drumstart(htime);
  if (limitswerror)
  {
    count++;
    if (count == 30)
    {
      limitswTimeout = true;
      quit = true;
      stopall();
      count = 0;
    }
    else
    {
      limitswTimeout = false;
    }
  }
  else {
    count = 0;
    //drumTemp = 10;//temp.readCelsius();
    //Serial.print("drumTemp:");
    //Serial.println(drumTemp);
    if ((drumTemp < (setTemp - 5)) && (drumTemp > 0))                //Turn heater ON if current temperature is less than (set temperature - 2)
    {
      digitalWrite(HEATER_PIN, HIGH);
    }
    else if ((drumTemp > (setTemp + 2)) || (drumTemp <= 0))       //Turn heater OFF if current temperature is more than (set temperature + 5)
    {
      digitalWrite(HEATER_PIN, LOW);
    }
    else;
  }
}

void drumstart(uint32_t Time)
{
  if (Time > 0)
  {
    digitalWrite(FAN_PIN, HIGH);
    if (anticlockwise && gap == 0)
    {
      digitalWrite(DRUM_LEFT_PIN, HIGH);
      digitalWrite(DRUM_RIGHT_PIN, LOW);
    }
    else if (clockwise && gap == 0)
    {
      digitalWrite(DRUM_LEFT_PIN, LOW);
      digitalWrite(DRUM_RIGHT_PIN, HIGH);
    }
    else {
      stopdrum();
    }
    ffTime++;
    if (gap != 0) {
      ffTime = 0;
      gap--;
    }
    if (ffTime == ff_interval)
    {
      ffTime = 0;
      if (digitalRead(SELECT_ROTATION_PIN) == 0 || clockwise)
      {
        gap = delay_between;
        anticlockwise = !anticlockwise;
        clockwise = !clockwise;
      }
      else
      {
        gap = 0;
      }
    }
  }
}

void mainscreen()
{
  lcd.clear();

  lcd.setCursor(0, 0);
  a = (heatCountdownTime % 60);
  b = (heatCountdownTime / 60);
  memset(tempBuf, 0, sizeof(tempBuf));//Prints countdown time on LCD
  if (heatblink && hblink)
  {
    sprintf(tempBuf, "");
    hblink = !hblink;
  }
  else
  {
    sprintf(tempBuf, "h=%02d:%02d", b, a);
    hblink = !hblink;
  }
  lcd.print(tempBuf);

  lcd.setCursor(9, 0);
  a = (coolCountdownTime % 60);
  b = (coolCountdownTime / 60);
  memset(tempBuf, 0, sizeof(tempBuf));

  if (coolblink && cblink)
  {
    sprintf(tempBuf, "");
    cblink = !cblink;
  }
  else
  {
    sprintf(tempBuf, "c=%02d:%02d", b, a);
    cblink = !cblink;
  }
  lcd.print(tempBuf);


  memset(tempBuf, 0, sizeof(tempBuf));
  lcd.setCursor(2, 1);
  lcd.print("LEVEL: ");
  lcd.setCursor(9, 1);
  if (DRYER_LEVEL == LEVEL_LOW)strcpy(tempBuf, "LOW"); //lcd.print("LOW");
  else if (DRYER_LEVEL == LEVEL_MED)strcpy(tempBuf, "MED"); //lcd.print("MEDIUM");
  else if (DRYER_LEVEL == LEVEL_HIGH)strcpy(tempBuf, "HIGH"); //lcd.print("HIGH");
  else;
  lcd.print(tempBuf);

  lcd.setCursor(-3, 2);
  memset(tempBuf, 0, sizeof(tempBuf));
  sprintf(tempBuf, "Set Temp: %dC", setTemp);          //Prints set temp on LCD
  lcd.print(tempBuf);

  lcd.setCursor(-3, 3);
  memset(tempBuf, 0, sizeof(tempBuf));
  if (drumTemp <= 0)sprintf(tempBuf, "Cur Temp: ERROR");
  else sprintf(tempBuf, "Cur Temp: %dC", drumTemp);

  lcd.print(tempBuf);
}

void processcompscreen()
{
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("PROCESS");
  lcd.setCursor(0, 2);
  lcd.print("COMPLETE");
}

void errorheaterscreen()
{
  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print("ERROR");
  lcd.setCursor(1, 2);
  lcd.print("HEATER");
}
void errorairscreen()
{
  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print("ERROR");
  lcd.setCursor(2, 2);
  lcd.print("AIR");
}

void dooropenscreen()
{
  lcd.clear();
  lcd.setCursor(6, 0);
  lcd.print("DOOR");
  lcd.setCursor(2, 2);
  lcd.print("OPEN");
}

void welcomescreen()
{
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("WELCOME!");
  lcd.setCursor(-3, 2);
  lcd.print("Select a mode");
  lcd.setCursor(-3, 3);
  lcd.print("to start dryer");
}
