#include <SPI.h>
#include <Wire.h> 
#include <max6675.h>
#include <LiquidCrystal_I2C.h>
//LiquidCrystal_I2C lcd(0x3F,16,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27,16,4);

#define LOGIC_ON    HIGH
#define LOGIC_OFF   LOW

#define DRUM_LEFT_PIN   A0
#define DRUM_RIGHT_PIN  A1
#define FAN_PIN         A2
#define HEATER_PIN      A3

#define LOW_SET_PIN     2
#define MED_SET_PIN     3
#define HIGH_SET_PIN    4

#define DOOR_PIN        5
#define LIMIT_SW_PIN    6

#define HEATERTEMP_SW_PIN    7

#define BUZZER_PIN      10

#define TEMP_PIN_S0        11
#define TEMP_PIN_CS        12
#define TEMP_PIN_SCK       13

#define SELECT_ROTATION_PIN    8

MAX6675 temp(TEMP_PIN_SCK, TEMP_PIN_CS, TEMP_PIN_S0);

enum
{
  LEVEL_HIGH=0,
  LEVEL_MED,
  LEVEL_LOW
}dryerLevels;

#define DEFAULT_HEAT_COUNTDOWN_TIME    600     //Seconds
#define DEFAULT_SET_TEMP          40      //degree Celcius
#define DEFAULT_COOL_COUNTDOWN_TIME    300     //Seconds

uint8_t levelDryer = LEVEL_LOW;   //Set Level as HIGH, MED OR LOW
int setTemp = DEFAULT_SET_TEMP;   //Set the default temp
uint32_t heatCountdownTime = DEFAULT_HEAT_COUNTDOWN_TIME, coolCountdownTime = DEFAULT_COOL_COUNTDOWN_TIME;  //Set default countdown time
int drumTemp=0;
uint8_t startProcess=0,errorFlag=0,coolingPeriod=0,countdownStarted=0,doorOpen=0,doorFoundOpen=0;
uint32_t starttime_cycle,starttime_checklimit;
uint32_t starttime_delay=millis();

char tempBuf[10];

void setup()
{
  Serial.begin(9600);
  pinMode(DRUM_LEFT_PIN,OUTPUT);
  pinMode(DRUM_RIGHT_PIN,OUTPUT);
  pinMode(FAN_PIN,OUTPUT);
  pinMode(HEATER_PIN,OUTPUT);
  digitalWrite(DRUM_LEFT_PIN,LOGIC_OFF);
  digitalWrite(DRUM_RIGHT_PIN,LOGIC_OFF);
  digitalWrite(FAN_PIN,LOGIC_OFF);
  digitalWrite(HEATER_PIN,LOGIC_OFF);
  pinMode(LOW_SET_PIN,INPUT_PULLUP);
  pinMode(MED_SET_PIN,INPUT_PULLUP);
  pinMode(HIGH_SET_PIN,INPUT_PULLUP);
  pinMode(DOOR_PIN,INPUT_PULLUP);
  pinMode(LIMIT_SW_PIN,INPUT_PULLUP);
  pinMode(HEATERTEMP_SW_PIN,INPUT_PULLUP);
  pinMode(SELECT_ROTATION_PIN,INPUT_PULLUP);
  
  pinMode(BUZZER_PIN,OUTPUT);
  digitalWrite(BUZZER_PIN,LOW);
  
  lcd.init();                      // initialize the lcd 
  lcd.init();
  // Print a message to the LCD.
  lcd.backlight();
}

