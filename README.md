# Funduino-W5100
## Install Arduino IDE

### Download IDE from:
https://www.arduino.cc/en/Main/Software

### Unpack in to (For example) 
```bash
/home/<user>/arduino-1.8.13
```

### Run the install script
```bash
cd /home/<user>/arduino-1.8.13
sudo ./install.sh
```
### Reboot!
This may not be required but is probably a good idea!

## Clone the repository
```bash
mkdir git
cd git
git clone https://github.com/<user>/Funduino-W5100.git
```
## Launch the IDE
### Open the ino file
```bash
Funduino-W5100/Funduino-W5100.ino
```

### If the bootloader for the board has been updated to an UNO.
Ensure the board selected in the __UNO__

Ensure the port is selected

The processor for the Arduino nano is the same as the UNO and the bootloader for the UNO is best.
If it has NOT been updated the choose the nono and select the booloader. If it only works with the old boot loader then you need to update the boot loader (or use an UNO)

## Update the contents of the web Page!

The web page is embedded in to the code space as a constant!

Refer to the line
```c
#include "PrepHtmlForSketch/embedded.html.cpp"
```

This file is generated from the file:
```bash
Funduino-W5100/PrepHtmlForSketch/embedded.html
```

The generation of this file is done by a 'small java program' that removes redundant white space from the file in order to reduce it's size. It then wraps the resultant html is a const, replacing the %% in the line below:
```c
const char htmlPage[] PROGMEM = %%;
```

To run this 'small java program' (assuming Java 9 or higher is installed) run the following command:

```dos
javac src\prephtmlforsketch\PrepHtmlForSketch.java -d build\classes
java -cp build\classes prephtmlforsketch.PrepHtmlForSketch embedded.html %1
```
```bash
cd Funduino-W5100/PrepHtmlForSketch
javac src/prephtmlforsketch/PrepHtmlForSketch.java -d build/classes
java -cp build/classes prephtmlforsketch.PrepHtmlForSketch config.properties ^C
```
Configure: The Java program via __config.properties__

```properties
htmlPage.filename=embedded.html
htmlPage.nl=false
htmlPage.wrap=const char htmlPage[] PROGMEM = %%;
```
* htmlPage.filename
** Defines the input file name (embedded.html) and the output file name (embedded.html.cpp)
* htmlPage.nl
** Will add new lines '\n' if true.This is usefull for debugging in the browser.
* htmlPage.wrap
** The line the resultant html is wraped in to (replacing %%). 

## Prototype layout:
![Prototype pic](https://github.com/stuartdd/Funduino-W5100/blob/master/pic001.png)
## Prototype Wiring diagram
![Prototype wiring](https://github.com/stuartdd/Funduino-W5100/blob/master/Wiring.png)
