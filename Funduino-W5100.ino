
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <avr/pgmspace.h>
#include <EEPROM.h>
#include <avr/wdt.h>
#include <Servo.h>

#define DEV_ID "125"
/*
   Define servo positions for microswitch positions
*/
#define BON_AOFF 125 // B ON A OFF
#define BON_AON 80 // BOTH ON
#define BOFF_AON 50 // A ON B OFF
#define BOFF_AOFF 10 // ALL OFF
#define PAUSE 10
#define SERVO1PIN 2

#define MAC 0x34, 0xA9, 0x5D, 0x09, 0xCF, 0xED
#define IP 192, 168, 1, 177
#define GATEWAY 192, 168, 1, 1
#define SUBNET 255, 255, 255, 0
#define NTP_IP 129,6,15,30
#define PORT 80
#define BAUD 115200
/*
   Define a physical pin for each function.
*/
#define LEDACTPIN 3
#define LED_A_PIN 4
#define LED_B_PIN 5
#define BTN_A_PIN 6
#define BTN_B_PIN 7
#define EN_SV_PIN 8
/*
   Define a physical pin for each analog value and digital sensors
*/
#define VOLTAGE_0_PIN A0
#define VOLTAGE_1_PIN A1
#define VOLTAGE_2_PIN A2
#define VOLTAGE_3_PIN A3
/*
   Define a physical pin for each digital sensors
*/
#define D4_PIN A4
#define D5_PIN A5
#define D6_PIN A6
#define D7_PIN A6
/*
   Define a number for each switch
   These are used as indexes in the the bits[]
   They must start at 0 and end one less that the array size.
*/
#define SWITCH_A 0
#define SWITCH_B 1
/*
   Define which bit represents a switch in the switchBits value below.
*/
#define SWITCH_A_BIT B00000001
#define SWITCH_B_BIT B00000010

#define S_1_HOUR 3600
#define S_2_HOUR 7200
#define S_3_HOUR 10800
#define S_24_HOUR 86400

#define S_1_BOOST 3600
#define S_2_BOOST 7200
#define S_3_BOOST 10800
#define S_24_BOOST 86400

#define WDT_DELAY 4000
#define BTN_TIMER_DELAY 100
#define SERVO_PWR_DELAY 1030
#define CLEAR_BUTTON_DELAY 700
#define UPDATE_SWITCH_DELAY 900

/*
   Constants for Temppreture chip MCP9701 and Voltage conversion
*/
#define TEMP_CONST (float)19.53   // mV for °C temperature constant for the  MCP9701/A
#define VOUT_MV_Z (float)400      //  sensor output voltage in mV at 0°C (400 * 1000)
#define IN_TO_VOLTS (float)4.814  // (Vref  / 1024) * 1000

#define HTTP_200 "HTTP/1.1 200 OK"
#define HTTP_CONTENT_LEN "Content-Length: "
#define HTTP_CONTENT_TYPE_JSON "Content-Type: application/json"

/*
   Define CODE macros.
   Used to inline code and reduce the call stack while declaring function once.
*/
#define M_VIN_TO_CELC(vIn)((vIn - VOUT_MV_Z) / TEMP_CONST)


enum REQ_DATA {
  Heading,
  ContentLen,
  Path,
  Body,
  Skip
};

/*
   Light flash sequence.
*/
#define SEQ_STEPS 9
#define SEQ_3H 0
#define SEQ_2H 2
#define SEQ_1H 4
#define SEQ_OFF 6
#define SEQ_TIME 200

