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

float servoAngle = 0;

float polarStepDistance = 1.0;

Servo servo;

typedef enum {  IDLE, MOVE_DIRECT, MOVE_LINEAR, MOVE_PEN, WAIT, SET_POSITION, SETUP } Commands;
typedef enum {  NONE, ACTION, SPECIAL } CommandTypes;

Commands command = IDLE;
CommandTypes commandType = NONE;

bool commandReadyToStart = false;
bool commandDone = true;

char commandChar = '0';

const unsigned int NUM_PARAMETERS = 7;
float parameters[NUM_PARAMETERS];
int parameterIndex = -1;

float currentValue = 0.0;
int nDecimals = 0;

int motorStateL = 0;
int motorStateR = 0;

int motorTimerL = 0;
int motorTimerR = 0;

int motorStepLengthL = 0;
int motorStepLengthR = 0;

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

string testCommands = "G4 P0\nM340 P3 S0\nM340 P3 S1500\nG92 X1150.0512 Y1975.0\nG0 X1125.0 Y1900.0\nG1 X1025.0 Y1800.0\nG90\n";

float testPositionL = 0;
float testPositionR = 0;
float testPositionX = 0;
float testPositionY = 0;

void digitalWrite(int pin, int value) {
   if(pin == STEP_L && value == 0) {
	   testPositionL += millimetersPerStep * motorDirectionL;
   }
   if(pin == STEP_R && value == 0) {
	   testPositionR += millimetersPerStep * motorDirectionR;
   }
   if(pin == STEP_L || pin == STEP_R){
		polarToOrtho(testPositionL, testPositionR, &testPositionX, &testPositionY);   
   }
}

int main() {
	
    setup();
	Serial.fakeFeed(testCommands);
	for(int i=0 ; i<10000000 ; i++) {
		loop();
	}
    return 0;
}

