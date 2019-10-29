/*
* step400_OSCtester.ino
*
* Created: 9/19/2019 4:53:04 PM
* Author: kanta
*/

#include <Arduino.h>
#include <Ethernet.h>
#include <OSCMessage.h>
//#include <powerSTEP01ArduinoLibrary.h>
#include "LocalLibraries/powerSTEP01_Arduino_Library/src/powerSTEP01ArduinoLibrary.h"
#include <SPI.h>
#include "wiring_private.h" // pinPeripheral() function

#define ledPin	13
byte mac[] = { 0x90, 0xA2, 0xDA, 0xD6, 0xA3, 23 };
IPAddress myIp(10,0,0,131);
IPAddress destIp(10, 0, 0, 10);
unsigned int outPort = 20203;
unsigned int inPort = 20000;
EthernetUDP Udp;

#define POWERSTEP_MISO	6	// D6 /SERCOM3/PAD[2] miso
#define POWERSTEP_MOSI	11	// D11/SERCOM3/PAD[0] mosi
#define POWERSTEP_SCK	12	// D12/SERCOM3/PAD[3] sck
//SPIClass altSPI (&sercom1, 12, 13, 11, SPI_PAD_0_SCK_1, SERCOM_RX_PAD_3);
SPIClass powerStepSPI (&sercom3, POWERSTEP_MISO, POWERSTEP_SCK, POWERSTEP_MOSI, SPI_PAD_0_SCK_3, SERCOM_RX_PAD_2);// MISO/SCK/MOSI pins

#define POWERSTEP_CS_PIN A0
#define POWERSTEP_RESET_PIN A2


// powerSTEP library instance, parameters are distance from the end of a daisy-chain
// of drivers, !CS pin, !STBY/!Reset pin
powerSTEP powerSteps[] = {
	powerSTEP(3, POWERSTEP_CS_PIN, POWERSTEP_RESET_PIN),
	powerSTEP(2, POWERSTEP_CS_PIN, POWERSTEP_RESET_PIN),
	powerSTEP(1, POWERSTEP_CS_PIN, POWERSTEP_RESET_PIN),
	powerSTEP(0, POWERSTEP_CS_PIN, POWERSTEP_RESET_PIN)
};

#define SD_CS	4u

const uint8_t dipSwPin[8] = {A5,SCL,7u,SDA,2u,9u,3u,0u};

bool isOriginReturn[4];
bool isSendBusy[4] = {false, false, false, false};
bool isSendSw[4] = {false, false, false, false};
uint8_t lastDir[4];
uint8_t lastSw[4];
uint8_t lastBusy[4];

uint64_t lastTime;

void setup()
{
	pinMode(ledPin, OUTPUT);
	pinMode(SD_CS, OUTPUT);

	// Start serial
	SerialUSB.begin(9600);
	SerialUSB.println("powerSTEP01 Arduino control initialising...");

	// Prepare pins
	pinMode(POWERSTEP_RESET_PIN, OUTPUT);
	pinMode(POWERSTEP_CS_PIN, OUTPUT);
	pinMode(POWERSTEP_MOSI, OUTPUT);
	pinMode(POWERSTEP_MISO, INPUT);
	pinMode(POWERSTEP_SCK, OUTPUT);

	// Reset powerSTEP and set CS
	digitalWrite(POWERSTEP_RESET_PIN, HIGH);
	digitalWrite(POWERSTEP_RESET_PIN, LOW);
	digitalWrite(POWERSTEP_RESET_PIN, HIGH);
	digitalWrite(POWERSTEP_CS_PIN, HIGH);

	// Start SPI for PowerSTEP
	powerStepSPI.begin();
	powerStepSPI.setClockDivider(SPI_CLOCK_DIV128); // default 4
	pinPeripheral(POWERSTEP_MOSI, PIO_SERCOM_ALT);
	pinPeripheral(POWERSTEP_SCK, PIO_SERCOM_ALT);
	pinPeripheral(POWERSTEP_MISO , PIO_SERCOM_ALT);
	powerStepSPI.setDataMode(SPI_MODE3);
	delay(100);
	// Configure powerSTEP
	for (uint8_t i=0; i<4; i++)
	{
		powerSteps[i].SPIPortConnect(&powerStepSPI);
		powerSteps[i].configStepMode(STEP_FS_128);

		powerSteps[i].setMaxSpeed(10000);
		powerSteps[i].setFullSpeed(2000);
		powerSteps[i].setAcc(2000);
		powerSteps[i].setDec(2000);
		powerSteps[i].setSlewRate(SR_520V_us);
		powerSteps[i].setOCThreshold(8);
		powerSteps[i].setOCShutdown(OC_SD_DISABLE);
		powerSteps[i].setPWMFreq(PWM_DIV_1, PWM_MUL_0_75);
		powerSteps[i].setVoltageComp(VS_COMP_DISABLE);
		powerSteps[i].setSwitchMode(SW_USER);
		//powerSteps[i].setSwitchMode(SW_HARD_STOP);
		powerSteps[i].setOscMode(EXT_24MHZ_OSCOUT_INVERT);
		//powerSteps[i].setOscMode(INT_16MHZ);
		powerSteps[i].setRunKVAL(64);
		powerSteps[i].setAccKVAL(64);
		powerSteps[i].setDecKVAL(64);
		powerSteps[i].setHoldKVAL(32);
		powerSteps[i].setParam(ALARM_EN, 0x8F); // disable ADC UVLO (divider not populated),
		// disable stall detection (not configured),
		// disable switch (not using as hard stop)
		delay(1);
		powerSteps[i].getStatus(); // clears error flags
		digitalWrite(ledPin, HIGH);
		delay(100);
		digitalWrite(ledPin, LOW);
		delay(100);
	}


	SerialUSB.println(F("Initialisation complete"));

	Ethernet.begin(mac, myIp);
	Udp.begin(inPort);
}