const int seqOut[] = {HIGH, LOW, HIGH, LOW, HIGH, LOW, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
int seq_step_all = 0;

int seq_base_sw_a = SEQ_OFF;
int seq_index_sw_a = SEQ_OFF;
int seq_base_sw_b = SEQ_OFF;
int seq_index_sw_b = SEQ_OFF;

long seq_led_timer = 0;

const String DEV_ID_STR = DEV_ID;
const String DEV_A_NAME = "Heating";
const String DEV_B_NAME = "Water";
const String DEV_T_A_ID = "ta";
const String DEV_T_B_ID = "tb";
const String DEV_S_A_ID = "sa";
const String DEV_S_B_ID = "sb";


// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
const byte mac[] = {MAC};

IPAddress ip(IP);
IPAddress subnet(SUBNET);
IPAddress gateway(GATEWAY);
IPAddress timeSrvr(NTP_IP);

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(PORT);

EthernetUDP ethernet_UDP;
byte ntpMessageBuffer[48];
int daylightSaving = S_1_HOUR;

Servo servoMotor;
int servoPosition;

byte currentSwitchState = 0;
bool debug = false;

//
boolean inProgress = false;

float voltage0 = 0;
float voltage1 = 0;
float voltage2 = 0;
float voltage3 = 0;

/*
   Properties set via interface.
*/
long boostSecondsAB[] = {0, 0};
bool scheduleOffForAB[] = {false, false};
/*
   Properties set as a result of processing the above and the schedule.
   This occures using updateSwitchTimer.
   These are used to drive the servos and the reply to the client
*/
long timeUntilNextChange[] = {0, 0};
long timeRemainNextChange[] = {0, 0};
bool statusForAB[] = {false, false};

long loop_time = 0;
long timer_wdt = 0;
long last_seconds_set_time = 0;
long last_seconds_diff = 0;
long last_seconds_temp = 0;
long last_seconds_offset = 0;
long last_seconds = 0;
long button_timer = 0;
long updateSwitchTimer = 0;
long buttonClearTimerA = 0;
long buttonClearTimerB = 0;
long servo_pwr_timer = 0;
bool servo_pwr_on = false;
bool button_a_pressed = false;
bool button_b_pressed = false;
int buttonStateAB[] = {0, 0};


void setup() {
  wdt_enable(WDTO_8S);
  // Open serial communications and wait for port to open:
  Serial.begin(BAUD);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  Serial.println("BOOT");

  /*
    Read the switch state from the EPROM
  */
  digitalPins();

  servoInit();

  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip, subnet, gateway);
  server.begin();

  if (debug) {
    Serial.print("server is at ");
    Serial.println(Ethernet.localIP());
  }

  last_seconds_set_time = millis();
  digitalWrite(LEDACTPIN, HIGH);
  ethernet_UDP.begin(2390);

  last_seconds = getTime();
  digitalWrite(LEDACTPIN, LOW);

}


