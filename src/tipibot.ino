// #include "mock_arduino.h"
#include <Servo.h>
#include <AccelStepper.h>

int STEP_L = 5;
int DIRECTION_L = 2;

int STEP_R = 6;
int DIRECTION_R = 3;

int ENABLE = 8;
int SERVO = 25;

int maxSpeed = 1000;

float machineWidth = 2000;
float stepsPerRevolution = 200;
float millimetersPerRevolution = 96;
float millimetersPerStep = millimetersPerRevolution / stepsPerRevolution;

int sleepTimer = 0;

float positionX = 0;
float positionY = 0;

float servoAngle = 0;

float polarStepDistance = 1.0;

Servo servo;

typedef enum {  IDLE, MOVE_DIRECT, MOVE_LINEAR, MOVE_PEN, WAIT, SET_POSITION, SET_SPEED, SETUP } Commands;
typedef enum {  NONE, ACTION, SPECIAL } CommandTypes;

Commands command = IDLE;
CommandTypes commandType = NONE;

bool commandReadyToStart = false;
bool commandDone = true;

char commandChar = '0';

const unsigned int NUM_PARAMETERS = 8;
float parameters[NUM_PARAMETERS];
bool parametersSet[NUM_PARAMETERS];

int PARAMETER_X = 0;
int PARAMETER_Y = 1;
int PARAMETER_R = 2;
int PARAMETER_S = 3;
int PARAMETER_G = 4;
int PARAMETER_M = 5;
int PARAMETER_P = 6;
int PARAMETER_F = 7;

int parameterIndex = -1;

float currentValue = 0.0;
bool currentValueSet = false;
int nDecimals = 0;

AccelStepper stepperL(AccelStepper::DRIVER, STEP_L, DIRECTION_L);
AccelStepper stepperR(AccelStepper::DRIVER, STEP_R, DIRECTION_R);

float targetX = 1500;
float targetY = 1500;


bool lastPolarStep = false;
//
// void setup();
// void orthoToPolar(float x, float y, float* l, float* r);
// void polarToOrtho(float l, float r, float* x, float* y);
// unsigned long getTime();
// int createTimer();
// void setTimer(int timerID, unsigned long delay);
// bool isTimerElapsed(int timerID, unsigned long currentTime);
// bool isTimerElapsed(int timerID);
// void setPosition(float x, float y);
// void processIncomingByte (const byte c);
// bool equals(float a, float b);
// void updateMotors();
// void updatePen();
// void setPolarTarget(float l, float r);
// void setTargetDirect(float x, float y);
// void setTarget(float x, float y);
// void readCommand();
// void startCommand();
// void executeCommand();
// void loop();
//
// string testCommands = "G4 P0\nM340 P3 S0\nM340 P3 S1500\nG92 X1150.0512 Y1975.0\nG0 X1125.0 Y1900.0\nG1 X1025.0 Y1800.0\nG90\n";
//
// float testPositionL = 0;
// float testPositionR = 0;
// float testPositionX = 0;
// float testPositionY = 0;
//
// void digitalWrite(int pin, int value) {
//    if(pin == STEP_L && value == 0) {
//      testPositionL += millimetersPerStep * motorDirectionL;
//    }
//    if(pin == STEP_R && value == 0) {
//     testPositionR += millimetersPerStep * motorDirectionR;
//    }
//    if(pin == STEP_L || pin == STEP_R){
//    polarToOrtho(testPositionL, testPositionR, &testPositionX, &testPositionY);
//    }
// }
//
// int main() {
//
//     setup();
//  Serial.fakeFeed(testCommands);
//  for(int i=0 ; i<10000000 ; i++) {
//    loop();
//  }
//     return 0;
// }