void sendOneData(char *address, int32_t data) {
	OSCMessage newMes(address);
	newMes.add((int32_t)data);
	Udp.beginPacket(destIp, outPort);
	newMes.send(Udp);
	Udp.endPacket();
	newMes.empty();
}

void sendOneData(char *address, uint8_t target, int32_t data) {
	OSCMessage newMes(address);
	newMes.add((int32_t)target);
	newMes.add((int32_t)data);
	Udp.beginPacket(destIp, outPort);
	newMes.send(Udp);
	Udp.endPacket();
	newMes.empty();
}

void setDestIp(OSCMessage &msg ,int addrOffset) {
	destIp = Udp.remoteIP();
	sendOneData("/newDestIp", (int32_t)destIp[3]);
	digitalWrite(ledPin, !digitalRead(ledPin));
}

void setSwFlag(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	uint8_t flag = constrain(msg.getInt(1),0,3);
	if (flag == 0) {
		isSendSw[target] = false;
		} else {
		isSendSw[target] = true;
	}
}

void setBusyFlag(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	uint8_t flag = constrain(msg.getInt(1),0,3);
	if (flag == 0) {
		isSendBusy[target] = false;
		} else {
		isSendBusy[target] = true;
	}
}


void getStatus(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	sendOneData("/status", target, powerSteps[target].getStatus());
}

void configStepMode(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	uint8_t stepMode = constrain(msg.getInt(1), STEP_FS, STEP_FS_128);
	powerSteps[target].configStepMode(stepMode);
}

void getStepMode(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	sendOneData("/stepMode", target, powerSteps[target].getStepMode());
}

void voltageMode(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	uint8_t stepMode = constrain(msg.getInt(1), STEP_FS, STEP_FS_128);
	powerSteps[target].voltageMode(stepMode);
}

void currentMode(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	uint8_t stepMode = constrain(msg.getInt(1), STEP_FS, STEP_FS_128);
	powerSteps[target].currentMode(stepMode);

	int t = 0;
	powerSteps[target].setHoldTVAL(t);
	powerSteps[target].setRunTVAL(t);
	powerSteps[target].setAccTVAL(t);
	powerSteps[target].setDecTVAL(t);
}


#pragma region kval_commands_osc_listener
void setKVAL(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	int t = msg.getInt(1);
	t = constrain(t,0,255);
	powerSteps[target].setHoldKVAL(t);
	t = msg.getInt(2);
	t = constrain(t,0,255);
	powerSteps[target].setRunKVAL(t);
	t = msg.getInt(3);
	t = constrain(t,0,255);
	powerSteps[target].setAccKVAL(t);
	t = msg.getInt(4);
	t = constrain(t,0,255);
	powerSteps[target].setDecKVAL(t);
}
void setAccKVAL(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	byte kvalInput = msg.getInt(1);
	powerSteps[target].setAccKVAL(kvalInput);
}
void setDecKVAL(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	byte kvalInput = msg.getInt(1);
	powerSteps[target].setDecKVAL(kvalInput);
}
void setRunKVAL(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	byte kvalInput = msg.getInt(1);
	powerSteps[target].setRunKVAL(kvalInput);
}
void setHoldKVAL(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	byte kvalInput = msg.getInt(1);
	powerSteps[target].setHoldKVAL(kvalInput);
}
void getKVAL(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);

	OSCMessage newMes("/kval");
	newMes.add((int32_t)target);
	newMes.add((int32_t)powerSteps[target].getHoldKVAL());
	newMes.add((int32_t)powerSteps[target].getRunKVAL());
	newMes.add((int32_t)powerSteps[target].getAccKVAL());
	newMes.add((int32_t)powerSteps[target].getDecKVAL());
	Udp.beginPacket(destIp, outPort);
	newMes.send(Udp);
	Udp.endPacket();
	newMes.empty();
}
#pragma endregion