void loop() {
  loop_time = millis();
  /*
     Check for wrap around millis
  */
  if (loop_time < last_seconds_set_time) {
    last_seconds_set_time = millis();
  }
  /*
     Count seconds!
  */
  if (loop_time > (last_seconds_set_time + 1000)) {
    last_seconds_offset = loop_time - last_seconds_set_time;
    last_seconds_set_time = loop_time;
    last_seconds_diff = last_seconds_diff + last_seconds_offset;
    last_seconds_temp = last_seconds_diff / 1000;
    last_seconds = last_seconds + last_seconds_temp;
    last_seconds_diff = last_seconds_diff - (last_seconds_temp * 1000);
  }
  /*
     Reset if we dont get here in time!
  */
  if (loop_time > timer_wdt) {
    /*
       Phew we got here in time so reset the watchdog timer
    */
    timer_wdt = loop_time + WDT_DELAY;
    wdt_reset();
  }
  /*
     Only look at the buttons every BTN_TIMER_DELAY ms
  */
  if (loop_time > button_timer) {

    button_timer = loop_time + BTN_TIMER_DELAY;
    if (!digitalRead(BTN_A_PIN)) {
      if (!button_a_pressed) {
        button_a_pressed = true;
        seq_base_sw_a = SEQ_OFF;
        buttonClearTimerA = loop_time + CLEAR_BUTTON_DELAY;
        buttonStateAB[SWITCH_A]++;
      }
    } else {
      button_a_pressed = false;
      if ((buttonClearTimerA > 0) && (loop_time > buttonClearTimerA)) {
        setBoostTime(SWITCH_A);
        updateSwitchState(SWITCH_A);
        buttonStateAB[SWITCH_A] = 0;
        buttonClearTimerA = 0;
      }
    }

    if (!digitalRead(BTN_B_PIN)) {
      if (!button_b_pressed) {
        button_b_pressed = true;
        seq_base_sw_b = SEQ_OFF;
        buttonClearTimerB = loop_time + CLEAR_BUTTON_DELAY;
        buttonStateAB[SWITCH_B]++;
      }
    } else {
      button_b_pressed = false;
      if ((buttonClearTimerB > 0) && (loop_time > buttonClearTimerB)) {
        setBoostTime(SWITCH_B);
        updateSwitchState(SWITCH_B);
        buttonStateAB[SWITCH_B] = 0;
        buttonClearTimerB = 0;
      }
    }
  }
  /*
     Do we need to change the state of the heating/hot water
  */
  if (loop_time > updateSwitchTimer) {
    updateSwitchTimer = loop_time + UPDATE_SWITCH_DELAY;
    setSwitchState();
    Serial.print("Offset:");
    Serial.println(last_seconds);
  }

  /*
     If servo power is ON and it is time to turn it OFF
  */
  if ((servo_pwr_on) && (loop_time > servo_pwr_timer)) {
    servo_pwr_on = false;
    digitalWrite(EN_SV_PIN, LOW);
    digitalWrite(LEDACTPIN, LOW);
    if (debug) {
      Serial.println(": Servo PWR OFF :");
    }
  }

  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;

    REQ_DATA state = Heading;

    String header = "";
    String contentLength = "";
    String method = "";
    String path = "";
    String body = "";
    char c = ' ';
    boolean notFinished = true;

    while (client.connected() && notFinished) {
      digitalWrite(LEDACTPIN, HIGH);
      inProgress = true;

      if (client.available()) {
        c = client.read();

        if (c == '\n') {
          if (currentLineIsBlank) {
            notFinished = false;
          } else {
            currentLineIsBlank = true;
            header = "";
            state = Heading;
          }
        } else {
          if (c >= ' ') {
            currentLineIsBlank = false;
            switch (state) {
              case ContentLen:
                if ((c >= '0') && (c <= '9')) {
                  contentLength = contentLength + c;
                } else {
                  state = Skip;
                }
                break;

              case Path:
                if (c == ' ') {
                  state = Skip;
                } else {
                  path = path + c;
                }
                break;

              case Heading:
                header = header + c;
                if (header == "POST ") {
                  method = "POST";
                  state = Path;
                }
                if (header == "GET ") {
                  method = "GET";
                  state = Path;
                }
                if (header == "Content-Length: ") {
                  state = ContentLen;
                }
                /*
                   Dont concatenate more that we have to.
                   17 is  enough to find "Content-Length: "
                */
                if (header.length() > 17) {
                  state = Skip;
                }
                break;
            } // end switch
          } // end if c >= ' '
        } // end else
      } // end if available
    } // end while connected

    if (contentLength.length() != 0) {
      int count = contentLength.toInt();
      while (client.connected() && count > 0) {
        if (client.available()) {
          c = client.read();
          body = body + c;
          count--;
        }
      }
    }

    if (path.indexOf("/debugon") >= 0) {
      debug = true;
    } else if (path.indexOf("/debugoff") >= 0) {
      debug = false;
    }

    if (debug) {
      Serial.println(">> (rec) ----------------------");
      Serial.println("contentLength: " + contentLength);
      Serial.println("Method: " + method);
      Serial.println("Path: " + path);
      Serial.println("Body: " + body);
      Serial.println("<< (resp) ---------------------");
    }
    sendResponse(client, method, path, body);

    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    inProgress = false;
    digitalWrite(LEDACTPIN, LOW);
    if (debug) {
      Serial.println("Connection Closed");
    }
  }

  if (loop_time > seq_led_timer) {
    seq_led_timer = loop_time + SEQ_TIME;

    if (statusForAB[SWITCH_A]) {
      if (timeRemainNextChange[SWITCH_A] <= S_1_BOOST) {
        seq_base_sw_a = SEQ_1H;
      } else if (timeRemainNextChange[SWITCH_A] <= S_2_BOOST) {
        seq_base_sw_a = SEQ_2H;
      } else if (timeRemainNextChange[SWITCH_A] <= S_3_BOOST) {
        seq_base_sw_a = SEQ_3H;
      } else {
        seq_base_sw_a = SEQ_OFF;
      }
      digitalWrite(LED_A_PIN, seqOut[seq_index_sw_a]);
    } else {
      digitalWrite(LED_A_PIN, LOW);
      seq_base_sw_a = SEQ_OFF;
    }

    if (statusForAB[SWITCH_B]) {
      digitalWrite(LED_B_PIN, seqOut[seq_index_sw_b]);
      if (timeRemainNextChange[SWITCH_B] <= S_1_BOOST) {
        seq_base_sw_b = SEQ_1H;
      } else if (timeRemainNextChange[SWITCH_B] <= S_2_BOOST) {
        seq_base_sw_b = SEQ_2H;
      } else if (timeRemainNextChange[SWITCH_B] <= S_3_BOOST) {
        seq_base_sw_b = SEQ_3H;
      } else {
        seq_base_sw_b = SEQ_OFF;
      }
    } else {
      digitalWrite(LED_B_PIN, LOW);
      seq_base_sw_b = SEQ_OFF;
    }

    seq_index_sw_a++;
    seq_index_sw_b++;
    seq_step_all++;
    if (seq_step_all >= SEQ_STEPS) {
      seq_index_sw_a = seq_base_sw_a;
      seq_index_sw_b = seq_base_sw_b;
      seq_step_all = 0;
    }

  }
}

