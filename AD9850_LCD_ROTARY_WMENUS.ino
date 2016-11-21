/*
Main code by Richard Visokey AD7C - www.ad7c.com
Revision 2.0 - November 6th, 2013

All changes and developments by Paolo Cravero IK1ZYW

*/

// Include the library code
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <LiquidCrystal.h>
#include <rotary.h>
#include <EEPROM.h>

//Setup some items
#define W_CLK 9   // Pin 9 - connect to AD9850 module word load clock pin (CLK)
#define FQ_UD 8   // Pin 8 - connect to freq update pin (FQ)
#define DATA 7   // Pin 7 - connect to serial data load pin (DATA)
#define RESET 6  // Pin 6 - connect to reset pin (RST) 
#define pulseHigh(pin) {digitalWrite(pin, HIGH); digitalWrite(pin, LOW); }
//Rotary r = Rotary(2,3); // sets the pins the rotary encoder uses.  Must be interrupt pins.
Rotary r = Rotary(3,2); // sets the pins the rotary encoder uses.  Must be interrupt pins.
//LiquidCrystal lcd(12, 13, 7, 6, 5, 4); // I used an odd pin combination because I need pin 2 and 3 for the interrupts.
LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x20 for a 16 chars and 2 line display


uint_fast32_t rx=7200000; // Starting frequency of VFO
uint_fast32_t rx2=1; // variable to hold the updated frequency

uint_fast32_t scanlow=14000000;  // just two defaults for band scan limits
uint_fast32_t scanhigh=14350000; //  " "          " "            " "

uint_fast32_t increment = 100; // starting VFO update increment in Hz.
int buttonstate = 0;
int inscan = 0; // 0 not scanning, 1 scan in progress
String hertz = "100 Hz";
int  hertzPosition = 5;
byte ones,tens,hundreds,thousands,tenthousands,hundredthousands,millions ;  //Placeholders
String freq; // string to hold the frequency
uint_fast32_t timepassed = millis(); // int to hold the arduino miilis since startup
int memstatus = 1;  // value to notify if memory is current or old. 0=old, 1=current.
int menupos = 0;





int ForceFreq = 0;  // Change this to 0 after you upload and run a working sketch to activate the EEPROM memory.  YOU MUST PUT THIS BACK TO 0 AND UPLOAD THE SKETCH AGAIN AFTER STARTING FREQUENCY IS SET!




void setup() {
  // prepare CONFIRM selection button
  pinMode(A0,INPUT_PULLUP); // Connect to a button that goes to GND on push
  //  digitalWrite(A0,HIGH);

  // prepare SCAN selection button
  // pinMode(A1,INPUT_PULLUP); // Connect to a button that goes to GND on push
  //  digitalWrite(A1,HIGH);

  lcd.init();
  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();
  pinMode(FQ_UD, OUTPUT);
  pinMode(W_CLK, OUTPUT);
  pinMode(DATA, OUTPUT);
  pinMode(RESET, OUTPUT); 
  pulseHigh(RESET);
  pulseHigh(W_CLK);
  pulseHigh(FQ_UD);  // this pulse enables serial mode on the AD9850 - Datasheet page 12.
  

  lcd.backlight();
  lcd.setCursor(hertzPosition,1);    
  lcd.print(hertz);
   // Load the stored frequency  
  if (ForceFreq == 0) {
    freq = String(EEPROM.read(0))+String(EEPROM.read(1))+String(EEPROM.read(2))+String(EEPROM.read(3))+String(EEPROM.read(4))+String(EEPROM.read(5))+String(EEPROM.read(6));
    rx = freq.toInt();  
    lcd.setCursor(15,0);
    lcd.print("M");
  }
}


void loop() {
  if (rx != rx2){    
        showFreq();
        sendFrequency(rx);
        rx2 = rx;
      }
      
  menupos = analogRead(1); // pin A1
  menupos = map(menupos, 0, 1023, 0, 5);
  
  /// TODO, switch SELECT to update display with current selection
  lcd.setCursor(0,1);
  switch (menupos) {
    case 0:
      lcd.print("Step: ");
      lcd.print(hertz); 
      lcd.print("    ");
      break;
    case 1:
      lcd.print("Scan start/stop ");
      break;
    case 2:
      lcd.print("Set scan LO lim.");
      break;
    case 3:
      lcd.print("Set scan HI lim.");
      break;
    case 4:
      lcd.print("AD9850DDS IK1ZYW");
      break;
    default:
      break;
  }
  
  buttonstate = digitalRead(A0);
  if(buttonstate == LOW) {
    delay(100);
    if(buttonstate == LOW) { // debounce 
    switch (menupos) {
      case 0:
        setincrement();        
        break;
      case 1:
        doScan();
        break;
      case 2:
        saveLimit();
        break;
      case 3:
        saveLimit();
        break;
      default:
        break;
        // do nothing yet
    } // end debounce
    }
  };

    
  // Write the frequency to memory if not stored and 10 seconds have passed since the last frequency change.
    if(memstatus == 0){   
      lcd.setCursor(15,0);
      lcd.print(" ");
      if(timepassed+10000 < millis()){
        storeMEM();
        }
      }   
}


