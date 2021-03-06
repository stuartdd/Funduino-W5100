#include <Servo.h>

#define BON_AOFF 125 // B ON A OFF
#define BON_AON 80 // BOTH ON
#define BOFF_AON 50 // A ON B OFF
#define BOFF_AOFF 10 // ALL OFF
#define PAUSE 10
#define EN_SV_PIN 8
#define SERVO1PIN 2
#define SERVO_PWR_DELAY 1000
#define STATE_DELAY 4000
#define BAUD 115200

#define SLEEP 1000
Servo servoMotor;
int servoPosition;
long state_timer = 0;
long servo_pwr_timer = 0;
bool servo_pwr_on = false;
bool debug = true;
long loop_time = 0;

int state = 0;
int statePos[] = {BOFF_AOFF, BOFF_AON, BON_AON, BON_AOFF};
/*
  -5----45---85---125--165
  -1    2    3    4    5
*/
void servoInit() {
  servoMotor.attach(SERVO1PIN);
  pinMode(EN_SV_PIN, OUTPUT);
  digitalWrite(EN_SV_PIN, HIGH);
}

void setup() {
    Serial.begin(BAUD);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }

  servoInit();
}

void loop() {
  loop_time = millis();
  
  if ((servo_pwr_on) && (loop_time > servo_pwr_timer)) {
    servo_pwr_on = false;
    digitalWrite(EN_SV_PIN, LOW);
    if (debug) {
      Serial.println("Servo PWR OFF :");
    }
  }
  
  if (loop_time > state_timer) {
    state_timer = loop_time + STATE_DELAY;
    servoPos(statePos[state]);
    if (debug) {
      Serial.print("Servo State :");
      Serial.println(state);
    }
    state++;
    if (state>3) {
      state = 0;
    }
  }
}

void servoPos(int pos) {
  if (servoPosition != pos) {
    /*
       Power up the servo.
       Flag it as ON
       Set a timer to turn it off
    */
    digitalWrite(EN_SV_PIN, HIGH);
    servo_pwr_on = true;
    servo_pwr_timer = millis() + SERVO_PWR_DELAY;
    if (debug) {
      Serial.print(millis());
      Serial.print(": Servo PWR ON :");
      Serial.println(servo_pwr_timer);
    }
    /*
       Set the position of the servo
    */
    servoPosition = pos;
    delay(1);
    servoMotor.write(pos);
  }
}