void nextScheduledTime(int switchId) {
  timeUntilNextChange[switchId] = 0;
  timeRemainNextChange[switchId] = 0;
  statusForAB[switchId] = false;
}


void setSwitchState() {
  int bits = 0;
  if (statusForAB[SWITCH_A]) {
    bits = bits | SWITCH_A_BIT;
  }
  if (statusForAB[SWITCH_B]) {
    bits = bits | SWITCH_B_BIT;
  }
  if (currentSwitchState == bits) {
    return;
  }
  currentSwitchState = bits;

  switch (bits) {
    case B00000000:
      servoPos(BOFF_AOFF);
      break;
    case B00000001:
      servoPos(BOFF_AON);
      break;
    case B00000010:
      servoPos(BON_AOFF);
      break;
    case B00000011:
      servoPos(BON_AON);
      break;
  }
}


void setBoostTime(int switchId) {
  switch (buttonStateAB[switchId]) {
    case 1:
      boostSecondsAB[switchId] = last_seconds + S_1_BOOST;
      break;
    case 2:
      boostSecondsAB[switchId] = last_seconds + S_2_BOOST;
      break;
    case 3:
      boostSecondsAB[switchId] = last_seconds + S_3_BOOST;
      break;
    default:
      buttonStateAB[switchId] = 0;
      boostSecondsAB[switchId] = 0;
      break;
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
    digitalWrite(LEDACTPIN, HIGH);
    servo_pwr_on = true;
    servo_pwr_timer = millis() + SERVO_PWR_DELAY;
    if (debug) {
      Serial.print(millis());
      Serial.println(": Servo PWR ON :");
    }
    /*
       Set the position of the servo
    */
    servoPosition = pos;
    delay(1);
    servoMotor.write(pos);
  }
}

void digitalPins() {
  pinMode(LEDACTPIN, OUTPUT);
  digitalWrite(LEDACTPIN, LOW);
  pinMode(LED_A_PIN, OUTPUT);
  digitalWrite(LEDACTPIN, LOW);
  pinMode(LED_B_PIN, OUTPUT);
  digitalWrite(LED_B_PIN, LOW);
  pinMode(BTN_A_PIN, INPUT_PULLUP);
  pinMode(BTN_B_PIN, INPUT_PULLUP);
  // Init analog pins for sensor input
  pinMode(D4_PIN, INPUT_PULLUP);
  pinMode(D5_PIN, INPUT_PULLUP);
  pinMode(D6_PIN, INPUT_PULLUP);
  pinMode(D7_PIN, INPUT_PULLUP);
}

void servoInit() {
  servoMotor.attach(SERVO1PIN);
  pinMode(EN_SV_PIN, OUTPUT);
  digitalWrite(EN_SV_PIN, LOW);
  digitalWrite(LEDACTPIN, LOW);
}



String param(String path, String key, int index) {
  return "+++" + path.substring(index + key.length());
}

/*
   -------------------------------------------------------------
   JSON Responses section
   -------------------------------------------------------------
*/
void sendResponse(EthernetClient client, String method, String path, String body) {
  if (method == "GET") {
    path.toLowerCase();
    if (path.indexOf("/switch") >= 0) {
      handleSwitch(path);
      updateSwitchState(SWITCH_A);
      updateSwitchState(SWITCH_B);
      _sendSwitchStatus(client);
    } else {
      if (path.indexOf("/data") >= 0) {
        _sendDataValues(client);
      } else {
        _sendError(client, "Path not recognised", "404 Not Found");
      }
    }
  } else {
    _sendError(client, "Only GET is Supported", "405 Method Not Allowed");
  }
  client.println("");
  client.flush();
}


