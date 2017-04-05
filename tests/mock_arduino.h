/*
  DSM2_tx implements the serial communication protocol used for operating
  the RF modules that can be found in many DSM2-compatible transmitters.

  Copyrigt (C) 2012  Erik Elmore <erik@ironsavior.net>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.  
*/

#include <iostream>
#include <math.h>

using namespace std;

#define OUTPUT 0
#define INPUT 1

#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#define lowByte(w) ((unsigned char) ((w) & 0xff))
#define highByte(w) ((unsigned char) ((w) >> 8))

typedef unsigned char byte;
typedef unsigned short int word;

void delay(unsigned long ms) {
}
unsigned long millis(){
  return 0;
}

// WMath.cpp
long map(long, long, long, long, long);

void initialize_mock_arduino(); 

void pinMode(int pin, int mode){
  cout << "Set pin " << pin << " to mode " << mode << endl;
}
void digitalWrite(int pin, int value) {
   cout << "Digital write: pin: " << pin << ", value: " << value << endl;
}
#include "fake_serial.h"