ISR(PCINT2_vect) {
  unsigned char result = r.process();
  if (result) {    
    if (result == DIR_CW){rx=rx+increment;}
    else {rx=rx-increment;};       
      if (rx >=60000000){rx=rx2;}; // UPPER VFO LIMIT 60 MHz
      if (rx <=100000){rx=rx2;}; // LOWER VFO LIMIT 100 kHz
  }
}


/* SET SCANNING LIMITS FUNCTION. Reads also menupos value */

void saveLimit() {
  delay(300);
  lcd.setCursor(0,1);
  switch (menupos) {
      case 2: // lower limit
        scanlow = rx;
        lcd.print("LO");
        break;
      case 3: // higher limit
        scanhigh = rx;
        lcd.print("HI");
        break;
      default:
        break;
  }
  lcd.print(" limit saved. ");
  delay(1000);
}


/* SCANNING FUNCTION, between scanlow and scanhigh frequencies */
void doScan() {
  delay(300);
  lcd.setCursor(0,1);
  lcd.print("  Scanning...   ");
  if (rx <= scanlow){rx=scanlow;};
  if (rx >= scanhigh){rx=scanlow;}; 
  
  rx = scanlow;
  do {
    showFreq();
    sendFrequency(rx);
    rx = rx + increment;
    if (rx >= scanhigh){rx=scanlow;}; // loop back to beginning

    menupos = analogRead(1); // pin A1  
    delay(menupos); // set scanning speed according to menu pot position
    
    buttonstate = digitalRead(A0); // check for end-of-scan command
  } while (buttonstate == HIGH);
  
  rx2 = rx;
  
  delay(100); // 100 milliseconds delay at each step
}

// frequency calc from datasheet page 8 = <sys clock> * <frequency tuning word>/2^32
void sendFrequency(double frequency) {  
  int32_t freq = frequency * 4294967295/125000694;  // note 125 MHz clock on 9850.  You can make 'slight' tuning variations here by adjusting the clock frequency.
  for (int b=0; b<4; b++, freq>>=8) {
    tfr_byte(freq & 0xFF);
  }
  tfr_byte(0x000);   // Final control byte, all 0 for 9850 chip
  pulseHigh(FQ_UD);  // Done!  Should see output
}
// transfers a byte, a bit at a time, LSB first to the 9850 via serial DATA line
void tfr_byte(byte data)
{
  for (int i=0; i<8; i++, data>>=1) {
    digitalWrite(DATA, data & 0x01);
    pulseHigh(W_CLK);   //after each bit sent, CLK is pulsed high
  }
}

void setincrement(){
  if(increment == 10){increment = 50; hertz = "50 Hz"; hertzPosition=5;}
  else if (increment == 50){increment = 100;  hertz = "100 Hz"; hertzPosition=4;}
  else if (increment == 100){increment = 500; hertz="500 Hz"; hertzPosition=4;}
  else if (increment == 500){increment = 1000; hertz="1 kHz"; hertzPosition=6;}
  else if (increment == 1000){increment = 2500; hertz="2.5 kHz"; hertzPosition=4;}
  else if (increment == 2500){increment = 5000; hertz="5 kHz"; hertzPosition=6;}
  else if (increment == 5000){increment = 10000; hertz="10 kHz"; hertzPosition=5;}
  else if (increment == 10000){increment = 100000; hertz="100 kHz"; hertzPosition=4;}
  else if (increment == 100000){increment = 1000000; hertz="1 MHz"; hertzPosition=6;}  
  else{increment = 10; hertz = "10 Hz"; hertzPosition=5;};  
   lcd.setCursor(0,1);
   lcd.print("                ");
   lcd.setCursor(hertzPosition,1); 
   lcd.print(hertz); 
   delay(500); // Adjust this delay to speed up/slow down the button menu scroll speed.
};

void showFreq(){
    millions = int(rx/1000000);
    hundredthousands = ((rx/100000)%10);
    tenthousands = ((rx/10000)%10);
    thousands = ((rx/1000)%10);
    hundreds = ((rx/100)%10);
    tens = ((rx/10)%10);
    ones = ((rx/1)%10);
    lcd.setCursor(0,0);
//    lcd.print("                ");
    lcd.print("  ");
   if (millions > 9){lcd.setCursor(1,0);}
   else{lcd.setCursor(2,0);}
    lcd.print(millions);
    lcd.print("'");
    lcd.print(hundredthousands);
    lcd.print(tenthousands);
    lcd.print(thousands);
    lcd.print("'");
    lcd.print(hundreds);
    lcd.print(tens);
    lcd.print(ones);
    lcd.print(" Hz   ");
    lcd.setCursor(0,1);
    if (rx >=30000000){
      lcd.print("!");
    } else {
      lcd.print(" ");
    }
    timepassed = millis();
    memstatus = 0; // Trigger memory write
};

void storeMEM(){
  //Write each frequency section to a EPROM slot.  Yes, it's cheating but it works!
   EEPROM.write(0,millions);
   EEPROM.write(1,hundredthousands);
   EEPROM.write(2,tenthousands);
   EEPROM.write(3,thousands);
   EEPROM.write(4,hundreds);       
   EEPROM.write(5,tens);
   EEPROM.write(6,ones);   
   lcd.setCursor(15,0);
   lcd.print("M");
   memstatus = 1;  // Let program know memory has been written
};