void handleSwitch(String path) {

  /*
    long boostSecondsAB[] = {0, 0};
    bool  = {false, false};

  */
  if (path.indexOf(DEV_T_A_ID + "=b1") > 0) {
    boostSecondsAB[SWITCH_A] = last_seconds + S_1_BOOST;
  }
  if (path.indexOf(DEV_T_A_ID + "=b2") > 0) {
    boostSecondsAB[SWITCH_A] = last_seconds + S_2_BOOST;
  }
  if (path.indexOf(DEV_T_A_ID + "=b3") > 0) {
    boostSecondsAB[SWITCH_A] = last_seconds + S_3_BOOST;
  }
  if (path.indexOf(DEV_T_A_ID + "=on") > 0) {
    boostSecondsAB[SWITCH_A] = last_seconds + S_24_BOOST;
  }
  if (path.indexOf(DEV_T_A_ID + "=off") > 0) {
    boostSecondsAB[SWITCH_A] = 0;
  }
  if (path.indexOf(DEV_T_A_ID + "=soff") > 0) {
    scheduleOffForAB[SWITCH_A] = true;
  }
  if (path.indexOf(DEV_T_A_ID + "=son") > 0) {
    scheduleOffForAB[SWITCH_A] = false;
  }

  if (path.indexOf(DEV_T_B_ID + "=b1") > 0) {
    boostSecondsAB[SWITCH_B] = last_seconds + S_1_BOOST;
  }
  if (path.indexOf(DEV_T_B_ID + "=b2") > 0) {
    boostSecondsAB[SWITCH_B] = last_seconds + S_2_BOOST;
  }
  if (path.indexOf(DEV_T_B_ID + "=b3") > 0) {
    boostSecondsAB[SWITCH_B] = last_seconds + S_3_BOOST;
  }
  if (path.indexOf(DEV_T_B_ID + "=on") > 0) {
    boostSecondsAB[SWITCH_B] = last_seconds + S_24_BOOST;
  }
  if (path.indexOf(DEV_T_B_ID + "=off") > 0) {
    boostSecondsAB[SWITCH_B] = 0;
  }
  if (path.indexOf(DEV_T_B_ID + "=soff") > 0) {
    scheduleOffForAB[SWITCH_B] = true;
  }
  if (path.indexOf(DEV_T_B_ID + "=son") > 0) {
    scheduleOffForAB[SWITCH_B] = false;
  }


}

void updateSwitchState(int switchId) {
  if (boostSecondsAB[switchId] < last_seconds) {
    boostSecondsAB[switchId] = 0;
  }

  if (boostSecondsAB[switchId] > 0) {
    timeUntilNextChange[switchId] = boostSecondsAB[switchId];
    timeRemainNextChange[switchId] = timeUntilNextChange[switchId] - last_seconds;
    statusForAB[switchId] = true;
  } else {
    if (scheduleOffForAB[switchId]) {
      timeUntilNextChange[switchId] = 0; // There is no 'until'
      timeRemainNextChange[switchId] = 0;
      statusForAB[switchId] = false;
    } else {
      nextScheduledTime(switchId);
    }
  }

}

void _sendSwitchStatus(EthernetClient client) {
  String content = "{\"id\":\"" + DEV_ID_STR + "\",\"sync\":" + String(last_seconds) + ","
                   "\"" + DEV_T_A_ID + "\":" + timeUntilNextChange[SWITCH_A] + ","
                   "\"" + DEV_S_A_ID + "\":" + statusForAB[SWITCH_A] + ","
                   "\"" + DEV_T_B_ID + "\":" + timeUntilNextChange[SWITCH_B] + ","
                   "\"" + DEV_S_B_ID + "\":" + statusForAB[SWITCH_B] +
                   "}";
  if (debug) {
    Serial.print("[");
    Serial.println(content);
  }
  client.println(HTTP_200);
  client.print(HTTP_CONTENT_LEN);
  client.println(content.length());
  client.println(HTTP_CONTENT_TYPE_JSON);
  client.println("");
  client.println(content);
}


void _sendDataValues(EthernetClient client) {
  voltage0  = analogRead(VOLTAGE_0_PIN) * IN_TO_VOLTS;
  voltage1  = analogRead(VOLTAGE_1_PIN) * IN_TO_VOLTS;
  voltage2  = analogRead(VOLTAGE_2_PIN) * IN_TO_VOLTS;
  voltage3  = analogRead(VOLTAGE_3_PIN) * IN_TO_VOLTS;
  String content = "{\"id\":\"" + DEV_ID_STR + "\",\"sync\":" + String(last_seconds) + ","
                   "\"t0\":" + M_VIN_TO_CELC(voltage0) + ","
                   "\"t1\":" + M_VIN_TO_CELC(voltage1) + ","
                   "\"v0\":" + (voltage2 / 1000) + ","
                   "\"v1\":" + (voltage3 / 1000) +
                   "}";
  if (debug) {
    Serial.print("[");
    Serial.println(content);
  }
  client.println(HTTP_200);
  client.print(HTTP_CONTENT_LEN);
  client.println(content.length());
  client.println(HTTP_CONTENT_TYPE_JSON);
  client.println("");
  client.println(content);
}



