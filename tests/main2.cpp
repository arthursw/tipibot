#include "mock_arduino.h"
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

int sleepTimer = 0;

float positionL = 0;
float positionR = 0;
float positionX = 0;
float positionY = 0;

float polarStepDistance = 1.0;

Servo servo;

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

int motorStateL = 0;
int motorStateR = 0;

int motorTimerL = 0;
int motorTimerR = 0;

int motorNumStepsDoneL = 0;
int motorNumStepsDoneR = 0;

int motorNumStepsToDoL = 0;
int motorNumStepsToDoR = 0;

int motorDirectionL = 0;
int motorDirectionR = 0;

float targetX = 0;
float targetY = 0;

float subTargetX = 0;
float subTargetY = 0;

bool lastPolarStep = false;

void setup();
void orthoToPolar(float x, float y, float* l, float* r);
void polarToOrtho(float l, float r, float* x, float* y);
unsigned long getTime();
int createTimer();
void setTimer(int timerID, unsigned long delay);
bool isTimerElapsed(int timerID, unsigned long currentTime);
bool isTimerElapsed(int timerID);
void setPosition(float x, float y);
void processIncomingByte (const byte c);
bool equals(float a, float b);
void updateMotors();
void updatePen();
void setPolarTarget(float l, float r);
void setTargetDirect(float x, float y);
void setTarget(float x, float y);
void readCommand();
void startCommand();
void executeCommand();
void loop();

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

	motorTimerL = createTimer();
	motorTimerR = createTimer());
	sleepTimer = createTimer();
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

const unsigned int NUM_TIMERS 5;
unsigned long timers[NUM_TIMERS];
bool timerOverflows[NUM_TIMERS];
int nTimers = 0;
unsigned long lastTime = 0;

unsigned long getTime() {
	unsigned long currentTime = micros();
	if(lastTime > currentTime) { 	// overflow!
		for(int i=0 ; i<NUM_TIMERS ; i++) {
			timerOverflows[i] = false;
		}
	}
	lastTime = currentTime;
	return currentTime;
}

int createTimer() {
	if(nTimers >= NUM_TIMERS) {
		return -1;
	}
	// initialize timers with overflow to prevent timeout when the timer is not set
	timerOverflows[nTimers] = true;
	return nTimers++;
}

void setTimer(int timerID, unsigned long delay) {
	unsigned long currentTime = getTime();
	timers[timerID] = currentTime + delay;
	timerOverflows[timerID] = timers[timerID] < currentTime;
}

bool isTimerElapsed(int timerID, unsigned long currentTime) {
	!timerOverflows[timerID] && currentTime > timers[timerID];
}

bool isTimerElapsed(int timerID) {
	unsigned long currentTime = getTime();
	return isTimerElapsed(timerID, currentTime);
}

void setPosition(float x, float y) {
	positionX = x;
	positionY = y;
	orthoToPolar(x, y, &positionL, &positionR);
}

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
	else if(c == '.') {
		nDecimals = 1;
	}
	else
	{
		nDecimals = 0;
		currentValue = 0;

		if(parameterIndex != -1) {
			parameters[parameterIndex] = currentValue;
		}

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
				commandChar = c;
			break;
			case 'M':
				parameterIndex = 5;
				commandChar = c;
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
				if(commandType == ACTION || commandType == SPECIAL) {
					commandReadyToStart = true;
				}
				parameterIndex = -1;
				commandChar = '0';
			break;
			default:
				parameterIndex = -1;
			break;
		}
	}
}

bool equals(float a, float b) {
	return fabs(a - b) <= millimetersPerStep;
}

void updateMotors() {
	unsigned long currentTime = getTime();
	bool positionChanged = false;

	if(isTimerElapsed(motorTimerL)) {
		motorStateL = motorStateL == 0 ? 1 : 0;
		digitalWrite(STEP_L, motorStateL);
		setTimer(motorTimerL, motorStepLengthL);
		if(motorStateL == 0) {
			motorNumStepsDoneL++;
			positionL += millimetersPerStep * motorDirectionL;
			positionChanged = true;
		}
	}

	if(isTimerElapsed(motorTimerR)) {
		motorStateR = motorStateR == 0 ? 1 : 0;
		digitalWrite(STEP_R, motorStateR);
		setTimer(motorTimerR, motorStepLengthR);
		if(motorStateR == 0) {
			motorNumStepsDoneR++;
			positionR += millimetersPerStep * motorDirectionR;
			positionChanged = true;
		}
	}

	if(positionChanged) {
		polarToOrtho(positionL, positionR, &positionX, &positionY);
	}

	if(motorNumStepsDoneL == motorNumStepsToDoL && motorNumStepsDoneR == motorNumStepsToDoR) {
		positionX = subTargetX;
		positionY = subTargetY;
		if(lastPolarStep) {
			commandDone = true;
			lastPolarStep = false;
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

	setTimer(motorTimerL, 0);
	setTimer(motorTimerR, 0);
}

void setTargetDirect(float x, float y) {
	float targetL = 0.0;
	float targetR = 0.0;
	orthoToPolar(x, y, &targetL, &targetR);
	lastPolarStep = true;
	setPolarTarget(targetL, targetR);
}

void setTarget(float x, float y) {

	float deltaX = x - positionX;
	float deltaY = y - positionY;

	float distance = sqrt(deltaX * deltaX + deltaY + deltaY);

	int numPolarStepsToDo = distance / int(polarStepDistance);
	lastPolarStep = numPolarStepsToDo == 0;

	if(numPolarStepsToDo > 0) {
		float intDistance = numPolarStepsToDo * polarStepDistance;

		float u = deltaX / distance;
		float v = deltaY / distance;

		float intDestinationX = u * intDistance;
		float intDestinationY = v * intDistance;

		float stepX = intDestinationX / numPolarStepsToDo;
		float stepY = intDestinationY / numPolarStepsToDo;

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
	int code = -1;
	if(commandType == ACTION) {
		code = parameters[4];
		switch (code) {
			case 0:
				targetX = parameters[0];
				targetY = parameters[1];

				setTargetDirect(targetX, targetY)

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
				setTimer(sleepTimer, parameters[6]);
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
		if(isTimerElapsed(sleepTimer)) {
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
