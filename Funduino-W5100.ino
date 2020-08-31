
#include <SPI.h>
#include <Ethernet.h>
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
#define S_1_YEAR 31536000
/*
   The address in EEP|ROM to store switchBits
   and remember the state of each switch even after a power off.
*/
#define EEPROM_DEBUG 0
#define EEPROM_SECONDS 4
#define EEPROM_SECONDS_A 8
#define EEPROM_SECONDS_B 12


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
#define HTTP_CONTENT_TYPE_HTML "Content-Type: text/html"

/*
   Define CODE macros.
   Used to inline code and reduce the call stack while declaring function once.
*/
#define M_REMAINING_SEC(id)(M_IS_ON(id) ? secondsToOffAB[id] - last_seconds : 0)
#define M_VIN_TO_CELC(vIn)((vIn - VOUT_MV_Z) / TEMP_CONST)
#define M_IS_ON(id)((last_seconds < secondsToOffAB[id]))
#define M_SET_SEC(id, sec)(secondsToOffAB[id] = (sec > 0 ? last_seconds + sec : 0))
#define M_EEPROM_LONG(addr, val)({ \
    EEPROM.write(addr, (val & 0xFF)); \
    EEPROM.write(addr + 1, ((val >> 8) & 0xFF)); \
    EEPROM.write(addr + 2, ((val >> 16) & 0xFF)); \
    EEPROM.write(addr + 3, ((val >> 24) & 0xFF)); \
  })

#define M_SET_OFF(id) ({ \
    M_SET_SEC(id, 0); \
    M_EEPROM_LONG(offsetToEEPROM[id], secondsToOffAB[id]); \
  })

#define M_SET_ON(id, sec)({ \
    M_SET_SEC(id,sec); \
    M_EEPROM_LONG(offsetToEEPROM[id], secondsToOffAB[id]); \
  })


#define M_SET_INV(id)({ \
    if (M_IS_ON(id)) { \
      M_SET_SEC(id,0); \
    } else { \
      M_SET_SEC(id,S_24_HOUR); \
    } \
    M_EEPROM_LONG(offsetToEEPROM[id], secondsToOffAB[id]); \
  })

enum REQ_DATA {
  Heading,
  ContentLen,
  Path,
  Body,
  Skip
};
/*
   Read the HTML/JavaScript String value.
   It is stored in FLASH memory along with the code.

   On Linux it needs to be /
   On Windows it needs to be \\
*/

#include "PrepHtmlForSketch/embedded.html.cpp"
/*
   Light flash sequence.
*/
#define SEQ_LEN 10
#define SEQ_3H 0
#define SEQ_2H 2
#define SEQ_1H 4
#define SEQ_ON 6
#define SEQ_TIME 200
const int seqOut[] = {HIGH, LOW, HIGH, LOW, HIGH, LOW, HIGH, HIGH, HIGH, HIGH};
int seq_index_sw_a = SEQ_ON;
int seq_index_sw_b = SEQ_ON;
int seq_base_sw_a = SEQ_ON;
int seq_base_sw_b = SEQ_ON;
long seq_led_timer = 0;

const String DEV_ID_STR = DEV_ID;
const String DEV_A_NAME = "Heating";
const String DEV_B_NAME = "Water";

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {MAC};

IPAddress ip(IP);

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(PORT);

Servo servoMotor;
int servoPosition;

byte currentSwitchState = 0;
bool debug = false;

boolean inProgress = false;

float voltage0 = 0;
float voltage1 = 0;
float voltage2 = 0;
float voltage3 = 0;


