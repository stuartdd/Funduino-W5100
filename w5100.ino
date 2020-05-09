
#include <SPI.h>
#include <Ethernet.h>
#include <avr/pgmspace.h>
#include <EEPROM.h>

#define MAC 0x34, 0xA9, 0x5D, 0x09, 0xCF, 0xED
#define IP 192, 168, 1, 177
#define PORT 80
#define BAUD 115200
/*
   Define a physical pin for each relay.
*/
#define RELAY1PIN 2
#define RELAY2PIN 3
#define RELAY3PIN 4
#define RELAY4PIN 5
/*
   Define a number for each relay
   These are used as indexes in the the bits[] and pins[] arrays.
   They must start at 0 and end one less that the array size.
*/
#define RELAY1 0
#define RELAY2 1
#define RELAY3 2
#define RELAY4 3
/*
   Define which bit represents a relay in the relayBits value below.
*/
#define RELAY1BIT B00000001
#define RELAY2BIT B00000010
#define RELAY3BIT B00000100
#define RELAY4BIT B00001000
#define DEBUG1BIT B00000001
/*
   As the voltage out for each relay is 5v (HIGH) for OFF and 0v (LOW)
   for ON we define these values for readability only. If your relays
   are 5v (HIGH) for ON and 0v (LOW) for OFF swap these values.
*/
#define RL_OFF HIGH
#define RL_ON LOW
/*
   The address in EEP|ROM to store relayBits
   and remember the state of each relay even after a power off.
*/
#define EEPROM_BASE 0
/**
   Read the HTML/JavaScript String value.
   It is stored in FLASH memory along with the code.

   On Linux it needs to be /
   On Windows it needs to be \
   ------------------------v
*/
#include "PrepHtmlForSketch/embedded.html.cpp"
/*
   Bits defines the bit positions for each relay using an index.
*/
byte bits[] = {RELAY1BIT, RELAY2BIT, RELAY3BIT, RELAY4BIT};
/*
   Bits defines the pins for each relay using an index.
*/
byte pins[] = {RELAY1PIN, RELAY2PIN, RELAY3PIN, RELAY4PIN};


// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {MAC };

IPAddress ip(IP);

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(PORT);

/*
   The state of each relay is held in the bits of this byte.
   See RELAY1BIT to RELAY4BIT above to find the bits.

   The relayBitsEEPROM is used to mirror what is is in EEPROM
   If these are different then updateEPROM will store the value
   of relayBits and update the value of relayBitsEEPROM.
*/
byte relayBits = 0;
byte relayBitsEEPROM = 0;

byte dubugData = false;
byte dubugDataEEPROM = false;
bool debug = false;

typedef enum READING {
  Heading,
  ContentLen,
  Path,
  Body,
  Skip
};



void setup() {
  /*
    Define all the pins
  */
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RELAY1PIN, OUTPUT);
  pinMode(RELAY2PIN, OUTPUT);
  pinMode(RELAY3PIN, OUTPUT);
  pinMode(RELAY4PIN, OUTPUT);
  /*
    Read the relay state from the EPROM
  */
  loadEEPROM();
  /*
     Set the relays to match the loaded state.
  */
  digitalWrite(RELAY1PIN, getRelayPinState(RELAY1));
  digitalWrite(RELAY2PIN, getRelayPinState(RELAY2));
  digitalWrite(RELAY3PIN, getRelayPinState(RELAY3));
  digitalWrite(RELAY4PIN, getRelayPinState(RELAY4));

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
}


void loop() {
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
    if (debug) {
      Serial.println("contentLength: " + contentLength);
      Serial.println("Method: " + method);
      Serial.println(path);
      Serial.println(body);
    }
    sendResponse(client, method, path, body);

    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    if (debug) {
      Serial.println("------------------------------------------------------------");
    }
  }
}


