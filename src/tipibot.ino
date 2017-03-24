#include <Servo.h>

int STEP_L = 5;
int DIRECTION_L = 2;

int STEP_R = 6;
int DIRECTION_R = 3;

int SERVO = 9;

float machineWidth = 2000;
float stepsPerRevolution = 200;
float millimetersPerRevolution = 88;
float millimetersPerStep = millimetersPerRevolution / stepsPerRevolution;

float positionL = 0;
float positionR = 0;

float polarStepDistance = 1.0;

const int serialBufferSize = 100;
char serialBuffer[serialBufferSize];

Servo servo;

void setup() {
	pinMode(STEP_L, OUTPUT);
	pinMode(DIRECTION_L, OUTPUT);
	pinMode(STEP_R, OUTPUT);
	pinMode(DIRECTION_R, OUTPUT);

	servo.attach(SERVO);

	Serial.begin(115200);
}

void loop() {
	int byteCount = Serial.readBytesUntil('\n', serialBuffer, serialBufferSize);

	int code = atoi(serialBuffer+1);
	bool charExists = false;
	switch(serialBuffer[0]) {
		case 'G':
			float x = 0;
			float y = 0;
			float r = 0;
			if(code == 0 || code == 1 || code == 2 || code == 3 || code == 92) {
				x = readFloat(serialBuffer, 'X', &charExists);
				if(!charExists) {
					Serial.println("Could not find X parameter");
					return;
				}
				y = readFloat(serialBuffer, 'Y', &charExists);
				if(!charExists) {
					Serial.println("Could not find Y parameter");
					return;
				}
				if(code == 2 || code == 3) {
					r = readFloat(serialBuffer, 'R', &charExists);
					if(!charExists) {
						Serial.println("Could not find R parameter");
						return;
					}
				}
			}

			switch(code) {
				case 0:
					moveToFast(x, y);
				case 1:
					moveTo(x, y);
					break;
				// case 2:
				// 	circleClockWise(x, y, r);
				// 	break;
				// case 3:
				// 	circleCounterClockWise(x, y, r);
				// 	break;
				case 4:
					int p = readInt(serialBuffer, 'P', &charExists);
					if(!charExists) {
						Serial.println("Could not find P parameter");
						return;
					}
					// wait(p);
					delay(p);
					break;
				// case 21:
				// 	setMillimeters();
				// 	break;
				// case 90:
				// 	setAbsolutePosition();
				// 	break;
				// case 91:
				// 	setRelativePosition();
				// 	break;
				case 92:
					setPosition(x, y);
					break;
				default:
					Serial.println("Unknown G gcode.");
			}
			break;
		case 'M':
			switch(code) {
				case 1:
					// stop();
					break;
				case 4:
					// float penWidth = readFloat(serialBuffer, 'E', &charExists);
					// if(charExists) {
					// 	setPenWidth(penWidth);
					// }
					float newMachineWidth = readFloat(serialBuffer, 'X', &charExists);
					if(charExists) {
						machineWidth = newMachineWidth;
					}
					float newStepsPerRevolution = readFloat(serialBuffer, 'S', &charExists);
					if(charExists) {
						stepsPerRevolution = newStepsPerRevolution;
					}
					float newMillimetersPerRevolution = readFloat(serialBuffer, 'P', &charExists);
					if(charExists) {
						millimetersPerRevolution = newMillimetersPerRevolution;
					}
					millimetersPerStep = millimetersPerRevolution / stepsPerRevolution;
					break;
				case 340:
					float angle = readAngle(serialBuffer, 'S', charExists);
					if(!charExists) {
						Serial.println("Could not find S parameter");
						return;
					}
					servo.write(angle);
					break
				default:
					Serial.println("Unknown M gcode.");
			}
			break;
		default:
			Serial.println("Unknown gcode.");
	}
}

int readInt(char* buffer, const char c, bool* charExists) {
	char* pch = strchr(buffer, c);
	if (charExists != NULL) {
		*charExists = pch != NULL;
	}
	if(pch != NULL) {
		return atoi(pch);
	} else {
		return -1;
	}
}

float readFloat(char* buffer, const char c, bool* charExists) {
	char * pch = strchr(buffer, c);
	if(charExists != NULL) {
		*charExists = pch != NULL;
	}
	if(pch != NULL) {
		return atof(pch);
	} else {
		return -1;
	}
}