long offsetToEEPROM[] = {EEPROM_SECONDS_A, EEPROM_SECONDS_B};
long secondsToOffAB[] = {0, 0};
long remainingForA = 0;
long remainingForB = 0;
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

  loadEEPROM();

  /*
    Read the switch state from the EPROM
  */
  digitalPins();

  if (!digitalRead(BTN_A_PIN) || !digitalRead(BTN_B_PIN)) {
    last_seconds = 0;
    debug = digitalRead(BTN_B_PIN);
    EEPROM.write(EEPROM_DEBUG, debug);
    M_SET_OFF(SWITCH_A);
    M_SET_OFF(SWITCH_B);
  }

  while (!digitalRead(BTN_A_PIN) || !digitalRead(BTN_B_PIN)) {
    delay(100);
    digitalWrite(LEDACTPIN, HIGH);
    delay(100);
    digitalWrite(LEDACTPIN, LOW);
  }

  servoInit();

  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
  if (debug) {
    Serial.print("server is at ");
    Serial.println(Ethernet.localIP());
  }
  digitalWrite(LEDACTPIN, LOW);
  last_seconds_set_time = millis();
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
    if (last_seconds_temp > 0) {
      M_EEPROM_LONG(EEPROM_SECONDS, last_seconds);
    }
  }
  /*
     Reset if we dont get here in time!
  */
  if (loop_time > timer_wdt) {
    timer_wdt = loop_time + WDT_DELAY;
    if (!inProgress) {
      wdt_reset();
      if (debug) {
        Serial.print(millis());
        Serial.println(": WDT Reset:");
      }
    } else {
      if (debug) {
        Serial.print(millis());
        Serial.println(": InProgress:");
      }
    }
  }
  /*
     Only look at the buttons every BTN_TIMER_DELAY ms
  */
  if (loop_time > button_timer) {
    button_timer = loop_time + BTN_TIMER_DELAY;
    if (!digitalRead(BTN_A_PIN)) {
      if (!button_a_pressed) {
        seq_base_sw_a = SEQ_ON;
        button_a_pressed = true;
        buttonClearTimerA = loop_time + CLEAR_BUTTON_DELAY;
        buttonStateAB[SWITCH_A]++;
        setTimeToOff(SWITCH_A);
      }
    } else {
      button_a_pressed = false;
      if ((buttonClearTimerA > 0) && (loop_time > buttonClearTimerA)) {
        buttonStateAB[SWITCH_A] = 0;
        buttonClearTimerA = 0;
      }
    }
    if (!digitalRead(BTN_B_PIN)) {
      if (!button_b_pressed) {
        seq_base_sw_b = SEQ_ON;
        button_b_pressed = true;
        buttonClearTimerB = loop_time + CLEAR_BUTTON_DELAY;
        buttonStateAB[SWITCH_B]++;
        setTimeToOff(SWITCH_B);
      }
    } else {
      button_b_pressed = false;
      if ((buttonClearTimerB > 0) && (loop_time > buttonClearTimerB)) {
        buttonStateAB[SWITCH_B] = 0;
        buttonClearTimerB = 0;
      }
    }
  }
  /*
     Do we need to tourn on the heating/hot water
  */
  if (loop_time > updateSwitchTimer) {
    updateSwitchTimer = loop_time + UPDATE_SWITCH_DELAY;
    remainingForA = M_REMAINING_SEC(SWITCH_A);
    remainingForB = M_REMAINING_SEC(SWITCH_B);
    setSwitchState();
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
      EEPROM.write(EEPROM_DEBUG, debug);
    } else if (path.indexOf("/debugoff") >= 0) {
      debug = false;
      EEPROM.write(EEPROM_DEBUG, debug);
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

    if (M_IS_ON(SWITCH_A)) {
      seq_index_sw_a++;
      if (seq_index_sw_a >= SEQ_LEN) {
        seq_index_sw_a = seq_base_sw_a;
      }
      digitalWrite(LED_A_PIN, seqOut[seq_index_sw_a]);
      if (remainingForA < S_1_HOUR) {
        seq_base_sw_a = SEQ_1H;
      } else if (remainingForA < S_2_HOUR) {
        seq_base_sw_a = SEQ_2H;
      } else if (remainingForA < S_3_HOUR) {
        seq_base_sw_a = SEQ_3H;
      } else {
        seq_base_sw_a = SEQ_ON;
      }
    } else {
      digitalWrite(LED_A_PIN, LOW);
      seq_base_sw_a = SEQ_ON;
    }

    if (M_IS_ON(SWITCH_B)) {
      seq_index_sw_b++;
      if (seq_index_sw_b >= SEQ_LEN) {
        seq_index_sw_b = seq_base_sw_b;
      }
      digitalWrite(LED_B_PIN, seqOut[seq_index_sw_b]);
      if (remainingForB < S_1_HOUR) {
        seq_base_sw_b = SEQ_1H;
      } else if (remainingForB < S_2_HOUR) {
        seq_base_sw_b = SEQ_2H;
      } else if (remainingForB < S_3_HOUR) {
        seq_base_sw_b = SEQ_3H;
      } else {
        seq_base_sw_b = SEQ_ON;
      }
    } else {
      digitalWrite(LED_B_PIN, LOW);
      seq_base_sw_b = SEQ_ON;
    }
  }
}


void sendResponse(EthernetClient client, String method, String path, String body) {
  if (method == "GET") {
    path.toLowerCase();
    if (path.indexOf("/switch") >= 0) {
      handleSwitch(path);
      sendSwitchStatus(client);
    } else {
      if (path.indexOf("/index.htm") >= 0) {
        sendHtmlPage(client);
      } else {
        sendError(client, "Path not recognised", "404 Not Found");
      }
    }
  } else {
    sendError(client, "Only GET is Supported", "405 Method Not Allowed");
  }
  client.println("");
  client.flush();
}