void loop()
{
  doorOpen = 0;
  if(IsDoorOpen())
  {
    doorOpen=1;
  }
  else if((startProcess==1)&&(errorFlag==0)&&(doorFoundOpen==0))
  {
    Serial.println("ProcessStarted");
    countdownStarted = 1;
    coolingPeriod = 0;
    digitalWrite(FAN_PIN,LOGIC_ON);              //Start Fan
    starttime_cycle = millis();
    starttime_checklimit = millis();
    
    while(heatCountdownTime>0)                  //Do the following process till countdown becomes 0
    {
        if((errorFlag==0)&&(startProcess==1)&&(doorFoundOpen==0))         //Heating time flip flop
        {
          TurnDrumAntiClockwise(30);        //Turn dryer anticlockwise for 30 seconds.
          int rotDirstate = digitalRead(SELECT_ROTATION_PIN);
          Serial.print("rotDirstate=");
          Serial.println(rotDirstate);
          if(rotDirstate==0)
          {
            Serial.print("Stop");
             StopDrum(3);                      //Stop dryer for 3 seconds
             Serial.print("Clockwise");
             if(heatCountdownTime>0)
             {
             TurnDrumClockwise(30); 
             }//Rotate in both directions if SELECT_ROTATION_PIN is low
             Serial.print("Stop");
             StopDrum(3);
          }
        }
        else
        {
          //Exit if any error found or door opened
          return;
        }
    }//While loop
    
    Serial.println("Cycle Complete");
    delay(3000);
    if((errorFlag==0)&&(startProcess==1)&&(doorFoundOpen==0))     //If no error or door closed, start cool down cycle
    {
      coolingPeriod = 1;
      digitalWrite(HEATER_PIN,LOGIC_OFF);          //Switch off heater
      starttime_cycle = millis();
      while(coolCountdownTime>0)                  //Do the following process till countdown becomes 0
      {
          if((errorFlag==0)&&(startProcess==1)&&(doorFoundOpen==0))         //Heating time flip flop
          {
            TurnDrumAntiClockwise(30);        //Turn dryer anticlockwise for 30 seconds.
            int rotDirstate = digitalRead(SELECT_ROTATION_PIN);
            Serial.print("rotDirstate=");
            Serial.println(rotDirstate);
            if(rotDirstate==0)
            {
              if(coolCountdownTime>0)
              {
               StopDrum(3);
              }
               if(coolCountdownTime>0)
               {//Stop dryer for 3 seconds
               TurnDrumClockwise(30);
               }//Rotate in both directions if SELECT_ROTATION_PIN is low
               StopDrum(3);
            }
          }
          else
          {
            //Exit if any error found or door opened
            return;
          }
      }//While loop
      
      starttime_cycle = millis();
      lcd.clear();
      lcd.setCursor(4,0);
      lcd.print("PROCESS");
      lcd.setCursor(0,2);
      lcd.print("COMPLETE");
      Serial.println("Process Complete");
      digitalWrite(BUZZER_PIN,LOW);
      digitalWrite(DRUM_LEFT_PIN,LOGIC_OFF);
      digitalWrite(DRUM_RIGHT_PIN,LOGIC_OFF);
      digitalWrite(FAN_PIN,LOGIC_OFF);
      digitalWrite(HEATER_PIN,LOGIC_OFF);
      //Ring buzzer for 60seconds after process completion or till door opened or Start switch pressed
      while(((millis()-starttime_cycle)<30000)&&(!IsDoorOpen())&&(!IsMedSetSwitchPressed())) 
      {
        ToggleBuzzer();
      }
      
      starttime_cycle = millis();
      //Turn off all relays and buzzer and set default parameters
      heatCountdownTime=DEFAULT_HEAT_COUNTDOWN_TIME;
      coolCountdownTime=DEFAULT_COOL_COUNTDOWN_TIME;
      setTemp=DEFAULT_SET_TEMP;
      countdownStarted = 0;
      coolingPeriod = 0;
      startProcess = 0;
    }
  }
  else if(errorFlag>0)
  {
    if(IsMedSetSwitchPressed())    //If start switch pressed after an error has occured, system goes back to initial state
    {
        errorFlag=0;
        starttime_cycle = millis();
        digitalWrite(BUZZER_PIN,LOW);
        digitalWrite(DRUM_LEFT_PIN,LOGIC_OFF);
        digitalWrite(DRUM_RIGHT_PIN,LOGIC_OFF);
        digitalWrite(FAN_PIN,LOGIC_OFF);
        digitalWrite(HEATER_PIN,LOGIC_OFF);
        heatCountdownTime=DEFAULT_HEAT_COUNTDOWN_TIME;
        coolCountdownTime=DEFAULT_COOL_COUNTDOWN_TIME;
        setTemp=DEFAULT_SET_TEMP;
        countdownStarted = 0;
        coolingPeriod = 0;
        startProcess = 0;
    }
  }
  else
  {
    digitalWrite(BUZZER_PIN,LOW);
    if(doorFoundOpen==1)
    {
      if(doorOpen==0)
      {
        doorFoundOpen=0;
      }
    }
    else
    {
      if(IsLowSetSwitchPressed())
      {
        setTemp = 40;
        heatCountdownTime = 600;
        startProcess=1;   //Start drying process
        doorFoundOpen=0;
        levelDryer=LEVEL_LOW;
      }
      else if(IsMedSetSwitchPressed())
      {
        setTemp = 70;
        heatCountdownTime = 1200;
        startProcess=1;   //Start drying process
        doorFoundOpen=0;
        levelDryer=LEVEL_MED;
      }
      else if(IsHighSetSwitchPressed())
      {
        setTemp = 90;
        heatCountdownTime = 1800;      
        startProcess=1;   //Start drying process
        doorFoundOpen=0;
        levelDryer=LEVEL_HIGH;
      }
    }
  }
  countdownStarted = 0;
//  if(doorOpen==1)
//  {
//    Serial.println("Door Open");
//  }
//  else
//  {
//    Serial.println("Door Closed");
//  }
//  delay(1000);
  UpdateDisplay();
}


