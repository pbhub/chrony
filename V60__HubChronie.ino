#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>
#include <string.h>
//#include <pbhub.h>

#include <SPI.h>

#include "U8glib.h"

const float Versionsnummer = 0.61;

//Displays:
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); //Arduino Uno Analog in 4+5
U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NONE|U8G_I2C_OPT_DEV_0);  // I2C / TWI 

//Werte:
volatile int fpsPointerNextWertImArray = 0;
volatile int maxAnzahlWerteImArray = -1;
volatile int anzahlWerteImArray = 0;

//Inhaltwerte
volatile float bpsLog[5];
volatile int fpsLog[25]; // <-- 25 ist nur eine Möglichekeit, Du kannst hier so viele / wenig Werte nehmen wie Du willst
volatile int varianz = -1; 
volatile int sigma = -1;
volatile int mittelwert = -1;

const int l = 100; //Abstand der Lichtschranke in mm
volatile int shotCounter = 0;

volatile int sigmaMax = 0;

volatile int delaySwitch = 0;
volatile unsigned long Zeit1 = 0;
volatile unsigned long Zeit2 = 0;
volatile unsigned long Dauer = 0;

volatile long lastBpS = -1;
volatile long last = -1;

volatile float aktBps = -1;
volatile float maxBps = -1;

volatile float fps = 0;
volatile float fpsMin = 0;
volatile float fpsMax = 0;

volatile int shotCount = 0;

void SetupLCD()
{
  lcd.begin (20,4);
  lcd.setBacklightPin(3,POSITIVE);
  lcd.setBacklight(HIGH);
  lcd.home ();  
}

void InitLCD()
{    
  lcd.setCursor(0,0);
  lcd.print("V");
  lcd.setCursor(1,0); 
  lcd.print(Versionsnummer,2);
  lcd.setCursor(6,0);
  lcd.print("pbhub.de");
  lcd.setCursor(0,1);
  lcd.print("fps:");
  lcd.setCursor(9,1);
  lcd.print("MAXfps:"); 
  lcd.setCursor(0,2);
  lcd.print("bps:"); 
  lcd.setCursor(9,2);
  lcd.print("MAXbps:"); 
  lcd.setCursor(0,3);
  lcd.print("#Shots:"); 
  lcd.setCursor(9,3);
  lcd.print("AVGfps+-"); 
}


 
void setup() {
  //Serial.begin(9600);
  //Serial.print("Start Up");   

  SetupLCD();  
  
  SetupU8G();    
  InitLCD();  
  SetupSonderzeichen();    
  
  attachInterrupt(0, Starten, FALLING);//PIN2
  attachInterrupt(1, Stoppen, FALLING);//PIN3
  
  lastBpS = millis();  
}


void SetupU8G()
{
  if ( u8g.getMode() == U8G_MODE_R3G3B2 ) {
    u8g.setColorIndex(255);     // white
  }
  else if ( u8g.getMode() == U8G_MODE_GRAY2BIT ) {
    u8g.setColorIndex(3);         // max intensity
  }
  else if ( u8g.getMode() == U8G_MODE_BW ) {
    u8g.setColorIndex(1);         // pixel on
  }
  else if ( u8g.getMode() == U8G_MODE_HICOLOR ) {
    u8g.setHiColorByRGB(255,255,255);
  }
}

void UpdateFps()
{
  
  lcd.setCursor(5,1);  
  lcd.print(fps,0);  

  lcd.setCursor(17,1);
  lcd.print(fpsMax,0);

  lcd.setCursor(7,3);
  lcd.print(shotCounter);
  
}

void UpdateBps()
{
    lcd.setCursor(4,2);
    
    float tmp = aktBps;
    if (tmp < 0)
    {
      tmp =0;
    }
    if (tmp < 10)
    {
      lcd.print(" ");
    }
    lcd.print(tmp,1);    
    
    lcd.setCursor(16,2);
    tmp = maxBps;
    if (tmp < 0)
    {
      tmp =0;
    }
    if (tmp < 10)
    {
      lcd.print(" ");
    }
    lcd.print(tmp,1);

}

