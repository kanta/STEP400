/*
 * step400_OSCtester.ino
 *
 * Created: 9/19/2019 4:53:04 PM
 * Author: kanta
 */ 

#include <Arduino.h>
#include <Ethernet.h>
#include <OSCMessage.h>
#include <powerSTEP01ArduinoLibrary.h>
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
	powerStepSPI.setClockDivider(128); // default 4
	pinPeripheral(POWERSTEP_MOSI, PIO_SERCOM_ALT);
	pinPeripheral(POWERSTEP_SCK, PIO_SERCOM_ALT);
	pinPeripheral(POWERSTEP_MISO , PIO_SERCOM_ALT);
	powerStepSPI.setDataMode(SPI_MODE3);
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
		//powerSteps[i].setOscMode(EXT_24MHZ_OSCOUT_INVERT);
		powerSteps[i].setOscMode(INT_16MHZ);
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

void setDestIp(OSCMessage &msg ,int addrOffset) {
	destIp = Udp.remoteIP();
	sendOneData("/newDestIp", (int32_t)destIp[3]);
	digitalWrite(ledPin, !digitalRead(ledPin));
}

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

void setSpdProfile(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	float t = msg.getFloat(1);
	powerSteps[target].setAcc(t);
	t = msg.getFloat(2);
	powerSteps[target].setDec(t);
	t = msg.getFloat(3);
	powerSteps[target].setMaxSpeed(t);
}

void run(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	float spd = msg.getFloat(1);
	boolean dir = spd>0;
	powerSteps[target].run(dir,abs(spd));
}

void getPos(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	sendOneData("/pos", powerSteps[target].getPos());
}

void setPos(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	long t = msg.getInt(1);
	powerSteps[target].setPos(t);
}

void hardStop(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	powerSteps[target].hardStop();
}

void hardHiZ(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	powerSteps[target].hardHiZ();
}

void getStatus(OSCMessage &msg ,int addrOffset) {
	uint8_t target = constrain(msg.getInt(0),0,3);
	sendOneData("/status", powerSteps[target].getStatus());
}

void OSCMsgReceive() {
	OSCMessage msgIN;
	int size;
	if((size = Udp.parsePacket())>0){
		while(size--)
		msgIN.fill(Udp.read());

		if(!msgIN.hasError()){
			msgIN.route("/getStatus", getStatus);
			msgIN.route("/run",run);
			msgIN.route("/setSpdProfile",setSpdProfile);
			msgIN.route("/getPos",getPos);
			msgIN.route("/setPos",setPos);
			msgIN.route("/setKVAL",setKVAL);
			msgIN.route("/hardStop",hardStop);
			msgIN.route("/hardHiZ",hardHiZ);
			msgIN.route("/setDestIp",setDestIp);
		}
	}
}

void swCheck()
{
  static byte lastVal;

}
void loop()
{
	OSCMsgReceive();
	swCheck();

}