void TurnDrumAntiClockwise(int timeOfRotation)
{
  starttime_delay=millis();
  digitalWrite(DRUM_LEFT_PIN,LOGIC_ON);
  while(((millis()-starttime_delay)<(timeOfRotation*1000))&&(errorFlag==0)&&(startProcess==1)&&(doorFoundOpen==0))
  {
    if(coolingPeriod==0)
    {
      MaintainHeatCountdownTimer();
      CheckLimitSwitch();
      CheckHeaterCoilSwitch();
      if(heatCountdownTime<=0)
      {
        break;
      }
    }
    else
    {
//      tempsensors.requestTemperatures();
//      drumTemp = tempsensors.getTempCByIndex(0);
      drumTemp = temp.readCelsius();
      MaintainCoolCountdownTimer();
      if(coolCountdownTime<=0)
      {
        break;
      }
    }
    UpdateDisplay();
    CheckDoorState();
    CheckStopSwitchState();
  }
  digitalWrite(DRUM_LEFT_PIN,LOGIC_OFF);
}

void TurnDrumClockwise(int timeOfRotation)
{
  starttime_delay=millis();
  digitalWrite(DRUM_RIGHT_PIN,LOGIC_ON);
  while(((millis()-starttime_delay)<(timeOfRotation*1000))&&(errorFlag==0)&&(startProcess)&&(doorFoundOpen==0))
  {
    if(coolingPeriod==0)
    {
      MaintainHeatCountdownTimer();
      CheckLimitSwitch();
      CheckHeaterCoilSwitch();
      if(heatCountdownTime<=0)
      {
        break;
      }
    }
    else
    {
      //tempsensors.requestTemperatures();
      //drumTemp = tempsensors.getTempCByIndex(0);
      drumTemp = temp.readCelsius();
      MaintainCoolCountdownTimer();
      if(coolCountdownTime<=0)
      {
        break;
      }
    }
    UpdateDisplay();
    CheckDoorState();
    CheckStopSwitchState();
  }
  digitalWrite(DRUM_RIGHT_PIN,LOGIC_OFF);
}

