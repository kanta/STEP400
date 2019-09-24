/* 
	Editor: https://www.visualmicro.com/
			visual micro and the arduino ide ignore this code during compilation. this code is automatically maintained by visualmicro, manual changes to this file will be overwritten
			the contents of the _vm sub folder can be deleted prior to publishing a project
			all non-arduino files created by visual micro and all visual studio project or solution files can be freely deleted and are not required to compile a sketch (do not delete your own code!).
			note: debugger breakpoints are stored in '.sln' or '.asln' files, knowledge of last uploaded breakpoints is stored in the upload.vmps.xml file. Both files are required to continue a previous debug session without needing to compile and upload again
	
	Hardware: Arduino/Genuino Zero (Native USB Port), Platform=samd, Package=arduino
*/

#define ARDUINO 10809
#define ARDUINO_MAIN
#define F_CPU 48000000L
#define printf iprintf
#define __SAMD__
#define F_CPU 48000000L
#define ARDUINO 10809
#define ARDUINO_SAMD_ZERO
#define ARDUINO_ARCH_SAMD
#define __SAMD21G18A__
#define USB_VID 0x2341
#define USB_PID 0x804d
#define USBCON
//
void sendOneData(char *address, int32_t data);
void setDestIp(OSCMessage &msg ,int addrOffset);
void setKVAL(OSCMessage &msg ,int addrOffset);
void setSpdProfile(OSCMessage &msg ,int addrOffset);
void run(OSCMessage &msg ,int addrOffset);
void getPos(OSCMessage &msg ,int addrOffset);
void setPos(OSCMessage &msg ,int addrOffset);
void hardStop(OSCMessage &msg ,int addrOffset);
void hardHiZ(OSCMessage &msg ,int addrOffset);
void getStatus(OSCMessage &msg ,int addrOffset);
void OSCMsgReceive();
void swCheck();
//

#include "pins_arduino.h" 
#include "variant.h" 
#include "arduino.h"
#include "step400_OSCtester.ino"