#pragma region tval_commands_osc_listener
void setTVAL(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	int t = msg.getInt(1);
	t = constrain(t,0,255);
	powerSteps[target].setHoldTVAL(t);
	t = msg.getInt(2);
	t = constrain(t,0,255);
	powerSteps[target].setRunTVAL(t);
	t = msg.getInt(3);
	t = constrain(t,0,255);
	powerSteps[target].setAccTVAL(t);
	t = msg.getInt(4);
	t = constrain(t,0,255);
	powerSteps[target].setDecTVAL(t);
}
void setAccTVAL(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	byte tvalInput = msg.getInt(1);
	powerSteps[target].setAccTVAL(tvalInput);
}
void setDecTVAL(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	byte tvalInput = msg.getInt(1);
	powerSteps[target].setDecTVAL(tvalInput);
}
void setRunTVAL(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	byte tvalInput = msg.getInt(1);
	powerSteps[target].setRunTVAL(tvalInput);
}
void setHoldTVAL(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	byte tvalInput = msg.getInt(1);
	powerSteps[target].setHoldTVAL(tvalInput);
}
void getTVAL(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);

	OSCMessage newMes("/tval");
	newMes.add((int32_t)target);
	newMes.add((int32_t)powerSteps[target].getHoldTVAL());
	newMes.add((int32_t)powerSteps[target].getRunTVAL());
	newMes.add((int32_t)powerSteps[target].getAccTVAL());
	newMes.add((int32_t)powerSteps[target].getDecTVAL());
	Udp.beginPacket(destIp, outPort);
	newMes.send(Udp);
	Udp.endPacket();
	newMes.empty();
}
#pragma endregion

#pragma region speed_commands_osc_listener

void setSpdProfile(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	float t = msg.getFloat(1);
	powerSteps[target].setMaxSpeed(t);
	t = msg.getFloat(2);
	powerSteps[target].setMinSpeed(t);
	t = msg.getFloat(3);
	powerSteps[target].setAcc(t);
	t = msg.getFloat(4);
	powerSteps[target].setDec(t);
}

void setMaxSpeed(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	float stepsPerSecond = msg.getFloat(1);
	powerSteps[target].setMaxSpeed(stepsPerSecond);
}
void setMinSpeed(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	float stepsPerSecond = msg.getFloat(1);
	powerSteps[target].setMinSpeed(stepsPerSecond);
}
void setFullSpeed(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	float stepsPerSecond = msg.getFloat(1);
	powerSteps[target].setFullSpeed(stepsPerSecond);
}
void setAcc(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	float stepsPerSecondPerSecond = msg.getFloat(1);
	powerSteps[target].setAcc(stepsPerSecondPerSecond);
}
void setDec(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	float stepsPerSecondPerSecond = msg.getFloat(1);
	powerSteps[target].setDec(stepsPerSecondPerSecond);
}

void setSpdProfileRaw(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	int t = msg.getInt(1);
	powerSteps[target].setMaxSpeedRaw(t);
	t = msg.getInt(2);
	powerSteps[target].setMinSpeedRaw(t);
	t = msg.getInt(3);
	powerSteps[target].setAccRaw(t);
	t = msg.getInt(4);
	powerSteps[target].setDecRaw(t);
}

void setMaxSpeedRaw(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	int integerSpeed = msg.getInt(1);
	powerSteps[target].setMaxSpeedRaw(integerSpeed);
}
void setMinSpeedRaw(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	int integerSpeed = msg.getInt(1);
	powerSteps[target].setMinSpeedRaw(integerSpeed);
}
void setFullSpeedRaw(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	int integerSpeed = msg.getInt(1);
	powerSteps[target].setFullSpeedRaw(integerSpeed);
}
void setAccRaw(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	int integerAcc = msg.getInt(1);
	powerSteps[target].setAccRaw(integerAcc);
}
void setDecRaw(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	int integerDec = msg.getInt(1);
	powerSteps[target].setDecRaw(integerDec);
}

