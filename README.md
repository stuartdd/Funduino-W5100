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
git clone https://github.com/stuartdd/Funduino-W5100.git
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

## Prototype layout:
![Prototype pic](https://github.com/stuartdd/Funduino-W5100/blob/master/pic001.png)
## Prototype Wiring diagram
![Prototype wiring](https://github.com/stuartdd/Funduino-W5100/blob/master/Wiring.png)