void StopDrum(int timeOfRotation)
{
  starttime_delay=millis();
  digitalWrite(DRUM_LEFT_PIN,LOGIC_OFF);
  digitalWrite(DRUM_RIGHT_PIN,LOGIC_OFF);
  while(((millis()-starttime_delay)<(timeOfRotation*1000))&&(errorFlag==0)&&(startProcess==1)&&(doorFoundOpen==0))
  {
    if(coolingPeriod==0)
    {
      MaintainHeatCountdownTimer();
      CheckLimitSwitch();
      CheckHeaterCoilSwitch();
      if(heatCountdownTime<=0)
      {
        break;
      }
    }
    else
    {
      //tempsensors.requestTemperatures();
      //drumTemp = tempsensors.getTempCByIndex(0);
      drumTemp = temp.readCelsius();
      MaintainCoolCountdownTimer();
      if(coolCountdownTime<=0)
      {
        break;
      }
    }
    UpdateDisplay();
    CheckDoorState();
    CheckStopSwitchState();    
  }
}


int IsDoorOpen(void)
{
  static uint8_t doorstate=0;
  if(doorstate==0)
  {
    if(digitalRead(DOOR_PIN)==1)
    {
      delay(1);
      if(digitalRead(DOOR_PIN)==1)
      {
        Serial.println("DoorOpen");
        doorstate = 1;
        return 1;
      }
      else
      {
        doorstate = 0;
      }
    }
    else
    {
      doorstate = 0;
    }
  }
  else
  {
    if(digitalRead(DOOR_PIN)==0)
    {
      doorstate = 0;
    }
    else
    {
      doorstate = 1;
      return 1;
    }
  }
  return 0;
}

int IsLimitSwitchPressed(void)
{
static uint8_t limitstate=0;
  if(digitalRead(LIMIT_SW_PIN)==0)
  {
    if(limitstate==1)
    {
      delay(1);
      if(digitalRead(LIMIT_SW_PIN)==0)
      {
        Serial.println("LimitSwitchPressed");
        limitstate = 0;
      }
      else
      {
        limitstate = 1;
      }
    }
  }
  else
  {
    limitstate = 1;
  }
  return (!limitstate);
}

int IsHeaterCoilSwitchOpen(void)
{
static uint8_t heatercoilstate=0;
  if(digitalRead(HEATERTEMP_SW_PIN)==1)
  {
    if(heatercoilstate==1)
    {
      delay(5);
      if(digitalRead(HEATERTEMP_SW_PIN)==1)
      {
        delay(5);
        if(digitalRead(HEATERTEMP_SW_PIN)==1)
        {
          Serial.println("Heater Coil Switch Open");
          heatercoilstate = 0;
        }
        else
        {
          heatercoilstate = 1;
        }
      }
      else
        {
          heatercoilstate = 1;
        }
    }
  }
  else
  {
    heatercoilstate = 1;
  }
  return (!heatercoilstate);
}

int IsLowSetSwitchPressed(void)
{
static uint8_t lowsetstate=0;
  if(digitalRead(LOW_SET_PIN)==0)
  {
    if(lowsetstate==1)
    {
      delay(1);
      if(digitalRead(LOW_SET_PIN)==0)
      {
        Serial.println("LowSetSwitchPressed");
        lowsetstate = 0;
        return 1;
      }
      else
      {
        lowsetstate = 1;
      }
    }
  }
  else
  {
    lowsetstate = 1;
  }
  return 0;
}

int IsMedSetSwitchPressed(void)
{
static uint8_t medsetstate=0;
  if(digitalRead(MED_SET_PIN)==0)
  {
    if(medsetstate==1)
    {
      delay(1);
      if(digitalRead(MED_SET_PIN)==0)
      {
        Serial.println("MedSetSwitchPressed");
        medsetstate = 0;
        return 1;
      }
      else
      {
        medsetstate = 1;
      }
    }
  }
  else
  {
    medsetstate = 1;
  }
  return 0;
}

