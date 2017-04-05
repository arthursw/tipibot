#include <Servo.h>

int STEP_L = 5;
int DIRECTION_L = 2;

int STEP_R = 6;
int DIRECTION_R = 3;

int ENABLE = 8;
int SERVO = 9;

float machineWidth = 2000;
float stepsPerRevolution = 200;
float millimetersPerRevolution = 96;
float millimetersPerStep = millimetersPerRevolution / stepsPerRevolution;

float wakeTime = 0.0;

float positionL = 0;
float positionR = 0;
float positionX = 0;
float positionY = 0;

float polarStepDistance = 1.0;

const int serialBufferSize = 100;
char serialBuffer[serialBufferSize];

// Parameters
float x = 0;
float y = 0;
float s = 0;
int p = 0;

Servo servo;

void setup() {
	pinMode(ENABLE, OUTPUT);
	digitalWrite(ENABLE, 0);

	pinMode(STEP_L, OUTPUT);
	pinMode(DIRECTION_L, OUTPUT);
	pinMode(STEP_R, OUTPUT);
	pinMode(DIRECTION_R, OUTPUT);

	servo.attach(SERVO);

	Serial.begin(115200);
	Serial.println("Tipibot ready.");
}

int loopCount = 0;

void loop() {

	if(loopCount == 0) {
		// Serial.println("loop");
		loopCount++;
	}

	int byteCount = Serial.readBytesUntil('\n', serialBuffer, serialBufferSize);
	if(byteCount == 0) {
		//Serial.println("nothing to read...");
		return;
	}

	loopCount = 0;
	// Serial.print(serialBuffer);

	int code = atoi(serialBuffer+1);
	bool charExists = false;
	float newMachineWidth = 0;
	float newStepsPerRevolution = 0;
	float newMillimetersPerRevolution = 0;
	float angle = 0;
	bool bParameterFound = false;
	switch(serialBuffer[0]) {
		case 'G':
		// Serial.println("G");
		if(code == 0 || code == 1 || code == 2 || code == 3 || code == 92) {
			x = readFloat(serialBuffer, 'X', &charExists);
			bParameterFound |= charExists;
			y = readFloat(serialBuffer, 'Y', &charExists);
			bParameterFound &= charExists;
			s = readFloat(serialBuffer, 'S', &charExists);
			bParameterFound |= code == 0 && charExists;
			if(!bParameterFound) {
				Serial.println("Could not find required parameter.");
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
			// Serial.println("0");
			moveToFast(x, y);
			break;
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
			p = readInt(serialBuffer, 'P', &charExists);
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
			newMachineWidth = readFloat(serialBuffer, 'X', &charExists);
			if(charExists) {
				machineWidth = newMachineWidth;
			}
			newStepsPerRevolution = readFloat(serialBuffer, 'S', &charExists);
			if(charExists) {
				stepsPerRevolution = newStepsPerRevolution;
			}
			newMillimetersPerRevolution = readFloat(serialBuffer, 'P', &charExists);
			if(charExists) {
				millimetersPerRevolution = newMillimetersPerRevolution;
			}
			millimetersPerStep = millimetersPerRevolution / stepsPerRevolution;
			break;
			case 340:
			angle = readFloat(serialBuffer, 'S', &charExists);
			if(!charExists) {
				Serial.println("Could not find S parameter");
				return;
			}
			servo.write(angle);
			break;
			default:
			Serial.println("Unknown M gcode.");
		}
		break;
		default:
		Serial.println("Unknown gcode.");
	}
	Serial.println("ok");
}

int readInt(char* buffer, const char c, bool* charExists) {
	char* pch = strchr(buffer, c);
	if(charExists != NULL) {
		*charExists = pch != NULL;
	}
	if(pch != NULL) {
		return atoi(pch+1);
	} else {
		return -1;
	}
}

float readFloat(char* buffer, const char c, bool* charExists) {
	char * pch = strchr(buffer, c);
	if(charExists != NULL) {
		*charExists = pch != NULL;
	}
	if(pch != NULL) {
		return atof(pch+1);
	} else {
		return -1;
	}
}

void moveOneStep(int stepPin, int duration) {
	digitalWrite(stepPin, 1);
	delayMicroseconds(duration/2.0f);
	digitalWrite(stepPin, 0);
	delayMicroseconds(duration/2.0f);
}

void setDirectionL(int direction) {
	digitalWrite(DIRECTION_L, direction);
}

void setDirectionR(int direction) {
	digitalWrite(DIRECTION_R, direction);
}

void moveOneStepL() {
	moveOneStep(STEP_L, 350);
}

void moveOneStepR() {
	moveOneStep(STEP_R, 350);
}

void moveToPolar(float l, float r) {
	float deltaL = l - positionL;
	float deltaR = r - positionR;
	int nStepL = round(fabs(deltaL) / millimetersPerStep);
	int nStepR = round(fabs(deltaR) / millimetersPerStep);

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

	// Serial.print("position: ");
	// Serial.print(positionL);
	// Serial.print(", ");
	// Serial.println(positionR);
	// Serial.print("nSteps: ");
	// Serial.print(nStepL);
	// Serial.print(", ");
	// Serial.println(nStepR);

	setDirectionL(directionL);
	setDirectionR(directionR);

	for(int i=0 ; i < nStepMin ; i++) {
		if(nStepL < nStepR) {
			moveOneStepL();
		} else {
			moveOneStepR();
		}
		for(int j=0 ; i < nStepMin - 1 ? j < nStepPerLoop : j < nStepRemaining ; j++) {
			if(nStepL < nStepR) {
				moveOneStepR();
			} else {
				moveOneStepL();
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

void polarToOrtho(float l, float r, float* x, float* y) {
	float l2 = l * l;
	float r2 = r * r;
	float w2 = machineWidth * machineWidth;
	*x = (l2 - r2 + w2) / ( 2.0 * machineWidth );
	float x2 = *x * *x;
	*y = sqrt(l2 - x2);
}

void moveTo(float x, float y) {

	float deltaX = x - positionX;
	float deltaY = y - positionY;

	float distance = sqrt(deltaX * deltaX + deltaY + deltaY);

	int nPolarSteps = distance / int(polarStepDistance);
	float intDistance = nPolarSteps * polarStepDistance;

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
			destinationX += stepX;
			destinationY += stepY;
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





typedef enum {  NONE, MOVE_DIRECT, MOVE_LINEAR, MOVE_PEN, WAIT, SET_POSITION, SETUP } Commands;
typedef enum {  NONE, ACTION, SPECIAL } CommandTypes;

Commands command = NONE;
CommandTypes commandType = NONE;

bool commandReadyToStart = false;
bool commandDone = true;

char commandChar = '0';

const unsigned int NUM_PARAMETERS = 7;
float[NUM_PARAMETERS] parameters;

float currentValue = 0.0;
int nDecimals = 0;

void processIncomingByte (const byte c)
{
	if (isdigit(c))
	{
		if(nDecimals <= 0) {
			currentValue *= 10;
			currentValue += c - '0';
		} else {
			currentValue += pow(10, -nDecimals) * (c - '0');
			nDecimals++;
		}
	}
	else
	{
		if(c == '.') {
			nDecimals = 1;
			return;
		}

		nDecimals = 0;
		currentValue = 0;

		if(commandChar == '0') {
			if(c == 'G' || c == 'M') {
				commandChar = c;
			}
		} else {

			if(parameterIndex != -1) {
				parameters[parameterIndex] = currentValue;
			}

			int code = -1;
			switch (c)
			{
				case 'X':
					parameterIndex = 0;
				break;
				case 'Y':
					parameterIndex = 1;
				break;
				case 'R':
					parameterIndex = 2;
				break;
				case 'S':
					parameterIndex = 3;
				break;
				case 'G':
					parameterIndex = 4;
				break;
				case 'M':
					parameterIndex = 5;
				break;
				case 'P':
					parameterIndex = 6;
				break;
				case '\n':
					if(commandChar == 'G') {
						commandType = ACTION;
					} else if (commandChar == 'M') {
						commandType = SPECIAL;
					}
					commandReadyToStart = true;
				break;
				default:
					parameterIndex = -1;
				break;
			}
		}
	}
}

unsigned long lastTime = 0;

unsigned long nextChangeL = 0;
unsigned long nextChangeR = 0;

int motorStateL = 0;
int motorStateR = 0;

int motorStepLengthL = 350;
int motorStepLengthR = 350;

int motorNumStepsDoneL = 0;
int motorNumStepsDoneR = 0;

int motorNumStepsToDoL = 0;
int motorNumStepsToDoR = 0;

int motorDirectionL = 0;
int motorDirectionR = 0;

float targetX = 0;
float targetY = 0;

bool equals(float a, float b) {
	return fabs(a - b) <= millimetersPerStep;
}

void updateMotors() {
	unsigned long currentTime = micros();

	if(currentTime >= nextChangeL) {
		motorStateL = motorStateL == 0 ? 1 : 0;
		digitalWrite(STEP_L, motorStateL);
		nextChangeL += motorStepLengthL;
		if(motorStateL == 0) {
			motorNumStepsDoneL++;
		}
	}

	if(motorNumStepsDoneL == motorNumStepsToDoL) {
		positionL += millimetersPerStep * motorNumStepsDoneL * motorDirectionL;
		polarToOrtho(positionL, positionR, &positionX, &positionY);
	}

	if(currentTime >= nextChangeR) {
		motorStateR = motorStateR == 0 ? 1 : 0;
		digitalWrite(STEP_R, motorStateR);
		nextChangeR += motorStepLengthR;
		if(motorStateR == 0) {
			motorNumStepsDoneR++;
			positionR += millimetersPerStep * motorDirectionR;
		}
	}

	if(motorNumStepsDoneR == motorNumStepsToDoR) {
		positionR += millimetersPerStep * motorNumStepsDoneR * motorDirectionR;
		polarToOrtho(positionL, positionR, &positionX, &positionY);
	}

	if(motorNumStepsDoneL == motorNumStepsToDoL || motorNumStepsDoneR == motorNumStepsToDoR) {
		if(equals(positionX, targetX) && equals(positionY, targetY)) {
			commandDone = true;
		} else {
			setTarget(targetX, targetY);
		}
	}
}

void updatePen() {
	servo.write(angle);
	commandDone = true;
}

void setPolarTarget(float l, float r) {
	float deltaL = l - positionL;
	float deltaR = r - positionR;
	motorNumStepsToDoL = round(fabs(deltaL) / millimetersPerStep);
	motorNumStepsToDoR = round(fabs(deltaR) / millimetersPerStep);

	motorDirectionL = deltaL >= 0 ? 1 : -1;
	motorDirectionR = deltaR >= 0 ? 1 : -1;

	if(motorNumStepsToDoL <= motorNumStepsToDoR) {
		motorStepLengthR = 175;
		motorStepLengthL = motorStepLengthR * motorNumStepsToDoR / motorNumStepsToDoL;
	} else {
		motorStepLengthL = 175;
		motorStepLengthR = motorStepLengthL * motorNumStepsToDoL / motorNumStepsToDoR;
	}
	motorNumStepsDoneL = 0;
	motorNumStepsDoneR = 0;
}

void setTarget(float x, float y) {

	float deltaX = x - positionX;
	float deltaY = y - positionY;

	float distance = sqrt(deltaX * deltaX + deltaY + deltaY);

	int nPolarSteps = distance / int(polarStepDistance);

	float subTargetX = 0;
	float subTargetY = 0;

	if(nPolarSteps > 0) {
		float intDistance = nPolarSteps * polarStepDistance;

		float u = deltaX / distance;
		float v = deltaY / distance;

		float intDestinationX = u * intDistance;
		float intDestinationY = v * intDistance;

		float stepX = intDestinationX / nPolarSteps;
		float stepY = intDestinationY / nPolarSteps;

		subTargetX = positionX + stepX;
		subTargetY = positionX + stepY;
	} else {
		subTargetX = x;
		subTargetY = y;
	}

	float destinationL = 0;
	float destinationR = 0;
	orthoToPolar(subTargetX, subTargetY, &destinationL, &destinationR);
	setPolarTarget(destinationL, destinationR);
}

// state machine

void readCommand() {
	while (Serial.available() && !commandReadyToStart) {
		processIncomingByte (Serial.read ());
	}
}

void startCommand() {
	float targetL = 0;
	float targetR = 0;
	if(commandType == ACTION) {
		code = parameters[4];
		switch (code) {
			case 0:
				targetX = parameters[0];
				targetY = parameters[1];

				orthoToPolar(targetX, targetY, &targetL, &targetR);
				setPolarTarget(targetL, targetR);

				command = MOVE_DIRECT;
			break;
			case 1:
				targetX = parameters[0];
				targetY = parameters[1];

				setTarget(targetX, targetY);
				command = MOVE_LINEAR;
			break;
			// case 2:
			// 	circleClockWise(x, y, r);
			// 	break;
			// case 3:
			// 	circleCounterClockWise(x, y, r);
			// 	break;
			case 4:
				wakeTime = micros() + parameters[6];
				command = WAIT;
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
				command = SET_POSITION;
			break;
			default:
				Serial.println("Unknown G gcode.");
		}
	} else if(commandType == SPECIAL) {
		code = parameters[5];
		switch(code) {
			case 1:
				// stop();
			break;
			case 4:
				if(parameters[0] > 0) {
					machineWidth = parameters[0];
				}
				if(parameters[3] > 0) {
					stepsPerRevolution = parameters[3];
				}
				if(parameters[6] > 0) {
					millimetersPerRevolution = parameters[6];
				}
				millimetersPerStep = millimetersPerRevolution / stepsPerRevolution;
				command = SETUP;
				commandDone = true;
			break;
			case 340:
				angle = parameters[3];
				command = MOVE_PEN;
			break;
			default:
				Serial.println("Unknown M gcode.");
		}
	}
	for(int i=0 ; i<NUM_PARAMETERS ; i++) {
		parameters[i] = -1.0;
	}
	commandReadyToStart = false;
	commandDone = false;
}

void executeCommand() {

	if(command == MOVE_DIRECT || command == MOVE_LINEAR) {
		updateMotors();
	} else if(command == MOVE_PEN){
		updatePen();
	} else if(command == WAIT) {
		if(micros() >= wakeTime) {
			commandDone = true;
		}
	}

	if(commandDone) {
		command = NONE;
	}
}

void loop ()
{
	if(!commandReadyToStart) {
		readCommand();
	}

	if(commandReadyToStart && commandDone) {
		startCommand();
	}

	if(!commandDone) {
		executeCommand();
	}
}
