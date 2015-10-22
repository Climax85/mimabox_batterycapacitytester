////////////////////////////////////////////////////////////////
// Lithium Ion Battery Capacity Tester
// Tests up to four 18650 Li-Ion 3,6V batteries
// Oct 21, 2015  - Patrick Wels
///////////////////////////////////////////////////////////////

#include <PCD8544.h>        // library of functions for Nokia LCD (http://code.google.com/p/pcd8544/)

#define LCD_TEXT_WIDTH      14   // Width of text line for Nokia LCD
#define LCD_LINE1            0
#define LCD_LINE2            1
#define LCD_LINE3            2
#define LCD_LINE4            3
#define LCD_LINE5            4
#define LCD_LINE6            5

#define LCD_WIDTH           84     // The dimensions of the Nokia LCD(in pixels)...
#define LCD_HEIGHT          48
#define BAT_WIDTH           16     // Width of battery icon in pixels
#define BAT_HEIGHT           1     // Height of battery Icon (1 line = 8 pixels)

#define BATTERY_ICON_HORIZ  33     // Horizontal position of battery icon (in pixels)
#define MAH_HORIZ_POSITION  33     // Horizontal position of mAh display
#define T_HORIZ_POSITION    66     // Horizontal position of mAh display

#define MAX_BATTERIES	     4          //count of batteries to test
#define MIN_VOLTAGE       2800
#define REFERENCE_VOLTAGE 5010

struct batteryStruct
{
    unsigned long  charge;         // Total microamp hours for this battery
    boolean done;                  // set this to DONE when this cell's test is complete
    byte batteryVoltagePin;        // Analog sensor pin (0-5) for reading battery voltage
    byte batteryAmpPin;            // Analog sensor pin (0-5) to read voltage across FET
    byte dischargeControlPin;      // Output Pin that controlls the load for this battery
    unsigned long StartTime;       // Start time reading (in milliseconds)
    unsigned long PrevTime;        // Previous time reading (in milliseconds)
}battery[MAX_BATTERIES];


static PCD8544 lcd(6, 5, 4, 3, 2);

// Bitmaps for battery icons, Full, Empty, and Battery with an X (no battery)
static const byte batteryEmptyIcon[] ={ 0x1c, 0x14, 0x77,0x41,0x41,
           0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x7f};
static const byte batteryFullIcon[] = { 0x1c, 0x14, 0x77,0x7f,0x7f,
           0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f};
static const byte noBatteryIcon [] = { 0x1C, 0x14, 0x77, 0x41, 0x41,
           0x41,0x63,0x77,0x5D,0x5D,0x77,0x63,0x41,0x41,0x41,0x7f};
           
static const byte progressIndicator[] = "-\0|/";
static const byte backslashNokia[] =    //define backslach LCD character for Nokia5110 (PCD8544)
{
   B0000010,
   B0000100,
   B0001000,
   B0010000,
   B0100000
};

//Prototypes for utility functions
void printRightJustifiedUint(unsigned int n, unsigned short numDigits);
void ClearDisplayLine(int line);   // function to clear specified line of LCD display
int getVolt(unsigned int numBattery);
int getLoadVolt(unsigned int numBattery);


void setup() {
  static unsigned int  batteryNum;
  
  for (batteryNum=0 ; batteryNum < MAX_BATTERIES ; batteryNum++)
  {
    battery[batteryNum].dischargeControlPin = 12 - batteryNum;
    battery[batteryNum].batteryVoltagePin = 0 + (2 * batteryNum);
    battery[batteryNum].batteryAmpPin = 1 + (2 * batteryNum);
    
    pinMode(battery[batteryNum].dischargeControlPin, OUTPUT);
    battery[batteryNum].done = false;
    battery[batteryNum].PrevTime = 0;
    battery[batteryNum].charge = 0;
  }
  
   lcd.begin(LCD_WIDTH, LCD_HEIGHT);// set up the LCD's dimensions in pixels
   // Register the backslash character as a special character
   // since the standard library doesn't have one
   lcd.createChar(0, backslashNokia);
   lcd.setContrast(55);
   
   lcd.setCursor(0, LCD_LINE1);// First Character, First line
   lcd.print(" Rechargeable");  // Print a message to the LCD.
   lcd.setCursor(0, LCD_LINE3);
   lcd.print("   Battery    "); 
   lcd.setCursor(0, LCD_LINE5);   
   lcd.print("    Tester    "); 
  
   delay(3000);
  
   lcd.clear();        // clear the display 
   lcd.print(" ** LI-ION ** "); 
   lcd.setCursor(0, LCD_LINE2);//  set cursor to 3rd line 
   lcd.print("Volt  mAh  min");
}

