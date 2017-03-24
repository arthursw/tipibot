int STEP_L = 5;
int DIRECTION_L = 2;

int STEP_R = 6;
int DIRECTION_R = 3;

int stepDuration = 750;

float positionL = 0;
float positionR = 0;

const int serialBufferSize = 100;
char serialBuffer[serialBufferSize];

void setup() {
	pinMode(STEP_L, OUTPUT);
	pinMode(DIRECTION_L, OUTPUT);
	pinMode(STEP_R, OUTPUT);
	pinMode(DIRECTION_R, OUTPUT);

	Serial.begin(115200);
}

void loop() {
	int byteCount = -1;
	int byteCount =  Serial.readBytesUntil('\n', serialBuffer, serialBufferSize);

	int code = atoi(serialBuffer+1);
	switch(serialBuffer[0]) {
		case 'G':
			float x = 0;
			float y = 0;
			float r = 0;
			if(code == 0 || code == 1 || code == 2 || code == 3 || code == 92) {
				bool charExists = false;
				x = readFloat(serialBuffer, 'X', charExists);
				if(!charExists) {
					Serial.println('Could not find X parameter');
					return;
				}
				y = readFloat(serialBuffer, 'Y', charExists);
				if(!charExists) {
					Serial.println('Could not find Y parameter');
					return;
				}
				if(code == 2 || code == 3) {
					r = readFloat(serialBuffer, 'R', charExists);
					if(!charExists) {
						Serial.println('Could not find R parameter');
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
				case 2:
					circleClockWise(x, y, r);
					break;
				case 3:
					circleCounterClockWise(x, y, r);
					break;
				case 4:
					int p = readInt(serialBuffer, 'P', charExists);
					if(!charExists) {
						Serial.println('Could not find P parameter');
						return;
					}
					wait(p);
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
			}
			break;
		case 'M':
			switch(code) {
				case 1:
					stop();
					break;
				case 4:
					float penWidth = readFloat(serialBuffer, 'E', charExists);
					if(charExists) {
						setPenWidth(penWidth);
					}
					float machineWidth = readFloat(serialBuffer, 'X', charExists);
					if(charExists) {
						setMachineWidth(machineWidth);
					}
					float stepsPerRevolution = readFloat(serialBuffer, 'S', charExists);
					if(charExists) {
						setStepsPerRevolution(stepsPerRevolution);
					}
					float millimetersPerRevolution = readFloat(serialBuffer, 'P', charExists);
					if(charExists) {
						setMillimetersPerRevolution(millimetersPerRevolution);
					}
					break;
				case 340:
					int servoID = readInt(serialBuffer, 'P');
					int position = readInt(serialBuffer, 'S');
					setServo(servoID, position);
					break;
			}
			break;
		default:
			Serial.println('Unknown gcode.')
	}
}

int readInt(char* buffer, const char c, bool* charExists) {
	char * pch = strchr(buffer, c);
	if(charExists != NULL) {
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
	int nStepX = round(abs(deltaL) * unitToStep);
	int nStepR = round(abs(deltaR) * unitToStep);

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
	int nStepMaL = nStepR;

	if(nStepL > nStepR) {
		nStepMin = nStepR;
		nStepMaL = nStepL;
	}

	int nStepPerLoop = nStepMin > 0 ? nStepMaL / nStepMin : 0;
	int nStepRemaining = nStepMaL - nStepPerLoop * nStepMin;
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

float polarStepDistance = 1.0;
float polarDistanceL = 0.0;
float polarDistanceR = 0.0;

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
		float destinationX2 = destinationX * destinationX;
		float destinationY2 = destinationY * destinationY;
		float WmX = machineWidth - destinationX;
		float WmX2 = WmX * WmX;
		float destinationL = sqrt(destinationX2 + destinationY2);
		float destinationR = sqrt(WmX2 + destinationY2);
		moveToPolar(destinationL, destinationR);
	}
    positionX = x;
    positionY = y;
}
