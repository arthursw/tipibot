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
	int code = -1;
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