void sendResponse(EthernetClient client, String method, String path, String body) {
  if (method == "GET") {
    path.toLowerCase();
    if (path.indexOf("/relay") >= 0) {
      switchRelays(path);
      sendRelayStatus(client);
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

void switchRelays(String path) {
  /*
    Set ALL the relays OFF
  */
  if (path.indexOf("all=off") > 0) {
    setRelay(RELAY1, false);
    setRelay(RELAY2, false);
    setRelay(RELAY3, false);
    setRelay(RELAY4, false);
  }
  /*
     Set ALL the relays ON
  */
  if (path.indexOf("all=on") > 0) {
    setRelay(RELAY1, true);
    setRelay(RELAY2, true);
    setRelay(RELAY3, true);
    setRelay(RELAY4, true);
  }
  /*
     Now deal with individual relays.

     We could receive: /relay?all=on&r1=off
     which results in all relays on except relay 1.
  */
  if (path.indexOf("r1=on") > 0) {
    setRelay(RELAY1, true);
  } else {
    if (path.indexOf("r1=off") > 0) {
      setRelay(RELAY1, false);
    } else {
      if (path.indexOf("r1=inv") > 0) {
        setRelay(RELAY1, !isRelayOn(RELAY1));
      }
    }
  }

  if (path.indexOf("r2=on") > 0) {
    setRelay(RELAY2, true);
  } else {
    if (path.indexOf("r2=off") > 0) {
      setRelay(RELAY2, false);
    } else {
      if (path.indexOf("r2=inv") > 0) {
        setRelay(RELAY2, !isRelayOn(RELAY2));
      }
    }
  }

  if (path.indexOf("r3=on") > 0) {
    setRelay(RELAY3, true);
  } else {
    if (path.indexOf("r3=off") > 0) {
      setRelay(RELAY3, false);
    } else {
      if (path.indexOf("r3=inv") > 0) {
        setRelay(RELAY3, !isRelayOn(RELAY3));
      }
    }
  }

  if (path.indexOf("r4=on") > 0) {
    setRelay(RELAY4, true);
  } else {
    if (path.indexOf("r4=off") > 0) {
      setRelay(RELAY4, false);
    } else {
      if (path.indexOf("r4=inv") > 0) {
        setRelay(RELAY4, !isRelayOn(RELAY4));
      }
    }
  }

}

/*
   Return 'ON' if relay is ON else return 'OFF'
*/
String getRelayTextState(int relayId) {
  return (isRelayOn(relayId) ? "ON" : "OFF");
}

/*
   Return the required value to turn a relay ON or OFF.
   Note: RL_ON == LOW and RL_OFF = HIGH
*/
int getRelayPinState(int relayId) {
  return (isRelayOn(relayId) ? RL_ON : RL_OFF);
}

/*
   Return true is the relay is ON and false if OFF.
*/
bool isRelayOn(int relayId) {
  return getBit(relayBits, bits[relayId]);
}

/*
   Set the actual output pin and set or clear the bit for the relay.
*/
void setRelay(int relayId, boolean on) {
  relayBits = setBit(relayBits, on, bits[relayId]);
  if (getBit(relayBits, bits[relayId])) {
    /*
       Set the bit then turn it ON
    */
    digitalWrite(pins[relayId], RL_ON);
  } else {
    /*
       Clear the bit then turn it OFF
    */
    digitalWrite(pins[relayId], RL_OFF);
  }
}

/*
   Read the relay state from the EPROM when we start.

   Note Un-Set EPROM locations retirn 255.
*/
void loadEEPROM() {
  byte value = EEPROM.read(EEPROM_BASE);
  if (value > 32) {
    relayBits = 0;
  } else {
    relayBits = value;
  }
  relayBitsEEPROM = relayBits;

  dubugData = EEPROM.read(EEPROM_BASE + 1);
  dubugDataEEPROM = dubugData;
  debug = getBit(dubugData, DEBUG1BIT);
}

void updateEPROM() {
  if (relayBitsEEPROM != relayBits) {
    EEPROM.write(EEPROM_BASE, relayBits);
    relayBitsEEPROM = relayBits;
  }

  dubugData = setBit(dubugData, debug, DEBUG1BIT);
  if (dubugDataEEPROM != dubugData) {
    EEPROM.write(EEPROM_BASE + 1, dubugData);
    dubugDataEEPROM = dubugData;
  }
}

void sendRelayStatus(EthernetClient client) {
  sendHeader(client, "200 OK");
  String content = "{\"id\":\"001\",\"up\":\"" + String(millis()) + "\",\"debug\":" + (debug ? "true" : "false") + ","
                   "\"on\":{"
                   "\"r1\":\"" + getRelayTextState(RELAY1) + "\","
                   "\"r2\":\"" + getRelayTextState(RELAY2) + "\","
                   "\"r3\":\"" + getRelayTextState(RELAY3) + "\","
                   "\"r4\":\"" + getRelayTextState(RELAY4) + "\"}"
                   "}";
  client.print("Content-Length: ");
  client.println(content.length());
  client.println("Content-Type: application/json");
  client.println("");
  client.println(content);
}

void sendHtmlPage(EthernetClient client) {
  sendHeader(client, "200 OK");
  int len = strlen_P(htmlPage);
  client.print("Content-Length: ");
  client.println(len);
  client.println("Content-Type: text/html");
  client.println("");
  char myChar;
  for (int k = 0; k < len; k++) {
    myChar = pgm_read_byte_near(htmlPage + k);
    client.write(myChar);
  }
}

void sendError(EthernetClient client, String msg, String rc) {
  sendHeader(client, rc);
  String cont = "{\"id\":\"001\",\"err\":\"" + rc + "\",\"msg\":\"" + msg + "\"}";
  client.print("Content-Length: ");
  client.println(cont.length());
  client.println("Content-Type: application/json");
  client.println("");
  client.println(cont);
}

void sendHeader(EthernetClient client, String statusCode) {
  digitalWrite(LED_BUILTIN, HIGH);
  client.print("HTTP/1.1 ");
  client.println(statusCode);
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
