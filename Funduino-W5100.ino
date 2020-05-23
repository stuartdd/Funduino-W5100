
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
#define PULSE_ON 950
#define PULSE_OFF 50
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
#define DEBUG1BIT B00000001

/*
   The address in EEP|ROM to store switchBits
   and remember the state of each switch even after a power off.
*/
#define EEPROM_BASE 0

#define WDT_DELAY 4000
#define BTN_TIMER_DELAY 50
#define SERVO_PWR_DELAY 1000
/**
   Read the HTML/JavaScript String value.
   It is stored in FLASH memory along with the code.

   On Linux it needs to be /
   On Windows it needs to be \
   ------------------------v
*/
#include "PrepHtmlForSketch/embedded.html.cpp"

String DEV_ID_STR = DEV_ID;
/*
   Bits defines the bit positions for each switch using an index.
*/
byte bits[] = {SWITCH_A_BIT, SWITCH_B_BIT};

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

/*
   The state of each switch is held in the bits of this byte.
   See SWITCH_A_BIT to SWITCH_B_BIT above to find the bits.

   The switchBitsEEPROM is used to mirror what is is in EEPROM
   If these are different then updateEPROM will store the value
   of switchBits and update the value of switchBitsEEPROM.
*/
byte switchBits = 0;
byte switchBitsEEPROM = 0;

byte sysData = false;
byte sysDataEEPROM = false;
bool debug = false;

unsigned long nextPulseFlip = 0;
boolean pulseOn = true;
boolean inProgress = false;

float voltage0 = 0;
float voltage1 = 0;
float voltage2 = 0;
float voltage3 = 0;

/*
   Analog constants
*/
const float inToVolts = 4.814;  // (Vref  / 1024) * 1000
/*
   Constants for Temppreture chip MCP9701
*/
const float vout0 = 400;        //  sensor output voltage in mV at 0°C (400 * 1000)
const float tc = 19.53;         // mV for °C temperature constant for the  MCP9701/A


typedef enum READING {
  Heading,
  ContentLen,
  Path,
  Body,
  Skip
};

long loop_time = 0;
long timer_wdt = 0;
long button_timer = 0;
long servo_pwr_timer = 0;

bool servo_pwr_on = false;
bool button_a_pressed = false;
bool button_b_pressed = false;


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
  setSwitchState();
  pinMode(EN_SV_PIN, OUTPUT);
  digitalWrite(EN_SV_PIN, HIGH);
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
/*
   Read the switch state from the EPROM when we start.

   Note Un-Set EPROM locations retirn 255.
*/
void loadEEPROM() {
  byte value = EEPROM.read(EEPROM_BASE);
  if (value > 32) {
    switchBits = 0;
  } else {
    switchBits = value;
  }
  switchBitsEEPROM = switchBits;
  /*
     Set the switches to match the loaded state.
  */

  sysData = EEPROM.read(EEPROM_BASE + 1);
  sysDataEEPROM = sysData;
  debug = getBit(sysData, DEBUG1BIT);
}

void setup() {
  wdt_enable(WDTO_8S);
  loadEEPROM();
  /*
    Read the switch state from the EPROM
  */
  digitalPins();
  servoInit();

  // Open serial communications and wait for port to open:
  Serial.begin(BAUD);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }


  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
  if (debug) {
    Serial.print("server is at ");
    Serial.println(Ethernet.localIP());
  }
  digitalWrite(LEDACTPIN, LOW);
  nextPulseFlip = millis() + PULSE_OFF;
  pulseOn = false;
}


void loop() {
  loop_time = millis();
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


  if (loop_time > button_timer) {
    button_timer = loop_time + BTN_TIMER_DELAY;
    if (!digitalRead(BTN_A_PIN)) {
      if (!button_a_pressed) {
        button_a_pressed = true;
        if (debug) {
          Serial.println("BUTTON A");
        }
        setSwitch(SWITCH_A, !isSwitchOn(SWITCH_A));
      }
    } else {
      button_a_pressed = false;
    }
    if (!digitalRead(BTN_B_PIN)) {
      if (!button_b_pressed) {
        button_b_pressed = true;
        if (debug) {
          Serial.println("BUTTON B");
        }
        setSwitch(SWITCH_B, !isSwitchOn(SWITCH_B));
      }
    } else {
      button_b_pressed = false;
    }
  }

  /*
     If servo power is ON and it is time to turn it OFF
  */
  if ((servo_pwr_on) && (loop_time > servo_pwr_timer)) {
    servo_pwr_on = false;
    digitalWrite(EN_SV_PIN, LOW);
    if (debug) {
      Serial.println(": Servo PWR OFF :");
    }
  }

  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;

    READING state = Heading;

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
  updateEPROM();
}