int IsHighSetSwitchPressed(void)
{
static uint8_t highsetstate=0;
  if(digitalRead(HIGH_SET_PIN)==0)
  {
    if(highsetstate==1)
    {
      delay(1);
      if(digitalRead(HIGH_SET_PIN)==0)
      {
        Serial.println("HighSetSwitchPressed");
        highsetstate = 0;
        return 1;
      }
      else
      {
        highsetstate = 1;
      }
    }
  }
  else
  {
    highsetstate = 1;
  }
  return 0;
}

void MaintainHeatCountdownTimer(void)
{
  static uint32_t starttime_countdown=millis();
  if(countdownStarted==1)
  {
    if(IsLowSetSwitchPressed())
    {
      if(heatCountdownTime>60)      //Cannot reduce countdown time to less than 60 seconds
      {
        heatCountdownTime = heatCountdownTime-60;     //Decrement time by 60 seconds if time decrement switch pressed
      }
    }
    else if(IsHighSetSwitchPressed())
    {
      //Cannot increase countdown time more than 1800 seconds for Low level, similarly 2400 seconds for medium level and 3000 seconds for high level
      if(((heatCountdownTime<1800)&&(levelDryer==LEVEL_LOW))||((heatCountdownTime<2400)&&(levelDryer==LEVEL_MED))||((heatCountdownTime<3000)&&(levelDryer==LEVEL_HIGH)))
      {
        heatCountdownTime = heatCountdownTime+60;
      }
    } 
    if((millis()-starttime_countdown)>1000)
    {
       if(heatCountdownTime>0){heatCountdownTime--;}
       starttime_countdown=millis();
    }
  }
}

void MaintainCoolCountdownTimer(void)
{
  static uint32_t starttime_countdown=millis();
  if(countdownStarted==1)
  {
    if(IsLowSetSwitchPressed())
    {
      if(coolCountdownTime>=60)      //Cannot reduce countdown time to less than 60 seconds
      {
        coolCountdownTime = coolCountdownTime-60;     //Decrement time by 60 seconds if time decrement switch pressed
      }
    }
    if(IsHighSetSwitchPressed())
    {
      //Cannot increase countdown time more than 1800 seconds for Low level, similarly 2400 seconds for medium level and 3000 seconds for high level
      if(coolCountdownTime<900)
      {
        coolCountdownTime = coolCountdownTime+60;
      }
    }
    if((millis()-starttime_countdown)>1000)
    {
       if(coolCountdownTime>0){coolCountdownTime--;}
       starttime_countdown=millis();
    }
  }
}

