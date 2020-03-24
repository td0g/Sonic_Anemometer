/*
Firmware for Arduino Uno-Based Sonic Anemometer
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
  
Test program for sonic anemometer using Arduino Uno
Program reports clock cycles, microseconds, and check microseconds per ping
Weighted average filtering
Send serial char(s) to pause/unpause
*/

#define DIVIDER 20

volatile unsigned long ovfCount;
float pulseTime;
unsigned long testTime;

void setup() {
  Serial.begin(9600);
  
//timer 2 setup (no prescaler, normal mode)
  TCCR2A = 0b00000000;
  TCCR2B = 0b00000001;
  
//pin setup
  pinMode(2, OUTPUT); //Trig
  pinMode(3, INPUT);  //Echo
  
//announce readiness
  Serial.println("SONIC ANEMOMETER");
}

void loop() {
  static unsigned long _timer;
  delay(10);
  unsigned long _t = getPulseTime();
  _t /= DIVIDER;
  pulseTime = pulseTime * (DIVIDER - 1) / DIVIDER;
  pulseTime += _t;
  if (millis() > _timer){
    _timer += 500;
    Serial.print(pulseTime);
    Serial.print("  (");
    Serial.print(pulseTime / 16);
    Serial.print(" us)");
    Serial.println(testTime);
  }
  if (Serial.available()){
    Serial.println("\n  PAUSED\n");
    delay(50);
    while (Serial.available()) Serial.read();
    while (!Serial.available()){};
    while (Serial.available()) Serial.read();
    _timer = millis();
    Serial.print("\n\n\n");
  }
}

unsigned long getPulseTime(){
  unsigned long _timerCount;
  ovfCount = 0;			//Reset counter
  PORTD |= 0b00000100;	//Send trigger pulse
  TCNT2 = 0;			//Reset timer
  TIMSK2 = 0b00000001; 	//ISR on
  testTime = micros();	//Check microseconds
  delayMicroseconds(100);
  PORTD &= 0b11111011;
  delayMicroseconds(500);			//Ignore fast echoes
  while ((PIND & 0b00001000)){};	//Wait for ping  	OOPS - Should be !(PIND & 0ob00001000)
  TIMSK2 = 0b00000000;  			//ISR off
  _timerCount = TCNT2;				//Get timer
  testTime = micros() - testTime;	
  ovfCount = ovfCount << 8;				//Add overflow counter
  _timerCount = _timerCount + ovfCount;	//Add overflow counter
  return _timerCount;
}


ISR(TIMER2_OVF_vect){
  ovfCount++;
}