void setup() {
  commandReadyToStart = false;
  commandDone = true;

  pinMode(ENABLE, OUTPUT);
  digitalWrite(ENABLE, 0);
  /*
    pinMode(STEP_L, OUTPUT);
    pinMode(DIRECTION_L, OUTPUT);
    pinMode(STEP_R, OUTPUT);
    pinMode(DIRECTION_R, OUTPUT);
  */
  servo.attach(SERVO);

  Serial.begin(115200);
  Serial.println("Tipibot ready.");

  stepperL.setCurrentPosition(0);
  stepperR.setCurrentPosition(0);

  stepperL.setMaxSpeed(maxSpeed);
  stepperR.setMaxSpeed(maxSpeed);
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
  targetX = x;
  targetY = y;
  positionX = x;
  positionY = y;
  float positionL = 0;
  float positionR = 0;
  orthoToPolar(x, y, &positionL, &positionR);
  stepperL.setCurrentPosition(positionL / millimetersPerStep);
  stepperR.setCurrentPosition(positionR / millimetersPerStep);
}

void processIncomingByte (const byte c)
{
  if (isdigit(c))
  {
    currentValueSet = true;
    if (nDecimals <= 0) {
      currentValue *= 10;
      currentValue += c - '0';
    } else {
      currentValue += pow(10, -nDecimals) * (c - '0');
      nDecimals++;
    }
  }
  else if (c == '.') {
    nDecimals = 1;
  }
  else
  {
    if (parameterIndex != -1 && currentValueSet) {
      parameters[parameterIndex] = currentValue;
      parametersSet[parameterIndex] = true;
    }

    nDecimals = 0;
    currentValue = 0;
    currentValueSet = false;

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
      case 'F':
        parameterIndex = 7;
        break;
      case '\n':
        if (commandChar == 'G') {
          commandType = ACTION;
        } else if (commandChar == 'M') {
          commandType = SPECIAL;
        }
        if (commandType == ACTION || commandType == SPECIAL) {
          commandReadyToStart = true;
        }
/*
        Serial.print("end of command: command done: ");
        Serial.print(commandDone);
        Serial.print(" - command char: ");
        Serial.println(commandChar);
*/
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

unsigned long updateMotorsCount = 0;

void printPositions(const char* prefix) {
  if (updateMotorsCount % 50000 == 0) {
    Serial.println(prefix);
    Serial.print("Current position: l: ");
    Serial.print(stepperL.currentPosition());
    Serial.print(", r: ");
    Serial.println(stepperR.currentPosition());
    Serial.print("Target position: l: ");
    Serial.print(stepperL.targetPosition());
    Serial.print(", r: ");
    Serial.println(stepperR.targetPosition());
  }
  updateMotorsCount++;
}

void updatePositions(long currentPositionL, long currentPositionR) {
  float positionL = currentPositionL * millimetersPerStep;
  float positionR = currentPositionR * millimetersPerStep;

  polarToOrtho(positionL, positionR, &positionX, &positionY);
}

void updateMotorsLinear() {
  stepperL.runSpeedToPosition();
  stepperR.runSpeedToPosition();

  // printPositions("updateMotorsLinear");

  long currentPositionL = stepperL.currentPosition();
  long currentPositionR = stepperR.currentPosition();

  updatePositions(currentPositionL, currentPositionR);

  if (currentPositionL == stepperL.targetPosition() && currentPositionR == stepperR.targetPosition()) {
    if (lastPolarStep) {
      commandDone = true;
      lastPolarStep = false;
    } else {
      setTarget(targetX, targetY);
    }
  }
}

void updateMotorsDirect() {
  stepperL.runSpeedToPosition();
  stepperR.runSpeedToPosition();

  // printPositions("updateMotorsDirect");

  long currentPositionL = stepperL.currentPosition();
  long currentPositionR = stepperR.currentPosition();

  updatePositions(currentPositionL, currentPositionR);

  commandDone = currentPositionL == stepperL.targetPosition() && currentPositionR == stepperR.targetPosition();
}

void updatePen() {
  servo.write(servoAngle);
  commandDone = true;
}

void setPolarTarget(float l, float r) {
  float targetNumStepsL = round(l / millimetersPerStep);
  float targetNumStepsR = round(r / millimetersPerStep);

  stepperL.moveTo(targetNumStepsL);
  stepperR.moveTo(targetNumStepsR);

  long currentPositionL = stepperL.currentPosition();
  long currentPositionR = stepperR.currentPosition();

  long deltaL = targetNumStepsL - currentPositionL;
  long deltaR = targetNumStepsR - currentPositionR;

  int motorDirectionL = deltaL >= 0 ? 1 : -1;
  int motorDirectionR = deltaR >= 0 ? 1 : -1;

  unsigned long motorNumStepsToDoL = abs(deltaL);
  unsigned long motorNumStepsToDoR = abs(deltaR);

  float numStepsPerSecondsL = 0;
  float numStepsPerSecondsR = 0;

  if (motorNumStepsToDoL <= motorNumStepsToDoR) {
    numStepsPerSecondsR = maxSpeed;
    numStepsPerSecondsL = motorNumStepsToDoL > 0 ? numStepsPerSecondsR * motorNumStepsToDoL / float(motorNumStepsToDoR) : 0;
  } else {
    numStepsPerSecondsL = maxSpeed;
    numStepsPerSecondsR = motorNumStepsToDoR > 0 ? numStepsPerSecondsL * motorNumStepsToDoR / float(motorNumStepsToDoL) : 0;
  }

  stepperL.setSpeed(numStepsPerSecondsL * motorDirectionL);
  stepperR.setSpeed(numStepsPerSecondsR * motorDirectionR);

  /*
  Serial.print("setPolarTarget: l: ");
  Serial.print(l);
  Serial.print(", r: ");
  Serial.println(r);
  Serial.print("millimetersPerStep: ");
  Serial.println(millimetersPerStep);
  Serial.print("currentPosition: l: ");
  Serial.print(currentPositionL);
  Serial.print(", r: ");
  Serial.println(currentPositionR);
  Serial.print("targetNumSteps: l: ");
  Serial.print(targetNumStepsL);
  Serial.print(", r: ");
  Serial.println(targetNumStepsR);
  Serial.print("delta: l: ");
  Serial.print(deltaL);
  Serial.print(", r: ");
  Serial.println(deltaR);
  Serial.print("motorDirection: l: ");
  Serial.print(motorDirectionL);
  Serial.print(", r: ");
  Serial.println(motorDirectionR);
  Serial.print("motorNumStepsToDo: l: ");
  Serial.print(motorNumStepsToDoL);
  Serial.print(", r: ");
  Serial.println(motorNumStepsToDoR);
  Serial.print("numStepsPerSeconds: l: ");
  Serial.print(numStepsPerSecondsL);
  Serial.print(", r: ");
  Serial.println(numStepsPerSecondsR);
  */
}

void setTargetDirect(float x, float y) {
  float targetL = 0.0;
  float targetR = 0.0;
  orthoToPolar(x, y, &targetL, &targetR);
  /*
  Serial.print("setTargetDirect: x: ");
  Serial.print(x);
  Serial.print(", y: ");
  Serial.println(y);
  Serial.print("target: l: ");
  Serial.print(targetL);
  Serial.print(", r: ");
  Serial.println(targetR);
  */
  setPolarTarget(targetL, targetR);
}

void setTarget(float x, float y) {

  float deltaX = x - positionX;
  float deltaY = y - positionY;

  float distance = sqrt(deltaX * deltaX + deltaY + deltaY);

  int numPolarStepsToDo = distance / int(polarStepDistance);
  lastPolarStep = numPolarStepsToDo == 0;

  float subTargetX = x;
  float subTargetY = y;

  if (numPolarStepsToDo > 0) {

    float u = deltaX / distance;
    float v = deltaY / distance;

    float subDeltaX = u * polarStepDistance;
    float subDeltaY = v * polarStepDistance;

    subTargetX = positionX + subDeltaX;
    subTargetY = positionY + subDeltaY;
  }

  float destinationL = 0;
  float destinationR = 0;
  orthoToPolar(subTargetX, subTargetY, &destinationL, &destinationR);
  /*
  Serial.print("SetTarget: x: ");
  Serial.print(x);
  Serial.print(", y: ");
  Serial.println(y);
  Serial.print("subTarget: x: ");
  Serial.print(subTargetX);
  Serial.print(", y: ");
  Serial.println(subTargetY);
  Serial.print("destination: l: ");
  Serial.print(destinationL);
  Serial.print(", r: ");
  Serial.println(destinationR);
  Serial.print("numPolarStepsToDo > 0: ");
  Serial.print(numPolarStepsToDo > 0);
  */
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
  if (commandType == ACTION) {
    code = parameters[4];
    //Serial.print("startCommand action, code: ");
    //Serial.println(code);
    switch (code) {
      case 0:

        if(parametersSet[PARAMETER_X] && parametersSet[PARAMETER_Y]) {
          targetX = parameters[PARAMETER_X];
          targetY = parameters[PARAMETER_Y];
          setTargetDirect(targetX, targetY);
          command = MOVE_DIRECT;
        } else {
          command = SET_SPEED;
          maxSpeed = parameters[PARAMETER_F];
          commandDone = true;
        }

        break;
      case 1:
        targetX = parameters[PARAMETER_X];
        targetY = parameters[PARAMETER_Y];

        setTarget(targetX, targetY);
        command = MOVE_LINEAR;
        break;
      // case 2:
      //  circleClockWise(x, y, r);
      //  break;
      // case 3:
      //  circleCounterClockWise(x, y, r);
      //  break;
      case 4:
        // setTimer(sleepTimer, parameters[6]);
        command = WAIT;
        commandDone = true;
        break;
      // case 21:
      //  setMillimeters();
      //  break;
      // case 90:
      //  setAbsolutePosition();
      //  break;
      // case 91:
      //  setRelativePosition();
      //  break;
      case 92:
        setPosition(parameters[PARAMETER_X], parameters[PARAMETER_Y]);
        command = SET_POSITION;
        commandDone = true;
        break;
      default:
        commandDone = true;
        Serial.println("Unknown G gcode.");
    }
  } else if (commandType == SPECIAL) {
    code = parameters[5];
    switch (code) {
      case 1:
        // stop();
        commandDone = true;
        break;
      case 4:
        if (parameters[PARAMETER_X] > 0) {
          machineWidth = parameters[PARAMETER_X];
        }
        if (parameters[PARAMETER_S] > 0) {
          stepsPerRevolution = parameters[PARAMETER_S];
        }
        if (parameters[PARAMETER_P] > 0) {
          millimetersPerRevolution = parameters[PARAMETER_P];
        }
        millimetersPerStep = millimetersPerRevolution / stepsPerRevolution;
        command = SETUP;
        commandDone = true;
        break;
      case 340:
        servoAngle = parameters[PARAMETER_S];
        command = MOVE_PEN;
        break;
      default:
        commandDone = true;
        Serial.println("Unknown M gcode.");
    }
  }
  for (unsigned int i = 0 ; i < NUM_PARAMETERS ; i++) {
    parameters[i] = -1.0;
   parametersSet[i] = false;
  }
  Serial.println("ok");
}

void executeCommand() {

  if (command == MOVE_DIRECT) {
    updateMotorsDirect();
  } else if (command == MOVE_LINEAR) {
    updateMotorsLinear();
  } else if (command == MOVE_PEN) {
    updatePen();
  } else if (command == WAIT) {
    //if(isTimerElapsed(sleepTimer)) {
    //  commandDone = true;
    //}
    commandDone = true;
  }

  if (commandDone) {
    command = IDLE;
  }
}

void loop ()
{

  if (!commandReadyToStart) {
    readCommand();
  }

  if (commandReadyToStart && commandDone) {
    startCommand();

/*
    Serial.println("Start command: ");
    switch (command) {
      case MOVE_DIRECT:
        Serial.println("Move direct");
        break;
      case MOVE_LINEAR:
        Serial.println("Move linear");
        break;
      case MOVE_PEN:
        Serial.println("Move pen");
        break;
      case WAIT:
        Serial.println("Wait");
        break;
      default:
        Serial.println(command);
    }*/
  }

  if (!commandDone) {
    Commands previousCommand = command;
    executeCommand();
    if (commandDone) {
      /*
      Serial.println("Command done: ");
      switch (previousCommand) {
        case MOVE_DIRECT:
          Serial.println("Move direct");
          break;
        case MOVE_LINEAR:
          Serial.println("Move linear");
          break;
        case MOVE_PEN:
          Serial.println("Move pen");
          break;
        case WAIT:
          Serial.println("Wait");
          break;
        default:
          Serial.println(command);
      }*/
    }
  }
}