void moveOnStep(int directionPin, int stepPin, int direction, int delay) {
	digitalWrite(directionPin, direction);

	digitalWrite(stepPin, 1);
	delay(delay/2);
	digitalWrite(stepPin, 0);
	delay(delay/2);
}

void moveOnStepL(bool direction) {
    moveOnStep(DIRECTION_L, STEP_L, direction ? 1 : 0, 2)
}

void moveOnStepR(bool direction) {
    moveOnStep(DIRECTION_R, STEP_R, direction ? 1 : 0, 2)
}

void moveToPolar(float l, float r) {
	float deltaL = l - positionL;
	float deltaR = y - positionY;
	int nStepX = round(abs(deltaL) / millimetersPerStep);
	int nStepR = round(abs(deltaR) / millimetersPerStep);

	int directionL = deltaL >= 0 ? 1 : -1;
	int directionR = deltaR >= 0 ? 1 : -1;
	//
	// if(nStepL <= nStepR) {
	//   int nStepPerLoop = nStepL > 0 ? nStepR / nStepL : 0;
	//   int nStepRemaining = nStepR - nStepPerLoop * nStepL;
	//   for(int i=0 ; i < nStepL+1 ; i++) {
	//     moveOneStepL();
	//     for(int j=0 ; i < nStepL ? j < nStepPerLoop : nStepRemaining ; j++) {
	//       moveOneStepR();
	//     }
	//   }
	// } else {
	//   int nStepPerLoop = nStepR > 0 ? nStepL / nStepR : 0;
	//   int nStepRemaining = nStepL - nStepPerLoop * nStepR;
	//   for(int i=0 ; i < nStepR+1 ; i++) {
	//     moveOneStepR();
	//     for(int j=0 ; i < nStepR ? j < nStepPerLoop : nStepRemaining ; j++) {
	//       moveOneStepL();
	//     }
	//   }
	// }

	int nStepMin = nStepL;
	int nStepMax = nStepR;

	if(nStepL > nStepR) {
		nStepMin = nStepR;
		nStepMax = nStepL;
	}

	int nStepPerLoop = nStepMin > 0 ? nStepMax / nStepMin : 0;
	int nStepRemaining = nStepMax - nStepPerLoop * nStepMin;
	for(int i=0 ; i < nStepMin+1 ; i++) {
		if(nStepL < nStepR) {
			moveOneStepL(directionL);
		} else {
			moveOneStepR(directionR);
		}
		for(int j=0 ; i < nStepMin ? j < nStepPerLoop : nStepRemaining ; j++) {
			if(nStepL < nStepR) {
				moveOneStepR(directionR);
			} else {
				moveOneStepL(directionL);
			}
		}
	}
    positionL = l;
    positionR = r;
}

void orthoToPolar(float x, float y, float* l, float* r) {
	float x2 = x * x;
	float y2 = y * y;
	float WmX = machineWidth - x;
	float WmX2 = WmX * WmX;
	*l = sqrt(x2 + y2);
	*r = sqrt(WmX2 + y2);
}

void moveTo(float x, float y) {

	float deltaX = x - positionX;
	float deltaY = y - positionY;

	float distance = sqrt(deltaX * deltaX + deltaY + deltaY);

	int nPolarSteps = distance / int(polarStepDistance);
	float intDistance = nPolarSteps * polarStepDistance;
	float remainingDistance = distance - intDistance;

	float u = deltaX / distance;
	float v = deltaY / distance;

	float intDestinationX = u * intDistance;
	float intDestinationY = v * intDistance;

	float stepX = intDestinationX / nPolarSteps;
	float stepY = intDestinationY / nPolarSteps;

	float destinationX = positionX;
	float destinationY = positionY;

	for(int i=0 ; i<nPolarSteps+1 ; i++) {
		if(i<nPolarSteps) {
			destinationX += u * stepX;
			destinationY += v * stepY;
		} else {
			destinationX = x;
			destinationY = y;
		}
		float destinationL = 0;
		float destinationR = 0;
		orthoToPolar(destinationX, destinationY, &destinationL, &destinationR);
		moveToPolar(destinationL, destinationR);
	}
    positionX = x;
    positionY = y;
}

void moveToFast(float x, float y) {
	float destinationL = 0;
	float destinationR = 0;
	orthoToPolar(x, y, &destinationL, &destinationR);
	moveToPolar(destinationL, destinationR);
    positionX = x;
    positionY = y;
}

void setPosition(float x, float y) {
	positionX = x;
    positionY = y;
	orthoToPolar(x, y, &positionL, &positionR);
}