void handleSwitch(String path) {
  /*
    Set ALL the switches OFF
  */
  if (path.indexOf("all=off") > 0) {
    setSwitch(SWITCH_A, false);
    setSwitch(SWITCH_B, false);
  }
  /*
     Set ALL the switches ON
  */
  if (path.indexOf("all=on") > 0) {
    setSwitch(SWITCH_A, true);
    setSwitch(SWITCH_B, true);
  }
  /*
     Now deal with individual switchs.

     We could receive: /switch?all=on&sa=off
     which results in all switchs on except switch 1.
  */
  if (path.indexOf("sa=on") > 0) {
    setSwitch(SWITCH_A, true);
  } else {
    if (path.indexOf("sa=off") > 0) {
      setSwitch(SWITCH_A, false);
    } else {
      if (path.indexOf("sa=inv") > 0) {
        setSwitch(SWITCH_A, !isSwitchOn(SWITCH_A));
      }
    }
  }

  if (path.indexOf("sb=on") > 0) {
    setSwitch(SWITCH_B, true);
  } else {
    if (path.indexOf("sb=off") > 0) {
      setSwitch(SWITCH_B, false);
    } else {
      if (path.indexOf("sb=inv") > 0) {
        setSwitch(SWITCH_B, !isSwitchOn(SWITCH_B));
      }
    }
  }
}
/*
   Return 'ON' if switch is ON else return 'OFF'
*/
String getSwitchStateText(int switchId) {
  return (isSwitchOn(switchId) ? "ON" : "OFF");
}
/*
   Return true if the switch is ON and false if OFF.
*/
bool isSwitchOn(int switchId) {
  return getBit(switchBits, bits[switchId]);
}
/*
   Set the bit is switchBits and make the Switchs the same.
*/
void setSwitch(int switchId, boolean on) {
  switchBits = setBit(switchBits, on, bits[switchId]);
  setSwitchState();
}

void setSwitchState() {
  switch (switchBits) {
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
  digitalWrite(LED_A_PIN, isSwitchOn(SWITCH_A));
  digitalWrite(LED_B_PIN, isSwitchOn(SWITCH_B));
}


void updateEPROM() {
  if (switchBitsEEPROM != switchBits) {
    EEPROM.write(EEPROM_BASE, switchBits);
    switchBitsEEPROM = switchBits;
  }

  sysData = setBit(sysData, debug, DEBUG1BIT);
  if (sysDataEEPROM != sysData) {
    EEPROM.write(EEPROM_BASE + 1, sysData);
    sysDataEEPROM = sysData;
  }
}

void sendSwitchStatus(EthernetClient client) {
  sendHeader(client, "200 OK");
  voltage0  = analogRead(VOLTAGE_0_PIN) * inToVolts;
  voltage1  = analogRead(VOLTAGE_1_PIN) * inToVolts;
  voltage2  = analogRead(VOLTAGE_2_PIN) * inToVolts;
  voltage3  = analogRead(VOLTAGE_3_PIN) * inToVolts;
  String content = "{\"id\":\""+DEV_ID_STR+"\",\"up\":\"" + String(millis()) + "\",\"debug\":" + (debug ? "true" : "false") + ","
                   "\"on\":{"
                   "\"sa\":\"" + getSwitchStateText(SWITCH_A) + "\","
                   "\"sb\":\"" + getSwitchStateText(SWITCH_B) + "\","
                   "\"d0\":" + "\"off\"" + ","
                   "\"d1\":" + "\"off\"" + ","
                   "\"t0\":" + vinToCelcius(voltage0) + ","
                   "\"t1\":" + vinToCelcius(voltage1) + ","
                   "\"v0\":" + vinToVolts(voltage2) + ","
                   "\"v1\":" + vinToVolts(voltage3) + "}"
                   "}";
  client.print("Content-Length: ");
  client.println(content.length());
  client.println("Content-Type: application/json");
  client.println("");
  client.println(content);
  if (debug) {
    Serial.println(content);
  }
}

void sendHtmlPage(EthernetClient client) {
  sendHeader(client, "200 OK");
  int len = strlen_P(htmlPage);
  client.print("Content-Length: ");
  client.println(len);
  client.println("Content-Type: text/html");
  client.println("");
  char c1;
  char c2;
  for (int k = 0; k < len; k++) {
    c1 = pgm_read_byte_near(htmlPage + k);
    client.write(c1);
  }
  if (debug) {
    Serial.println("HTML index.htm. Sent");
  }
}

void sendError(EthernetClient client, String msg, String rc) {
  sendHeader(client, "200 OK");
  String cont = "{\"id\":\""+DEV_ID_STR+"\",\"err\":\"" + rc + "\",\"msg\":\"" + msg + "\"}";
  client.print("Content-Length: ");
  client.println(cont.length());
  client.println("Content-Type: application/json");
  client.println("");
  client.println(cont);
  if (debug) {
    Serial.println(cont);
  }
}

void sendHeader(EthernetClient client, String statusCode) {
  client.print("HTTP/1.1 ");
  client.println(statusCode);
  if (debug) {
    Serial.println("HTTP/1.1 " + statusCode);
  }
}

byte setBit(byte byteIn, bool on, byte byteBit) {
  if (on) {
    /*
       Set the bit to 1
    */
    return byteIn | byteBit;
  } else {
    /*
       Clear the bit (0)
    */
    return byteIn & ~byteBit;
  }
}

bool getBit(byte byteIn, byte byteBit) {
  return ((byteIn & byteBit) != 0);
}

float vinToCelcius(float vIn) {
  float celc = (vIn - vout0) / tc ;
  if (debug) {
    Serial.print("Temp: vIn: ");
    Serial.print(vIn);
    Serial.print(" celc: ");
    Serial.println(celc);
  }
  return celc;
}

float vinToVolts(float vIn) {
  return vIn / 1000;
}
