////////////////////////////////////////////////////////////////
// Lithium Ion Battery Capacity Tester
// Tests up to 8 18650 Li-Ion batteries
// Feb 11, 2016  - Patrick Wels
///////////////////////////////////////////////////////////////

#include <PCD8544.h>        // library of functions for Nokia LCD (http://code.google.com/p/pcd8544/)
#include <EEPROMVar.h>
#include <avr/eeprom.h>

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

#define MAX_BATTERIES	     8     //count of batteries to test
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


static PCD8544 lcd(1, 0, 2, 3, 4);

// Bitmaps for Battery with an X (no battery)
static const byte noBatteryIcon [] = { 0x1C, 0x14, 0x77, 0x41, 0x41,
           0x41,0x63,0x77,0x5D,0x5D,0x77,0x63,0x41,0x41,0x41,0x7f};

//Prototypes for utility functions
void printRightJustifiedUint(unsigned int n, unsigned short numDigits);
void ClearDisplayLine(int line);   // function to clear specified line of LCD display
float getVolt(unsigned int numBattery);

void setup() {
  static unsigned int  batteryNum;
  
  // stop serial to use D0 and D1 as digital output
  Serial.end();
  
  for (batteryNum=0 ; batteryNum < MAX_BATTERIES ; batteryNum++)
  {
    battery[batteryNum].dischargeControlPin = 12 - batteryNum;
    battery[batteryNum].batteryVoltagePin = batteryNum;
    
    pinMode(battery[batteryNum].dischargeControlPin, OUTPUT);
    battery[batteryNum].done = false;
    battery[batteryNum].PrevTime = 0;
    battery[batteryNum].charge = 0;
    battery[batteryNum].resistance = 0;
    battery[batteryNum].started = false;
    
    battery[batteryNum].calibrationVoltageAddress = 0 + ( 2 * ( batteryNum * sizeof(float) ) );
    battery[batteryNum].calibrationCurrentAddress = sizeof(float) + ( 2 * ( batteryNum * sizeof(float) ) );
  }
  pinMode(controlButtonPin, INPUT);
   
   lcd.begin(LCD_WIDTH, LCD_HEIGHT);// set up the LCD's dimensions in pixels
   lcd.setContrast(55);
   
   lcd.setCursor(0, LCD_LINE1);
   lcd.print(" Rechargeable");
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
         readf = getAmps(battery[batteryNum].batteryVoltagePin);
         lcd.print("put 1A to");
         lcd.setCursor(0, LCD_LINE3); lcd.print("Battery #"); lcd.print(batteryNum + 1);
         //EEPROM.updateFloat(battery[batteryNum].calibrationCurrentAddress, readf);
         eeprom_write_float((float *)battery[batteryNum].calibrationCurrentAddress, readf);
         ClearDisplayLine(LCD_LINE5);
         //lcd.print("Current: ");
         lcd.print(battery[batteryNum].calibrationCurrentAddress);
         lcd.print(" - ");
         lcd.print(eeprom_read_float((float *)(battery[batteryNum].calibrationCurrentAddress)), 2);
         ClearDisplayLine(LCD_LINE6);
         lcd.print(readf, 2); lcd.print(" mA");
         
         // switch battery on button pressed
         if ( digitalRead(controlButtonPin) ) {
           digitalWrite(battery[batteryNum].dischargeControlPin, 0);
           batteryNum++;
           ampere = false;
           //delay(1000);
         }
         
       } else { // voltage calibration mode
         readf = getVolt(battery[batteryNum].batteryVoltagePin) / 4;
         lcd.print("put 4V to");
         lcd.setCursor(0, LCD_LINE3); lcd.print("Battery #"); lcd.print(batteryNum + 1);
         //EEPROM.updateFloat(battery[batteryNum].calibrationVoltageAddress, readf);
         eeprom_write_float((float *)battery[batteryNum].calibrationVoltageAddress, readf);
         
         ClearDisplayLine(LCD_LINE5);
         //lcd.print("Voltage: ");
         lcd.print(battery[batteryNum].calibrationVoltageAddress);
         lcd.print(" - ");
         lcd.print(eeprom_read_float((float *)(battery[batteryNum].calibrationVoltageAddress)), 2);
         ClearDisplayLine(LCD_LINE6); 
         lcd.print(readf * 4, 2); lcd.print(" mV");
         
         // switch mode on button pressed
         if ( digitalRead(controlButtonPin) ) {
           ampere = true;
         }
       }
       delay(1000);
     }
   }