void loop() {
  unsigned int  batteryNum, battVoltage, loadVoltage, loadCurrent, line;
  unsigned int minuten, sekunden;
  unsigned long duration, currentTime;
  char buf[6];
  
  for (batteryNum=0 ; batteryNum < MAX_BATTERIES ; batteryNum++)
  {
    battVoltage = getVolt(batteryNum);
    
    line = LCD_LINE3 + batteryNum;
    ClearDisplayLine(line);
    lcd.setCursor(0, line);
    
    if (battVoltage < 100)
    {
      battery[batteryNum].done = false;
      digitalWrite(battery[batteryNum].dischargeControlPin, 0);
      battery[batteryNum].charge = 0;
      battery[batteryNum].PrevTime = 0;  
      
      lcd.setCursor(BATTERY_ICON_HORIZ , line);
      lcd.drawBitmap(noBatteryIcon, sizeof(noBatteryIcon), 1);
    }
    else
    {
      lcd.print(((float)battVoltage) / 1000, 2);
      if (battVoltage > MIN_VOLTAGE && !battery[batteryNum].done)
      {
        if (battery[batteryNum].charge == 0)
        {
          battery[batteryNum].StartTime = millis();
          battery[batteryNum].charge = 1;
          delay(2000);
        }
        digitalWrite(battery[batteryNum].dischargeControlPin, 1);
        loadVoltage = getLoadVolt(batteryNum);
        loadCurrent = ((battVoltage-loadVoltage)*10) / 39;
        currentTime = millis() - battery[batteryNum].StartTime;
        duration = (currentTime - battery[batteryNum].PrevTime);
        battery[batteryNum].PrevTime = currentTime;
        battery[batteryNum].charge += (loadCurrent * duration) / 3600;
        
        lcd.setCursor(MAH_HORIZ_POSITION, line);
        sprintf(buf, "%4lu", battery[batteryNum].charge / 1000);
        lcd.print(buf);
        
        lcd.setCursor(T_HORIZ_POSITION, line);
        minuten = currentTime / 60000;
        //sekunden = (currentTime % 60000) / 1000;
        //sprintf(buf, "%u:%2u", minuten, sekunden);
        sprintf(buf, "%3u", minuten);
        lcd.print(buf);
      }
      else
      {
        battery[batteryNum].done = true;
        digitalWrite(battery[batteryNum].dischargeControlPin, 0);
        
        lcd.print(((float)battVoltage) / 1000, 2);
        
        
        lcd.setCursor(MAH_HORIZ_POSITION, line);
        sprintf(buf, "%4lu", battery[batteryNum].charge / 1000);
        lcd.print(buf);
        
        lcd.setCursor(T_HORIZ_POSITION, line);
        lcd.print("RDY");
      }
    }
  }
  delay(1000);
}


// clear spezified line
void ClearDisplayLine(int line) 
{
  unsigned int i;
  lcd.setCursor(0, line);  // put cursor on first char of specified line
  lcd.clearLine();
  lcd.home();
}

// get battery volts in mV
int getVolt(unsigned int numBattery)
{
	return (map(analogRead(battery[numBattery].batteryVoltagePin),0,1023,0,REFERENCE_VOLTAGE));
}

// get load volts in mV
int getLoadVolt(unsigned int numBattery)
{
	return (map(analogRead(battery[numBattery].batteryAmpPin),0,1023,0,REFERENCE_VOLTAGE));
}