void setup() {
	commandReadyToStart = false;
	commandDone = true;
	
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
	motorTimerR = createTimer();
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

const unsigned int NUM_TIMERS = 5;
unsigned long timers[NUM_TIMERS];
bool timerOverflows[NUM_TIMERS];
int nTimers = 0;
unsigned long lastTime = 0;

unsigned long getTime() {
	unsigned long currentTime = micros();
	if(lastTime > currentTime) { 	// overflow!
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
	return !timerOverflows[timerID] && currentTime > timers[timerID];
}

bool isTimerElapsed(int timerID) {
	unsigned long currentTime = getTime();
	return isTimerElapsed(timerID, currentTime);
}

void setPosition(float x, float y) {
	targetX = x;
	targetY = y;
	subTargetX = x;
	subTargetY = y;
	positionX = x;
	positionY = y;
	orthoToPolar(x, y, &positionL, &positionR);
	testPositionL = positionL;
	testPositionR = positionR;
	testPositionX = positionX;
	testPositionY = positionY;
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
		if(parameterIndex != -1) {
			parameters[parameterIndex] = currentValue;
		}
		
		nDecimals = 0;
		currentValue = 0;

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

bool equals(float a, float b) {
	return fabs(a - b) <= millimetersPerStep;
}

void updateMotors() {
	bool positionChanged = false;

	if(motorNumStepsDoneL < motorNumStepsToDoL && isTimerElapsed(motorTimerL)) {
		motorStateL = motorStateL == 0 ? 1 : 0;
		digitalWrite(STEP_L, motorStateL);
		setTimer(motorTimerL, motorStepLengthL);
		if(motorStateL == 0) {
			motorNumStepsDoneL++;
			positionL += millimetersPerStep * motorDirectionL;
			positionChanged = true;
		}
	}

	if(motorNumStepsDoneR < motorNumStepsToDoR && isTimerElapsed(motorTimerR)) {
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
			cout << "Position reached: XY: " << positionX << ", " << positionY << " - LR: " << positionL << ", " << positionR <<  endl;
			cout << "   Test position: XY: " << testPositionX << ", " << testPositionY << " - LR: " << testPositionL << ", " << testPositionR <<  endl;
			commandDone = true;
			lastPolarStep = false;
		} else {
			cout << "Subposition reached: XY: " << positionX << ", " << positionY << " - LR: " << positionL << ", " << positionR <<  endl;
			cout << "      Test position: XY: " << testPositionX << ", " << testPositionY << " - LR: " << testPositionL << ", " << testPositionR <<  endl;
			setTarget(targetX, targetY);
		}
	}
}

void updatePen() {
	servo.write(servoAngle);
	cout << "Set servo to " << servoAngle << endl;
	commandDone = true;
}

void setPolarTarget(float l, float r) {
	float deltaL = l - positionL;
	float deltaR = r - positionR;
	motorNumStepsToDoL = round(fabs(deltaL) / millimetersPerStep);
	motorNumStepsToDoR = round(fabs(deltaR) / millimetersPerStep);
	
	if(deltaL >= 0 && motorDirectionL != 1) {
		motorDirectionL = 1;
		digitalWrite(DIRECTION_L, 1);
	}
	else if(deltaL < 0 && motorDirectionL != -1) {
		motorDirectionL = -1;
		digitalWrite(DIRECTION_L, 0);
	}
	
	if(deltaR >= 0 && motorDirectionR != 1) {
		motorDirectionR = 1;
		digitalWrite(DIRECTION_R, 1);
	}
	else if(deltaR < 0 && motorDirectionR != -1) {
		motorDirectionR = -1;
		digitalWrite(DIRECTION_R, 0);
	}

	if(motorNumStepsToDoL <= motorNumStepsToDoR) {
		motorStepLengthR = 175;
		motorStepLengthL = motorNumStepsToDoL > 0 ? motorStepLengthR * motorNumStepsToDoR / motorNumStepsToDoL : 0;
	} else {
		motorStepLengthL = 175;
		motorStepLengthR = motorNumStepsToDoR > 0 ? motorStepLengthL * motorNumStepsToDoL / motorNumStepsToDoR : 0;
	}
	motorNumStepsDoneL = 0;
	motorNumStepsDoneR = 0;

	setTimer(motorTimerL, 0);
	setTimer(motorTimerR, 0);
	
	cout << "Set polar target: LR: " << l << ", " << r << endl;
}

void setTargetDirect(float x, float y) {
	float targetL = 0.0;
	float targetR = 0.0;
	orthoToPolar(x, y, &targetL, &targetR);
	lastPolarStep = true;
	targetX = x;
	targetY = y;
	subTargetX = x;
	subTargetY = y;
	cout << "Set target direct: xy: " << targetX << ", " << targetY << endl;
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
		subTargetY = positionY + stepY;
	} else {
		subTargetX = x;
		subTargetY = y;
	}

	float destinationL = 0;
	float destinationR = 0;
	orthoToPolar(subTargetX, subTargetY, &destinationL, &destinationR);
	cout << "Set target linear: xy: " << targetX << ", " << targetY << endl;
	cout << "                 : sub-xy: " << subTargetX << ", " << subTargetY << endl;
	setPolarTarget(destinationL, destinationR);
}

// state machine

void readCommand() {
	while (Serial.available() && !commandReadyToStart) {
		char c = Serial.read();
		processIncomingByte(c);
	}
}

void startCommand() {
	commandReadyToStart = false;
	commandDone = false;
	int code = -1;
	if(commandType == ACTION) {
		code = parameters[4];
		switch (code) {
			case 0:
				
 				targetX = parameters[0];
				targetY = parameters[1];
				
				cout << "Start command: move direct to " << targetX << ", " << targetY << endl;
				
				setTargetDirect(targetX, targetY);

				command = MOVE_DIRECT;
			break;
			case 1:
				targetX = parameters[0];
				targetY = parameters[1];
				
				cout << "Start command: move linear to " << targetX << ", " << targetY << endl;
				
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
				cout << "Start command: wait " << parameters[6] << endl;
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
				cout << "Start command: set position " << parameters[0] << ", " << parameters[1] << endl;
				setPosition(parameters[0], parameters[1]);
				command = SET_POSITION;
				commandDone = true;
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
				cout << "Start command: setup: machineWidth: " << machineWidth<< ", stepsPerRevolution: " << stepsPerRevolution << ", millimetersPerRevolution: " << millimetersPerRevolution << ", millimetersPerSteps: " << millimetersPerStep << endl;
			break;
			case 340:
				servoAngle = parameters[3];
				command = MOVE_PEN;
				cout << "Start command: move pen " << servoAngle << endl;
			break;
			default:
				Serial.println("Unknown M gcode.");
		}
	}
	for(int i=0 ; i<NUM_PARAMETERS ; i++) {
		parameters[i] = -1.0;
	}
	Serial.println("ok");
	cout << endl;
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
		command = IDLE;
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
