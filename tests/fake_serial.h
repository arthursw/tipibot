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
#pragma once

#include <iostream>
#include <string>

using namespace std;

class FakeSerial {
public:
	string fakeData;
	int dataIndex;
	FakeSerial():dataIndex(0) {
		
	}
	void fakeFeed(string& testCommands){
		fakeData = testCommands;
	}
	bool available() {
		return dataIndex < fakeData.length();
	}
	char read(){
		return fakeData[dataIndex++];
	}
  void println(const char * str)
  { 
    cout << str << endl;
  }
  void print(const char * str)
  { 
    cout << str;
  }
  void println(float str)
  { 
    cout << str << endl;
  }
  void print(float str)
  { 
    cout << str;
  }
  int readBytesUntil(const char c, const char * buffer, int bufferSize){
    return 1;
  }
  void begin(unsigned long speed){

  }

  void end(){

  }
  size_t write(const unsigned char*, size_t){ return 0;}
};

FakeSerial Serial;