void AddBpS()
{  
  long now = millis();
   
  float aktBpsDeltaT = now - last;
  if (aktBpsDeltaT == 0)
  {
    return;
  }
   
  last = now;
  
  float sumBpsT = aktBpsDeltaT;
  float maxBpsTAkt = 1 * 1000 / aktBpsDeltaT;
  
  if (maxBpsTAkt > maxBps && maxBpsTAkt < 25)
  {
    maxBps = maxBpsTAkt;
  }
  
  for (int i = 4; i > 0; i--)
  {  
    sumBpsT += bpsLog[i];
    bpsLog[i] = bpsLog[i-1];  
  }
  
  bpsLog[0] = aktBpsDeltaT;
  aktBps = 5 * 1000 / sumBpsT;
 
}

float CalcJoule(float meterProSekunde)
{
  float joule = 0.0032 * pow(meterProSekunde,2) / 1 / 2;
  if (joule>100) 
  {
    joule=0;
  }
  
  return joule;
}



void calcMittelwert()
{   
    for (int i = 0; i < anzahlWerteImArray; i++) 
    {
        mittelwert += fpsLog[i];       
    }    
    mittelwert /= anzahlWerteImArray;    

}

void calcVarianz()
{
    for (int i = 0; i < anzahlWerteImArray; i++) 
    {   
        varianz += (fpsLog[i] - mittelwert) * (fpsLog[i] - mittelwert);  //quadrat
    }
    varianz /= ( anzahlWerteImArray-1 );  

}

void calcStandardabweichung()
{
  sigma = sqrt( varianz );


  // sigmaMax = max(sigma, sigmaMax);

    //lcd.setCursor(17,3);
    //lcd.print(sigma);
}

void resetFPS()
{
  memset(&fpsLog[0], 0, sizeof(fpsLog));
  anzahlWerteImArray = 0;
  fpsPointerNextWertImArray = 0;  
}

void RecalcAbweichung()
{  calcVarianz();
  calcStandardabweichung();
}



void MessungFps()
{    
  Dauer=Zeit2-Zeit1;  

  if (Dauer <= 0)
  {
    fps = 0;
    return;
  }
    
  float vAkt = 3600.0*l/Dauer/1.075;
 
  float meterProSekunde = vAkt * 0.277778;  
    
  fps = vAkt * 0.911344;    
  
  fpsLog[fpsPointerNextWertImArray] = fps;
  fpsPointerNextWertImArray += 1;
  if (fpsPointerNextWertImArray == maxAnzahlWerteImArray)
  {
    fpsPointerNextWertImArray = 0; //Wir fangen vorne an!
  } 
  shotCount += 1;

  if (anzahlWerteImArray < maxAnzahlWerteImArray) //wenn wir einmal den Array gefüllt haben, laufen wir ihn immer komplett durch
  {
    anzahlWerteImArray += 1;
  }
  
  if (fps < fpsMin)
  {
    fpsMin = fps;
  }
  
  if (fps > fpsMax)
  {
    fpsMax = fps;
  }
        
  float joule = CalcJoule(meterProSekunde);    
}

volatile int resetCounter = 10;

const int ICON_LICHTSCHRANK_AN = 1;
const int ICON_LICHTSCHRANK_AUS = 2;
const int ICON_BATTERIE_8 = 3;
const int ICON_BATTERIE_7 = 4;
const int ICON_BATTERIE_6 = 5;
const int ICON_BATTERIE_5 = 6;
const int ICON_BATTERIE_4 = 7;
const int ICON_BATTERIE_3 = 8; 

