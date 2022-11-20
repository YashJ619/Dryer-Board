#include <LiquidCrystal_I2C.h>

#define DEFAULT_HEAT_COUNTDOWN_TIME    600     //Seconds
#define DEFAULT_SET_TEMP          40      //degree Celcius
#define DEFAULT_COOL_COUNTDOWN_TIME    300     //Seconds


LiquidCrystal_I2C lcd(0x27, 16, 4);

char tempBuf[20];

uint8_t a, b;

uint32_t check = millis();
uint32_t start = millis();

uint32_t heatCountdownTime = DEFAULT_HEAT_COUNTDOWN_TIME;
uint32_t coolCountdownTime = DEFAULT_COOL_COUNTDOWN_TIME;

int8_t setTemp = DEFAULT_SET_TEMP;
int8_t drumTemp = 50;

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

bool stop_process = false;
bool startprocess = false;

bool dooropen = false;

enum
{
  INT,
  LEVEL_LOW,
  LEVEL_MED,
  LEVEL_HIGH
} DRYER_LEVEL;

void setup() {
  Serial.begin(9600);

  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);

  lcd.init();

  lcd.backlight();
  DRYER_LEVEL = INT;
}


void loop() {

  if (millis() - start > 500)
  {
    if (startprocess)
    {
      heatCountdownTime--;
      mainscreen();
    }
    else if (lowpress || midpress || highpress)mainscreen();
    else welcomescreen();
    start = millis();
  }

  if (millis() - check > 150)
  {
    //key board code here
    //display code here
    if (!dooropen)
    {
      if ((digitalRead(2) == 0) && !keydead)
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
      else if ((digitalRead(3) == 0) && !keydead)
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
          //heattimeset = false;
          //cooltimeset = false;
          //startprocess = true;
        }
      }
      else if ((digitalRead(4) == 0) && !keydead)
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
          setTemp = 90;
          heatCountdownTime = 1800;
          coolCountdownTime = DEFAULT_COOL_COUNTDOWN_TIME;
          highpress = true;
          DRYER_LEVEL = LEVEL_HIGH;
        }
      }
      else if ((digitalRead(2)) && (digitalRead(3)) && (digitalRead(4)))
      {
        keydead = false;
      }
      else;
    }
    else;
    check = millis();
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

void processcomplete()
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

void test()
{
  welcomescreen();
  delay(1000);
  dooropenscreen();
  delay(1000);
  errorairscreen();
  delay(1000);
  errorheaterscreen();
  delay(1000);

  DRYER_LEVEL = LEVEL_LOW;
  mainscreen();
  delay(1000);

  DRYER_LEVEL = LEVEL_MED;
  mainscreen();
  delay(1000);

  DRYER_LEVEL = LEVEL_HIGH;
  mainscreen();
  delay(1000);

  processcomplete();

}