void getSpdProfile(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);

	OSCMessage newMes("/spd");
	newMes.add((int32_t)target);
	newMes.add((float)powerSteps[target].getMaxSpeed());
	newMes.add((float)powerSteps[target].getMinSpeed());
	newMes.add((float)powerSteps[target].getAcc());
	newMes.add((float)powerSteps[target].getDec());
	Udp.beginPacket(destIp, outPort);
	newMes.send(Udp);
	Udp.endPacket();
	newMes.empty();
}
void getSpdProfileRaw(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);

	OSCMessage newMes("/spdRaw");
	newMes.add((int32_t)target);
	newMes.add((int32_t)powerSteps[target].getMaxSpeedRaw());
	newMes.add((int32_t)powerSteps[target].getMinSpeedRaw());
	newMes.add((int32_t)powerSteps[target].getAccRaw());
	newMes.add((int32_t)powerSteps[target].getDecRaw());
	Udp.beginPacket(destIp, outPort);
	newMes.send(Udp);
	Udp.endPacket();
	newMes.empty();
}
#pragma endregion

#pragma region operational_commands_osc_listener

void getPos(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	sendOneData("/pos", target, powerSteps[target].getPos());
}
void getMark(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	sendOneData("/mark", target, powerSteps[target].getMark());
}

void run(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	float stepsPerSec = msg.getFloat(1);
	boolean dir = stepsPerSec>0;
	powerSteps[target].run(dir,abs(stepsPerSec));
}
void runRaw(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	int integerSpeed = msg.getInt(1);
	boolean dir = integerSpeed>0;
	powerSteps[target].runRaw(dir, abs(integerSpeed));
}
void move(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0), 0, 3);
	uint8_t dir = constrain(msg.getInt(1), 0, 1);
	unsigned long numSteps = msg.getInt(2);
	powerSteps[target].move(dir, numSteps);
}
void goTo(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0), 0, 3);
	unsigned long pos = msg.getInt(1);
	powerSteps[target].goTo(pos);
}
void goToDir(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0), 0, 3);
	uint8_t dir = constrain(msg.getInt(1), 0, 1);
	unsigned long pos = msg.getInt(2);
	powerSteps[target].goToDir(dir, pos);
}
// todo: action‚Ä‚È‚É
void goUntil(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0), 0, 3);
	uint8_t action = msg.getInt(1);
	uint8_t dir = constrain(msg.getInt(2), 0, 1);
	float stepsPerSec = msg.getFloat(3);
	powerSteps[target].goUntil(action, dir, stepsPerSec);
}
void goUntilRaw(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0), 0, 3);
	uint8_t action = msg.getInt(1);
	uint8_t dir = constrain(msg.getInt(2), 0, 1);
	int integerSpeed = msg.getInt(3);
	powerSteps[target].goUntilRaw(action, dir, integerSpeed);
}
void releaseSw(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0), 0, 3);
	uint8_t action = msg.getInt(1);
	uint8_t dir = constrain(msg.getInt(2), 0, 1);
	powerSteps[target].releaseSw(action, dir);
}
void goHome(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0), 0, 3);
	powerSteps[target].goHome();
}
void goMark(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0), 0, 3);
	powerSteps[target].goMark();
}
void setMark(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0), 0, 3);
	unsigned long newMark = msg.getInt(1);
	powerSteps[target].setMark(newMark);
}
void setPos(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0), 0, 3);
	unsigned long newPos = msg.getInt(1);
	powerSteps[target].setPos(newPos);
}
void resetPos(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0), 0, 3);
	powerSteps[target].resetPos();
}
void resetDev(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0), 0, 3);
	powerSteps[target].resetDev();
}
void softStop(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0), 0, 3);
	powerSteps[target].softStop();
}
void hardStop(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0), 0, 3);
	powerSteps[target].hardStop();
}
void softHiZ(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0), 0, 3);
	powerSteps[target].softHiZ();
}
void hardHiZ(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0), 0, 3);
	powerSteps[target].hardHiZ();
}
#pragma endregion