void SetupSonderzeichen()
 {
  byte an[8] = {
    B00000,
    B00000,
    B01110,
    B01110,
    B01110,
    B01110,
    B00000,
    B00000,
    };
  lcd.createChar(ICON_LICHTSCHRANK_AN , an);
  
  byte aus[8] = {
    B00000,
    B11111,
    B10001,
    B10001,
    B10001,
    B10001,
    B11111,
    B00000,
    };
    lcd.createChar(ICON_LICHTSCHRANK_AUS , aus);
   
  byte batlevel8[8] = {
    B01110,
    B11111,
    B11111,
    B11111,
    B11111,
    B11111,
    B11111,
    B00000,
    };
    lcd.createChar(ICON_BATTERIE_8 , batlevel8);
  
  byte batlevel7[8] = {
    B01110,
    B10001,
    B11111,
    B11111,
    B11111,
    B11111,
    B11111,
    B00000,
    };
    lcd.createChar(ICON_BATTERIE_7 , batlevel7);
  
  byte batlevel6[8] = {
    B01110,
    B10001,
    B10001,
    B11111,
    B11111,
    B11111,
    B11111,
    B00000,
    };
    lcd.createChar(ICON_BATTERIE_6 , batlevel6);
  
  byte batlevel5[8] = {
    B01110,
    B10001,
    B10001,
    B10001,
    B11111,
    B11111,
    B11111,
    B00000,
    };
    lcd.createChar(ICON_BATTERIE_5 , batlevel5); 
  
   byte batlevel4[8] = {
    B01110,
    B10001,
    B10001,
    B10001,
    B10001,
    B11111,
    B11111,
    B00000,
    };
    lcd.createChar(ICON_BATTERIE_4 , batlevel4);  
  
   byte batlevel3[8] = {
    B01110,
    B10001,
    B10001,
    B10001,
    B10001,
    B10001,
    B11111,
    B00000,
    };
    lcd.createChar(ICON_BATTERIE_3 , batlevel3);       
  
 }

void Batterie() 
{     
  lcd.setCursor(19,0);
 
  int BattValue = analogRead(A0);  
  float voltage = BattValue * (5.0 / 1023.0) * 2 ;  // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 10V):
 
  if(voltage > 9.0)
  {        
    lcd.write(byte(ICON_BATTERIE_8));
  }
  else if(voltage > 8)
  {     
    lcd.write(byte(ICON_BATTERIE_7));
  }
  else if(voltage > 7.5)
  {        
    lcd.write(byte(ICON_BATTERIE_6));
  }
  else if(voltage > 7)
  {      
    lcd.write(byte(ICON_BATTERIE_5));
  }
  else if(voltage > 6.5)
  {   
    lcd.write(byte(ICON_BATTERIE_4));
  }
  else if(voltage > 6)
  {
   lcd.write(byte(ICON_BATTERIE_3));
  } 
  else
  {
    lcd.setCursor(17,0);
    //lcd.print(voltage,1);
    lcd.print("low");
  }
} 


void CheckLEDs()
{    
  int state1 = digitalRead(2);     
  int state2 = digitalRead(3);    
  lcd.setCursor(15,0);
  if (state1 == 0)
  {
    lcd.write(byte(ICON_LICHTSCHRANK_AN));  
  }
  else
  {
    lcd.write(byte(ICON_LICHTSCHRANK_AUS));  
  }
  
  lcd.setCursor(16,0);
  if (state2 == 0)
  {
    lcd.write(byte(ICON_LICHTSCHRANK_AN));  
  }
  else
  {
    lcd.write(byte(ICON_LICHTSCHRANK_AUS));  
  }
}

void UpdateOLED () { 
   
  u8g.setFont(u8g_font_gdb12);

  u8g.setPrintPos(10, 15);
  u8g.print("fps"); 
  u8g.setPrintPos(85, 15);
  u8g.print(fps,0); 

  u8g.setPrintPos(10, 38);
  u8g.print("fps +/-"); 
  u8g.setPrintPos(85, 38);
  u8g.print(sigma); 

  u8g.setPrintPos(10, 61);
  u8g.print("bps"); 
  u8g.setPrintPos(85, 61);
  u8g.print(maxBps,1); 
}


void loop() {
  
  CheckLEDs();

  UpdateBps();
  MessungFps();
  UpdateFps();  
  Batterie();

  u8g.firstPage();  
  do 
  {
    UpdateOLED();
  } while( u8g.nextPage() );
         
  if (delaySwitch == 0)
  {
    delaySwitch = 1;
    resetCounter-= 1;
    
    if (resetCounter == 0)
    {
      maxBps = 0;
      aktBps = 0;
      resetFPS();
      resetCounter = 10;
    }  
  }
  else
  {   
    RecalcAbweichung();
    delaySwitch = 0;
  }
  delay(500); 
}


void Starten() {    
  AddBpS();
  resetCounter = 10;
  Zeit1=micros();
  shotCounter++;  
 }

void Stoppen() {    
  Zeit2=micros();    
}