// ----------  setup mode block end  ----------------------------------------------------------------
  
   lcd.clear();
   lcd.setCursor(0, LCD_LINE1);
   lcd.print(" ** LI-ION ** "); 
   lcd.setCursor(0, LCD_LINE2);
   lcd.print(" Volt mAh  min");
}


bool page = 0;

void loop() {
  unsigned int  batteryNum, line;
  unsigned long duration, currentTime;
  float battVoltage, battCurrent;
  char buf[20];
  
  // measure all battery slots
  for ( batteryNum=0 ; batteryNum < MAX_BATTERIES; batteryNum++ )
  {
    // set calibration Values for slot from EEPROM
    float calibrationVoltage = eeprom_read_float((float *)(battery[batteryNum].calibrationVoltageAddress));
    float calibrationCurrent = eeprom_read_float((float *)(battery[batteryNum].calibrationCurrentAddress));
    
    // read battery voltage
    battVoltage = ( getVolt(batteryNum) * 1000 ) / calibrationVoltage;
    
    bool show = ( !page && batteryNum < 4 ) || ( page && batteryNum >= 4 );
    
    // print on lcd
    if (show) {
      line = LCD_LINE3 + batteryNum % 4;
      ClearDisplayLine(line);
      lcd.print(batteryNum + 1);
      lcd.print(" ");
    }
    
    if (battVoltage < 500) // no battery
    {
      battery[batteryNum].done = false;
      digitalWrite(battery[batteryNum].dischargeControlPin, 0);
      battery[batteryNum].charge = 0;
      battery[batteryNum].PrevTime = 0;
      
      if (show) {
        lcd.setCursor(BATTERY_ICON_HORIZ , line);
        lcd.drawBitmap(noBatteryIcon, sizeof(noBatteryIcon), 1);
      }
    }
    else // battery inserted
    {
      if (battVoltage > MIN_VOLTAGE && !battery[batteryNum].done) // battery not empty or done
      {
        if (show) {
          lcd.print(battVoltage / 1000, 1);
        }
        
        if (battery[batteryNum].started) // measure internal battery resistance after 1 sec of load
        {
          battery[batteryNum].resistance = ( battery[batteryNum].resistance - battVoltage ) / RESISTOR_VALUE;
          battery[batteryNum].started = false;
        }
        
        if (!battery[batteryNum].charge) // new battery plugged in
        {
          battery[batteryNum].StartTime = millis();
          battery[batteryNum].charge = 1;
          battery[batteryNum].resistance = battVoltage;
          battery[batteryNum].started = true;
          delay(2000); // give time to read idle battery voltage
        }
        
        digitalWrite(battery[batteryNum].dischargeControlPin, 1);
        
        battCurrent = ( getAmps(batteryNum) * 1000 ) / calibrationCurrent;
        currentTime = millis() - battery[batteryNum].StartTime;
        duration = (currentTime - battery[batteryNum].PrevTime);
        battery[batteryNum].PrevTime = currentTime;
        battery[batteryNum].charge += (battCurrent * duration) / 3600;
        
        if (show) {
          sprintf(buf, " %4lu %3u", battery[batteryNum].charge / 1000, currentTime / 60000);
          lcd.print(buf);
        }
      }
      else // discharge of battery done
      {
        battery[batteryNum].done = true;
        digitalWrite(battery[batteryNum].dischargeControlPin, 0);
        
        if (show) {
          sprintf(buf, "%lu R:", battery[batteryNum].charge / 1000);
          lcd.print(buf);
          lcd.print(battery[batteryNum].resistance, 1);
        }
      }
    }
  }
  
  delay(1000);
  
  if ( digitalRead(controlButtonPin) ) { // button pushed -> switch to other page
    page = !page;
  }
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