void UpdateDisplay(void)
{
  static int displayIndex=0;
  int a,b;
  static uint32_t starttime_Display=millis();
  doorOpen = 0;
  if(IsDoorOpen())
  {
    doorOpen=1;
  }
  if((millis()-starttime_Display)>1000)
  {
    memset(tempBuf,0,sizeof(tempBuf));
    if(doorOpen==1)
    {
      if(displayIndex!=1)
      {
        lcd.clear();
        displayIndex = 1;
        lcd.setCursor(6,0);
        lcd.print("DOOR");
        lcd.setCursor(2,2);
        lcd.print("OPEN");
      }
    }
    else if(errorFlag==1)
    {
      if(displayIndex!=2)
      {
        lcd.clear();
        displayIndex = 2;
        lcd.setCursor(5,0);
        lcd.print("ERROR");
        lcd.setCursor(2,2);
        lcd.print("AIR");
      }
    }
    else if(errorFlag==2)
    {
      if(displayIndex!=3)
      {
        lcd.clear();
        displayIndex = 3;
        lcd.setCursor(5,0);
        lcd.print("ERROR");
        lcd.setCursor(1,2);
        lcd.print("HEATER");
      }
    }
//    else if(coolingPeriod==1)
//    {
//      lcd.clear();
//      lcd.setCursor(5,0);
//      lcd.print("00:00");
//      lcd.setCursor(4,1);
//      lcd.print("COOLING");
//      lcd.setCursor(1,2);
//      lcd.print("DOWN");
//      lcd.setCursor(-2,3);
//      memset(tempBuf,0,sizeof(tempBuf));
//      sprintf(tempBuf,"Cur Temp: %dC",drumTemp);             //Prints set temp on LCD
//      lcd.print(tempBuf);
//    }
    else
    {
      if(startProcess==1)
      {
        if(displayIndex!=4)
        {
          lcd.clear();
          displayIndex = 4;      

        
          memset(tempBuf,0,sizeof(tempBuf));
          if(levelDryer==LEVEL_HIGH)
          {
            lcd.setCursor(3,1);
            lcd.print("LEVEL: ");
            lcd.setCursor(10,1);
            strcpy(tempBuf,"HIGH");             //Prints set temp on LCD
          }
          else if(levelDryer==LEVEL_MED)
          {
            lcd.setCursor(2,1);
            lcd.print("LEVEL: ");
            lcd.setCursor(9,1);
            strcpy(tempBuf,"MEDIUM");             //Prints set temp on LCD
          }
          else if(levelDryer==LEVEL_LOW)
          {
            lcd.setCursor(3,1);
            lcd.print("LEVEL: ");
            lcd.setCursor(10,1);
            strcpy(tempBuf,"LOW");             //Prints set temp on LCD
          }
          lcd.print(tempBuf);
          lcd.setCursor(-3,2);
          memset(tempBuf,0,sizeof(tempBuf));
          sprintf(tempBuf,"Set Temp: %dC",setTemp);             //Prints set temp on LCD
    lcd.print(tempBuf);
  }
        lcd.setCursor(0,0);
        a=(heatCountdownTime%60);b=(heatCountdownTime/60);
        sprintf(tempBuf,"h=%02d:%02d",b,a);           //Prints countdown time on LCD
        lcd.print(tempBuf);
        lcd.setCursor(9,0);
        a=(coolCountdownTime%60);b=(coolCountdownTime/60);
        sprintf(tempBuf,"c=%02d:%02d",b,a);           //Prints countdown time on LCD
        lcd.print(tempBuf);
        if(startProcess==1)
        {
          lcd.setCursor(5,3);
          lcd.print("        ");  
          lcd.setCursor(-3,3);
          if(drumTemp<=0)
          {
            memset(tempBuf,0,sizeof(tempBuf));
            sprintf(tempBuf,"Cur Temp: ERROR");             //Prints set temp on LCD
          }
          else
          {
            memset(tempBuf,0,sizeof(tempBuf));
            sprintf(tempBuf,"Cur Temp: %dC",drumTemp);             //Prints set temp on LCD
          }
          lcd.print(tempBuf);
        }
      }
      else
      {
        if(displayIndex!=5)
        {
          lcd.clear();
          displayIndex = 5;
          lcd.setCursor(4,0);
          lcd.print("WELCOME!");
          lcd.setCursor(-3,2);
          lcd.print("Select a mode");
          lcd.setCursor(-3,3);
          lcd.print("to start dryer");
        }
      }
    }
    starttime_Display=millis();
  }
}

void CheckLimitSwitch(void)
{
    if(IsLimitSwitchPressed())
    {
      starttime_checklimit = millis();
      MaintainDrumTemperature();
    }
    else if((millis()-starttime_checklimit)>30000)      //If fan limit switch not pressed for 30 seconds show error
    {
      errorFlag = 1;
      Serial.println("Error Air");
      startProcess = 0;
      digitalWrite(DRUM_LEFT_PIN,LOGIC_OFF);
      digitalWrite(DRUM_RIGHT_PIN,LOGIC_OFF);
      digitalWrite(FAN_PIN,LOGIC_OFF);
      digitalWrite(HEATER_PIN,LOGIC_OFF);
      starttime_checklimit = millis();
    }
    else
    {
      digitalWrite(HEATER_PIN,LOGIC_OFF);
    }
}