void _sendError(EthernetClient client, String msg, String rc) {
  String cont = "{\"id\":\"" + DEV_ID_STR + "\",\"err\":\"" + rc + "\",\"msg\":\"" + msg + "\"}";
  client.println(HTTP_200);
  client.print(HTTP_CONTENT_LEN);
  client.println(cont.length());
  client.println(HTTP_CONTENT_TYPE_JSON);
  client.println("");
  client.println(cont);
  if (debug) {
    Serial.println(cont);
  }
}
/*
   -------------------------------------------------------------
   NTP (Network Time protocol) section.
   This is where we look up the time and return the number of
   seconds since last Monday 0:0:0

   This happens once on reset. and sets last_seconds. From then
   on last_seconds is incremented by the main loop.

   You would only need to call NTP again if the seconds drift.
   -------------------------------------------------------------
*/
unsigned long getTime()
{

  while (ethernet_UDP.parsePacket() > 0) ; // discard packets remaining to be parsed
  Serial.println("NTP: Transmit Request");
  // send packet to request time from NTP server
  sendRequest(timeSrvr);

  uint32_t beginWait = millis();
  while (millis() - beginWait < 10000) {

    if (ethernet_UDP.parsePacket() >= 48) {
      Serial.println("NTP: Received Response");
      // read data and save to messageBuffer
      ethernet_UDP.read(ntpMessageBuffer, 48);

      // NTP time received will be the seconds elapsed since 1 January 1900
      unsigned long secsSince1900;

      // convert to an unsigned long integer the reference timestamp found at byte 40 to 43
      secsSince1900 =  (unsigned long)ntpMessageBuffer[40] << 24;
      secsSince1900 |= (unsigned long)ntpMessageBuffer[41] << 16;
      secsSince1900 |= (unsigned long)ntpMessageBuffer[42] << 8;
      secsSince1900 |= (unsigned long)ntpMessageBuffer[43];
      /*
         Find number of whole days so we can find the Day of the week (wholeDays % 7)
         Monday = 0
         Thesday = 1
         ...
         And we can work in units of one day.
      */
      unsigned long wholeDays = secsSince1900 / S_24_HOUR;
      /*
         Find the number of seconds from this morning (today 0:0:0) until NOW.
         Seconds since Jan 1 1990 (Which is a Monday 0:0:0) - Number of days * seconds per day
      */
      unsigned long secsRemainToday = secsSince1900 - (wholeDays * S_24_HOUR);
      /*
         Find number of seconds since Monday 0:0:0
         DayOfWeek (wholeDays % 7) * seconds per day + seconds remaining from today
         Also add 1 hour for Daylight Saving!
      */
      unsigned long secsSinceMonday =  ((wholeDays % 7) * S_24_HOUR) + secsRemainToday + daylightSaving;

      Serial.print("NTP: Secs Since Monday:");
      Serial.println(secsSinceMonday);

      return secsSinceMonday;
    }
  }
  // error if no response
  Serial.println("Error: No Response.");
  return 0;
}
/*
   helper function for getTime()
   this function sends a request packet 48 bytes long
*/
void sendRequest(IPAddress &address)
{
  // set all bytes in messageBuffer to 0
  memset(ntpMessageBuffer, 0, 48);

  // create the NTP request message

  ntpMessageBuffer[0] = 0b11100011;  // LI, Version, Mode
  ntpMessageBuffer[1] = 0;           // Stratum, or type of clock
  ntpMessageBuffer[2] = 6;           // Polling Interval
  ntpMessageBuffer[3] = 0xEC;        // Peer Clock Precision
  // array index 4 to 11 is left unchanged - 8 bytes of zero for Root Delay & Root Dispersion
  ntpMessageBuffer[12]  = 49;
  ntpMessageBuffer[13]  = 0x4E;
  ntpMessageBuffer[14]  = 49;
  ntpMessageBuffer[15]  = 52;

  // send messageBuffer to NTP server via UDP at port 123
  ethernet_UDP.beginPacket(address, 123);
  ethernet_UDP.write(ntpMessageBuffer, 48);
  ethernet_UDP.endPacket();
}