void handleSwitch(String path) {
  /*
    Set ALL the switches OFF
  */
  if (path.indexOf("all=off") > 0) {
    M_SET_OFF(SWITCH_A);
    M_SET_OFF(SWITCH_B);
  }
  /*
     Set ALL the switches ON
  */
  if (path.indexOf("all=on") > 0) {
    M_SET_ON(SWITCH_A, S_24_HOUR);
    M_SET_ON(SWITCH_B, S_24_HOUR);
  }
  /*
     Now deal with individual switchs.

     We could receive: /switch?all=on&sa=off
     which results in all switchs on except switch 1.
  */
  if (path.indexOf("sa=on") > 0) {
    M_SET_ON(SWITCH_A, S_24_HOUR);
  } else {
    if (path.indexOf("sa=off") > 0) {
      M_SET_OFF(SWITCH_A);
    } else {
      if (path.indexOf("sa=inv") > 0) {
        M_SET_INV(SWITCH_A);
      }
    }
  }

  if (path.indexOf("sb=on") > 0) {
    M_SET_ON(SWITCH_B, S_24_HOUR);
  } else {
    if (path.indexOf("sb=off") > 0) {
      M_SET_OFF(SWITCH_B);
    } else {
      if (path.indexOf("sb=inv") > 0) {
        M_SET_INV(SWITCH_B);
      }
    }
  }
}

void setSwitchState() {
  int bits = 0;
  if (M_IS_ON(SWITCH_A)) {
    bits = bits | SWITCH_A_BIT;
  }
  if M_IS_ON(SWITCH_B) {
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

void sendSwitchStatus(EthernetClient client) {
  voltage0  = analogRead(VOLTAGE_0_PIN) * IN_TO_VOLTS;
  voltage1  = analogRead(VOLTAGE_1_PIN) * IN_TO_VOLTS;
  voltage2  = analogRead(VOLTAGE_2_PIN) * IN_TO_VOLTS;
  voltage3  = analogRead(VOLTAGE_3_PIN) * IN_TO_VOLTS;
  String content = "{\"id\":\"" + DEV_ID_STR + "\",\"up\":\"" + String(millis()) + "\",\"debug\":" + (debug ? "true" : "false") + ","
            "\"ra\":" + remainingForA + ","
            "\"rb\":" + remainingForB + ","
            "\"t0\":" + M_VIN_TO_CELC(voltage0) + ","
            "\"t1\":" + M_VIN_TO_CELC(voltage1) + ","
            "\"v0\":" + (voltage2 / 1000) + ","
            "\"v1\":" + (voltage3 / 1000) +
            "}";
  client.println(HTTP_200);
  client.print(HTTP_CONTENT_LEN);
  client.println(content.length());
  client.println(HTTP_CONTENT_TYPE_JSON);
  client.println("");
  client.println(content);
  if (debug) {
    Serial.print("[");
    Serial.println(content);
  }
}

void sendHtmlPage(EthernetClient client) {
  int len = strlen_P(htmlPage);
  client.println(HTTP_200);
  client.print(HTTP_CONTENT_LEN);
  client.println(len);
  client.println(HTTP_CONTENT_TYPE_HTML);
  client.println("");
  char c1;
  char c2;
  for (int k = 0; k < len; k++) {
    c1 = pgm_read_byte_near(htmlPage + k);
    if (c1 == '$') {
      k++;
      c1 = pgm_read_byte_near(htmlPage + k);
      switch (c1) {
        case '0':
          client.print(DEV_ID_STR);
          break;
        case '1':
          client.print(DEV_A_NAME);
          break;
        case '2':
          client.print(DEV_B_NAME);
          break;
        default:
          client.write('$');
          client.write(c1);
      }
    } else {
      client.write(c1);
    }
  }
  if (debug) {
    Serial.println("HTML index.htm. Sent");
  }
}

void sendError(EthernetClient client, String msg, String rc) {
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

void setTimeToOff(int switchId) {
  switch (buttonStateAB[switchId]) {
    case 1:
      M_SET_ON(switchId, S_1_HOUR);
      break;
    case 2:
      M_SET_ON(switchId, S_2_HOUR);
      break;
    case 3:
      M_SET_ON(switchId, S_3_HOUR);
      break;
    default:
      buttonStateAB[switchId] = 0;
      M_SET_OFF(switchId);
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
/*
   Read the switch state from the EPROM when we start.

   Note Un-Set EPROM locations retirn 255.
*/
void loadEEPROM() {
  last_seconds = EEPROMReadlong(EEPROM_SECONDS);
  secondsToOffAB[SWITCH_A] = EEPROMReadlong(EEPROM_SECONDS_A);
  secondsToOffAB[SWITCH_B] = EEPROMReadlong(EEPROM_SECONDS_B);
  debug = EEPROM.read(EEPROM_DEBUG);
}

long EEPROMReadlong(long address) {
  long four = EEPROM.read(address);
  long three = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);
  long v = ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
  if ((v < 0) || (v > S_1_YEAR)) {
    return 0;
  }
  return v;
}