void CheckHeaterCoilSwitch(void)
{
    if(IsHeaterCoilSwitchOpen())
    {
      errorFlag = 2;
      Serial.println("Error Heater");
      startProcess = 0;
      digitalWrite(DRUM_LEFT_PIN,LOGIC_OFF);
      digitalWrite(DRUM_RIGHT_PIN,LOGIC_OFF);
      digitalWrite(FAN_PIN,LOGIC_OFF);
      digitalWrite(HEATER_PIN,LOGIC_OFF);
      starttime_checklimit = millis();
    }
}
      
void MaintainDrumTemperature(void)
{
  static uint32_t starttime_Temp=millis();
  if((millis()-starttime_Temp)>1000)
  {  
//    tempsensors.requestTemperatures();
//    drumTemp = tempsensors.getTempCByIndex(0);
    drumTemp = temp.readCelsius();
    Serial.print("drumTemp:");
    Serial.println(drumTemp);
    //if(levelDryer!=LEVEL_NOHEAT)
    {
      if((drumTemp < (setTemp-5)) && (drumTemp>0))                     //Turn heater ON if current temperature is less than (set temperature - 2)
      {
        digitalWrite(HEATER_PIN,LOGIC_ON);
      }
      else if((drumTemp>(setTemp+2))||(drumTemp<=0))                //Turn heater OFF if current temperature is more than (set temperature + 5)
      {
        digitalWrite(HEATER_PIN,LOGIC_OFF);    
      }
    }
    starttime_Temp = millis();
  }
}

void ToggleBuzzer(void)
{
  static uint32_t starttime_buzzer=millis();
  static uint8_t buzzerState=1;
  
  if((millis()-starttime_buzzer)>2000)    //Buzzer toggles after 2000ms(2seconds)
  {
    if(buzzerState==1)
    {
      digitalWrite(BUZZER_PIN,HIGH);
      buzzerState=0;
    }
    else
    {
      digitalWrite(BUZZER_PIN,LOW);
      buzzerState=1;
    }
    starttime_buzzer=millis();
  }
}

void CheckDoorState(void)
{
    if(IsDoorOpen())
    {    
      Serial.println("Door Open");
//      starttime_cycle = millis();
      digitalWrite(BUZZER_PIN,LOW);
      digitalWrite(DRUM_LEFT_PIN,LOGIC_OFF);
      digitalWrite(DRUM_RIGHT_PIN,LOGIC_OFF);
      digitalWrite(FAN_PIN,LOGIC_OFF);
      digitalWrite(HEATER_PIN,LOGIC_OFF);
      doorFoundOpen=1;
//      countdownTime=DEFAULT_COUNTDOWN_TIME;
//      setTemp=DEFAULT_SET_TEMP;
//      countdownStarted = 0;
//      coolingPeriod = 0;
//      startProcess = 0;
    }
}

void CheckStopSwitchState(void)
{
    if(IsMedSetSwitchPressed())
    {    
      starttime_cycle = millis();
      digitalWrite(BUZZER_PIN,LOW);
      digitalWrite(DRUM_LEFT_PIN,LOGIC_OFF);
      digitalWrite(DRUM_RIGHT_PIN,LOGIC_OFF);
      digitalWrite(FAN_PIN,LOGIC_OFF);
      digitalWrite(HEATER_PIN,LOGIC_OFF);
      heatCountdownTime=DEFAULT_HEAT_COUNTDOWN_TIME;
      coolCountdownTime=DEFAULT_COOL_COUNTDOWN_TIME;
      setTemp=DEFAULT_SET_TEMP;
      countdownStarted = 0;
      coolingPeriod = 0;
      startProcess = 0;
    }
}
