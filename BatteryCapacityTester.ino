////////////////////////////////////////////////////////////////
// Lithium Ion Battery Capacity Tester
// Tests up to four 18650 Li-Ion 3,6V batteries
// Oct 21, 2015  - Patrick Wels
///////////////////////////////////////////////////////////////

#include <PCD8544.h>        // library of functions for Nokia LCD (http://code.google.com/p/pcd8544/)
#include <EEPROMex.h>
#include <EEPROMVar.h>

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

#define MAX_BATTERIES	     1     //count of batteries to test
#define MIN_VOLTAGE       2800
#define RESISTOR_VALUE     3.9

#define controlButtonPin    13

struct batteryStruct
{
    unsigned long  charge;         // Total microamp hours for this battery
    boolean done;                  // set this to DONE when this cell's test is complete
    byte batteryVoltagePin;        // Analog sensor pin (0-5) for reading battery voltage
    byte batteryAmpPin;            // Analog sensor pin (0-5) to read voltage across FET
    byte dischargeControlPin;      // Output Pin that controlls the load for this battery
    unsigned long StartTime;       // Start time reading (in milliseconds)
    unsigned long PrevTime;        // Previous time reading (in milliseconds)
    int calibrationVoltageAddress; // Address in EEPROM of voltage correction factor
    int calibrationCurrentAddress;  // Address in EEPROM of ampere correction factor
    float resistance;              // internal battery resistance
    bool started;
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
float getVolt(unsigned int numBattery);

void setup() {
  static unsigned int  batteryNum;
  
    Serial.begin(115200);
  
  for (batteryNum=0 ; batteryNum < MAX_BATTERIES ; batteryNum++)
  {
    battery[batteryNum].dischargeControlPin = 12 - batteryNum;
    battery[batteryNum].batteryVoltagePin = batteryNum;
    
    pinMode(battery[batteryNum].dischargeControlPin, OUTPUT);
    pinMode(controlButtonPin, INPUT);
    battery[batteryNum].done = false;
    battery[batteryNum].PrevTime = 0;
    battery[batteryNum].charge = 0;
    battery[batteryNum].resistance = 0;
    battery[batteryNum].started = false;
    
    battery[batteryNum].calibrationVoltageAddress = 0 + 2 * ( batteryNum * sizeof(float) );
    battery[batteryNum].calibrationCurrentAddress = sizeof(float) + 2 * ( batteryNum * sizeof(float) );
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
   
// ----------  setup mode block start  ----------------------------------------------------------------
   if (digitalRead(controlButtonPin)) {
     bool ampere = false;  // ampere or voltage calibartion
     int batteryNum = 0;   // battery index
     float readf;
     lcd.clear();
     lcd.setCursor(0, LCD_LINE1);
     lcd.print("  Setup   ");
     delay(1000);
     
     while(batteryNum < MAX_BATTERIES) {
       lcd.setCursor(0, LCD_LINE2);
       
       if ( ampere ) { // ampere calibration mode
         digitalWrite(battery[batteryNum].dischargeControlPin, 1);
         lcd.print("put 1A to");
         lcd.setCursor(0, LCD_LINE3); lcd.print("Battery #"); lcd.print(batteryNum + 1);
         readf = getAmps(battery[batteryNum].batteryVoltagePin);
         EEPROM.updateFloat(battery[batteryNum].calibrationCurrentAddress, readf);
           
         ClearDisplayLine(LCD_LINE5);
         lcd.print("Current: ");
         ClearDisplayLine(LCD_LINE6); lcd.print(readf, 2); lcd.print(" mA");
         
         Serial.print("calibration current on address: "); Serial.print(battery[batteryNum].calibrationCurrentAddress); Serial.print(": "); Serial.println(EEPROM.readFloat(battery[batteryNum].calibrationCurrentAddress));
         
         Serial.print("calibration voltage on address "); Serial.print(battery[batteryNum].calibrationVoltageAddress); Serial.print(": "); Serial.println(EEPROM.readFloat(battery[batteryNum].calibrationVoltageAddress));
         
         // switch battery on button pressed
         if ( digitalRead(controlButtonPin) ) {
           batteryNum++;
           ampere = false;
           digitalWrite(battery[batteryNum].dischargeControlPin, 0);
           delay(1000);
         }
         
       } else { // voltage calibration mode
         readf = getVolt(battery[batteryNum].batteryVoltagePin);
         lcd.print("put 4V to");
         lcd.setCursor(0, LCD_LINE3); lcd.print("Battery #"); lcd.print(batteryNum + 1);
         readf = getVolt(battery[batteryNum].batteryVoltagePin) / 4;
         EEPROM.updateFloat(battery[batteryNum].calibrationVoltageAddress, readf);
         
         ClearDisplayLine(LCD_LINE5);
         lcd.print("Voltage: ");
         ClearDisplayLine(LCD_LINE6); lcd.print(readf * 4, 2); lcd.print(" mV");
         
         Serial.print("calibration voltage on address "); Serial.print(battery[batteryNum].calibrationVoltageAddress); Serial.print(": "); Serial.println(EEPROM.readFloat(battery[batteryNum].calibrationVoltageAddress));
         
         // switch mode on button pressed
         if ( digitalRead(controlButtonPin) ) {
           ampere = true;
           delay(1000);
         }
       }
       delay(1000);
     }
   }
// ----------  setup mode block end  ----------------------------------------------------------------
  
   lcd.clear();        // clear the display 
   lcd.setCursor(0, LCD_LINE1);
   lcd.print(" ** LI-ION ** "); 
   lcd.setCursor(0, LCD_LINE2);
   lcd.print("Volt  mAh  min");
}

void loop() {
  unsigned int  batteryNum, line;
  unsigned int minuten, sekunden;
  unsigned long duration, currentTime;
  float battVoltage, battCurrent;
  char buf[6];
  float calibrationVoltage = EEPROM.readFloat(battery[batteryNum].calibrationVoltageAddress);
  float calibrationCurrent = EEPROM.readFloat(battery[batteryNum].calibrationCurrentAddress);
  
  Serial.print("calibrationVoltage: "); Serial.println(calibrationVoltage);
  Serial.print("calibrationCurrent: "); Serial.println(calibrationCurrent);
  
  for ( batteryNum=0 ; batteryNum < MAX_BATTERIES ; batteryNum++ )
  {
    battVoltage = ( getVolt(batteryNum) * 1000 ) / calibrationVoltage;
    Serial.print("Voltage: "); Serial.println(battVoltage);
    
    line = LCD_LINE3 + batteryNum;
    ClearDisplayLine(line);
    
    // no battery
    if (battVoltage < 500)
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
        if (battery[batteryNum].started) {
          battery[batteryNum].resistance = ( battery[batteryNum].resistance - battVoltage ) / RESISTOR_VALUE;   
          ClearDisplayLine(line);
          lcd.print(battery[batteryNum].resistance, 2); lcd.print("Ohm");
          // TODO change to step counter - no delay
          delay(3000);
          battery[batteryNum].started = false;
        }
        if (battery[batteryNum].charge == 0)
        {
          battery[batteryNum].StartTime = millis();
          battery[batteryNum].charge = 1;
          battery[batteryNum].resistance = battVoltage;
          battery[batteryNum].started = true;
          
          delay(2000);
        }
        
        digitalWrite(battery[batteryNum].dischargeControlPin, 1);
        
        battCurrent = ( getAmps(batteryNum) * 1000 ) / calibrationCurrent;
        currentTime = millis() - battery[batteryNum].StartTime;
        duration = (currentTime - battery[batteryNum].PrevTime);
        battery[batteryNum].PrevTime = currentTime;
        battery[batteryNum].charge += (battCurrent * duration) / 3600;
        
        Serial.print(" - Current: "); Serial.print(battCurrent); Serial.println(" mA");
        
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
        
        //lcd.print(((float)battVoltage) / 1000, 2);
        
        
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
float getVolt(unsigned int numBattery)
{
	return map( analogRead(battery[numBattery].batteryVoltagePin), 0, 1023, 0, 5000 );
}

// get battery ampere in mA
float getAmps(unsigned int numBattery)
{
	return ( map( analogRead(battery[numBattery].batteryVoltagePin), 0, 1023, 0, 5000 ) / RESISTOR_VALUE );
}

