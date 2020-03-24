/*
Firmware for ATTINY85-Based Sonic Anemometer
2020-03-21
Written by Tyler Gerritsen
vtgerritsen@gmail.com
www.td0g.ca

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/

#define DISPLAY_CLK_PIN 2
#define DISPLAY_DIO_PIN 3
#define PING_OUT_PIN 1
#define PING_IN_PIN 0
#define BUTTON_PIN 4

#define UP_BUTTON_PRESSED (btn & 0b00000100)
#define RIGHT_BUTTON_PRESSED (btn & 0b00000010)
#define DOWN_BUTTON_PRESSED (btn & 0b00000001)

volatile byte ovfCount;
unsigned long pulseTime = 1000;
unsigned long testTime;
byte btn;
byte menuPosition = 0;
unsigned int testInterval = 20;

byte filterSize = 4;

unsigned long instZeroTime = 0;

//Display
  #include "TD0G_1637.h"
  tm1637 display(DISPLAY_CLK_PIN, DISPLAY_DIO_PIN);

  #include <EEPROM.h>
  
void setup() {
//timer 2 setup (no prescaler, normal mode)
  TCCR1 = 0b00000001; //clock/1, no pin out
  GTCCR = 0B00000000;
  
  ADMUX = 0b00100010; //PB4
  
//pin setup
  pinMode(PING_OUT_PIN, OUTPUT); //Trig
  pinMode(PING_IN_PIN, INPUT);  //Echo
  pinMode(BUTTON_PIN, INPUT);
  
  display.setBrightness(7);

  filterSize = EEPROM.read(0);
  if (!filterSize || filterSize == 255){
    EEPROM.write(0, 4);
    filterSize = 4;
  }
}

void loop() {
  analogReadAll();
  runDisplay();
  runAnemometer();
}

void analogReadAll(){
  static byte lastBtn;
  static byte lastBtnCount;
  if (!(ADCSRA & 0b01000000)){
    byte adc = ADCH;
    ADCSRB = 1;
    ADCSRA = 0b11010111; //Enable ADC
    /*
     175 = RIGHT
     127 = DOWN
     100 = DOWN,RIGHT
     89 = UP
     78 = UP,RIGHT
     67 = UP,DOWN
     58 = UP,DOWN,RIGHT
     */
     byte thisBtn;
    if (adc < 72) thisBtn = 0b00000101;
    else if (adc < 110) thisBtn = 0b00000100;
    else if (adc < 150) thisBtn = 0b00000001;
    else if (adc < 200) thisBtn = 0b00000010;
    else thisBtn = 0;
    if (thisBtn == lastBtn && thisBtn) lastBtnCount++;
    else {
      lastBtnCount = 0;
      lastBtn = thisBtn;
      btn = 0;
    }
    if (lastBtnCount == 5) btn = lastBtn;
  }
}

void runDisplay(){
  static unsigned long _lastUpdate = 0;
  switch (menuPosition){
    case 0:
      if (millis() > _lastUpdate + 200){
        if (instZeroTime){
          if (pulseTime < instZeroTime) {
            display.printInt(instZeroTime - pulseTime);
            display.forceLineState(0,0b01000000,0b00111111);
          }
          else {
            display.clearLineState();
            display.printInt(pulseTime - instZeroTime);
          }
          _lastUpdate = millis();
        }
        else display.printInt(pulseTime);
      }
      if (DOWN_BUTTON_PRESSED) instZeroTime = pulseTime;
      else if (UP_BUTTON_PRESSED) instZeroTime = 0;
      else if (RIGHT_BUTTON_PRESSED) {
        menuPosition = 1;
        display.forceLineState(0,0b01110001,0);
        display.printInt(filterSize);
      }
      while (btn) analogReadAll();
    break;
    case 1:
      if (UP_BUTTON_PRESSED || DOWN_BUTTON_PRESSED){
        if (UP_BUTTON_PRESSED) filterSize++;
        else if (DOWN_BUTTON_PRESSED) filterSize = max(filterSize-1, 1);
        display.printInt(filterSize);
      }
      if (RIGHT_BUTTON_PRESSED){
        menuPosition = 0;
        display.clearLineState();
        EEPROM.update(0, filterSize);
      }
      while (btn) analogReadAll();
    break;
  }
}

void runAnemometer(){
  static unsigned long _lastTestTime;
  if (millis() > _lastTestTime + testInterval){
    _lastTestTime = millis();
    unsigned int _t = getPulseTime();
    pulseTime *= (filterSize - 1);
    pulseTime += _t;
    pulseTime /= filterSize;
  }
}

unsigned long getPulseTime(){
  unsigned long _timerCount;
  byte _isrOff = TIMSK;
  byte _isrOn = _isrOff | 0b00000100;
  unsigned int _ovfCountTime;
  TIMSK = _isrOn; 	//ISR on
  PORTB = 0b00000010;  //Send trigger pulse
  TCNT1 = 0;      //Reset timer
  ovfCount = 0;      //Reset counter
  delayMicroseconds(100);
  PORTB &= 0b11111101;
  delayMicroseconds(500);			//Ignore fast echoes
  while ((PINB & 0b00000001)){};	//Wait for ping  	OOPS - Should be !(PIND & 0ob00001000)
  TIMSK = _isrOff;  			//ISR off
  _timerCount = TCNT1;				//Get timer
  _ovfCountTime = ovfCount;
  _ovfCountTime = _ovfCountTime << 8;				//Add overflow counter
  _timerCount = _timerCount + _ovfCountTime;	//Add overflow counter
  //_timerCount = ovfCount;
  return _timerCount;
}


ISR(TIMER1_OVF_vect){
  ovfCount++;
}
