#include <iostream>

using namespace std;

class Servo {
public:
	Servo() {

	}

	void attach(int pin) {
		cout << "attach servo: " << pin << endl;
		return;
	}

	void write(int angle) {
		cout << "write to servo: " << angle << endl;
		return;
	}
};