void OSCMsgReceive() {

	OSCMessage msgIN;
	int size;
	if((size = Udp.parsePacket())>0){
		while(size--)
		msgIN.fill(Udp.read());

		if(!msgIN.hasError()){
			msgIN.route("/setDestIp",setDestIp);
			msgIN.route("/setSwFlag", setSwFlag);
			msgIN.route("/setBusyFlag", setBusyFlag);

			msgIN.route("/getStatus", getStatus);

			msgIN.route("/configStepMode", configStepMode);
			msgIN.route("/getStepMode", getStepMode);

			msgIN.route("/voltageMode", voltageMode);
			msgIN.route("/currentMode", currentMode);

			msgIN.route("/setSpdProfile",setSpdProfile);
			msgIN.route("/setMaxSpeed", setMaxSpeed);
			msgIN.route("/setMinSpeed", setMinSpeed);
			msgIN.route("/setFullSpeed", setFullSpeed);
			msgIN.route("/setAcc", setAcc);
			msgIN.route("/setDec", setDec);
			msgIN.route("/getSpdProfile",getSpdProfile);

			msgIN.route("/setSpdProfileRaw",setSpdProfileRaw);
			msgIN.route("/setMaxSpeedRaw", setMaxSpeedRaw);
			msgIN.route("/setMinSpeedRaw", setMinSpeedRaw);
			msgIN.route("/setFullSpeedRaw", setFullSpeedRaw);
			msgIN.route("/setAccRaw", setAccRaw);
			msgIN.route("/setDecRaw", setDecRaw);
			msgIN.route("/getSpdProfileRaw",getSpdProfileRaw);

			msgIN.route("/setKVAL",setKVAL);
			msgIN.route("/setAccKVAL", setAccKVAL);
			msgIN.route("/setDecKVAL", setDecKVAL);
			msgIN.route("/setRunKVAL", setRunKVAL);
			msgIN.route("/setHoldKVAL", setHoldKVAL);

			msgIN.route("/getKVAL",getKVAL);

			msgIN.route("/setTVAL",setTVAL);
			msgIN.route("/setAccTVAL", setAccTVAL);
			msgIN.route("/setDecTVAL", setDecTVAL);
			msgIN.route("/setRunTVAL", setRunTVAL);
			msgIN.route("/setHoldTVAL", setHoldTVAL);

			msgIN.route("/getTVAL",getTVAL);

			msgIN.route("/getPos", getPos);
			msgIN.route("/getMark", getMark);
			msgIN.route("/run", run);
			msgIN.route("/runRaw", runRaw);
			msgIN.route("/move", move);
			msgIN.route("/goTo", goTo);
			msgIN.route("/goToDir", goToDir);
			msgIN.route("/goUntil", goUntil);
			msgIN.route("/goUntilRaw", goUntilRaw);
			msgIN.route("/releaseSw", releaseSw);
			msgIN.route("/goHome", goHome);
			msgIN.route("/goMark", goMark);
			msgIN.route("/setMark", setMark);
			msgIN.route("/setPos", setPos);
			msgIN.route("/resetPos", resetPos);
			msgIN.route("/resetDev", resetDev);
			msgIN.route("/softStop", softStop);
			msgIN.route("/hardStop", hardStop);
			msgIN.route("/softHiZ", softHiZ);
			msgIN.route("/hardHiZ", hardHiZ);
		}
	}
}

void statusCheck()
{
	for(uint8_t i = 0; i < 4; i ++) {
		if (!isOriginReturn[i]) {
			int status = powerSteps[i].getStatus();
			uint8_t sw = (status & 0b100) >> 2;
			uint8_t busy = (status & 0b10) >> 1;
			uint8_t dir = (status & 0b10000) >> 4;

			if (i == 0 && status != 0b1110001000000111) {
				SerialUSB.println(status, BIN);
				SerialUSB.print(sw);
				SerialUSB.print(" ");
				SerialUSB.print(busy);
				SerialUSB.print(" ");
				SerialUSB.print(dir);
				SerialUSB.println();
			}

			if (sw != lastSw[i]) {
				if (isSendSw[i]) {
					sendSw(i, sw, dir);
				}
				lastSw[i] = sw;
			}
			if (busy != lastBusy[i]) {
				if (isSendBusy[i]) {
					sendBusy(i, busy);
				}

				lastBusy[i] = busy;
			}
		}
	}
}

void sendSw(uint8_t target, uint8_t sw, uint8_t dir) {
	OSCMessage newMes("/sw");
	newMes.add((int32_t)target);
	newMes.add((int32_t)sw);
	newMes.add((int32_t)dir);
	Udp.beginPacket(destIp, outPort);
	newMes.send(Udp);
	Udp.endPacket();
	newMes.empty();
}
void sendBusy(uint8_t target, uint8_t busy) {
	OSCMessage newMes("/busy");
	newMes.add((int32_t)target);
	newMes.add((int32_t)busy);
	Udp.beginPacket(destIp, outPort);
	newMes.send(Udp);
	Udp.endPacket();
	newMes.empty();
}
void loop()
{
	uint64_t time = millis();
	if (0 < time - lastTime) {
		statusCheck();
		lastTime = time;
	}


	OSCMsgReceive();
}