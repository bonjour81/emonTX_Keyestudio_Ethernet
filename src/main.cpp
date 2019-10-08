// V0.10 in dev swith to MQTT

#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
//#include <EthernetUdp.h>^ // for ntp
//#include <Dns.h> // for ntp
#include <Time.h>
#include "EmonLib.h"
#include <avr/wdt.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

#include "passwords.h"

#define FW_VERSION  "10"
/* *********************************** Emon ***************************** */
//char emoncmsserver[] = "192.168.1.152";
//String emoncmsapikey = "YOUR API KEY";  => Moved in passwords.h
const unsigned long postingInterval = 20000; // Update frequency
unsigned long lastConnectionTime = 0;

// Create  instances for each CT channel
EnergyMonitor ct1;
EnergyMonitor ct2;
EnergyMonitor ct3;
EnergyMonitor ct4;
// On-board emonTx LED
const int LEDpin = 9;

//optical pulse counter settings, Number of pulses, used to measure energy.
long pulseCount = 0;
long pulseCount_buffer;
bool pulse_occured = false;
//Used to calculate instant power from time between 2 pulses
unsigned long pulseTime,lastTime;
//power and energy
double power, elapsedkWh, elapsedkWh_buffer;
//Number of pulses per wh - found or set on the meter.
int ppwh = 1; //1000 pulses/kwh = 1 pulse per wh

/* *********************************** Ethernet ************************* */
// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 1, 196);  // IP address of the devices (if no DHCP)
// Initialize the Ethernet client instance
EthernetClient client;

/* *********************************** MQTT ************************* */
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, "emonTX", AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish ct1p_pub      = Adafruit_MQTT_Publish(&mqtt, "emon/ct1_power", 1);
Adafruit_MQTT_Publish ct2p_pub      = Adafruit_MQTT_Publish(&mqtt, "emon/ct2_power", 1);
Adafruit_MQTT_Publish ct3p_pub      = Adafruit_MQTT_Publish(&mqtt, "emon/ct3_power", 1);
Adafruit_MQTT_Publish ct4p_pub      = Adafruit_MQTT_Publish(&mqtt, "emon/ct4_power", 1);
Adafruit_MQTT_Publish compteurp_pub = Adafruit_MQTT_Publish(&mqtt, "emon/compteur_power", 1);
Adafruit_MQTT_Publish compteurpulse_pub = Adafruit_MQTT_Publish(&mqtt, "emon/compteur_pulses", 1);
Adafruit_MQTT_Publish compteurkwh_pub = Adafruit_MQTT_Publish(&mqtt, "emon/compteur_kwh", 1);

Adafruit_MQTT_Publish ct1v_pub      = Adafruit_MQTT_Publish(&mqtt, "emon/ct1_volt", 1);
Adafruit_MQTT_Publish ct2v_pub      = Adafruit_MQTT_Publish(&mqtt, "emon/ct2_volt", 1);
Adafruit_MQTT_Publish ct3v_pub      = Adafruit_MQTT_Publish(&mqtt, "emon/ct3_volt", 1);
Adafruit_MQTT_Publish ct4v_pub      = Adafruit_MQTT_Publish(&mqtt, "emon/ct4_volt", 1);
Adafruit_MQTT_Publish compteurv_pub = Adafruit_MQTT_Publish(&mqtt, "emon/compteur_volt", 1);

Adafruit_MQTT_Publish ct1i_pub      = Adafruit_MQTT_Publish(&mqtt, "emon/ct1_amp", 1);
Adafruit_MQTT_Publish ct2i_pub      = Adafruit_MQTT_Publish(&mqtt, "emon/ct2_amp", 1);
Adafruit_MQTT_Publish ct3i_pub      = Adafruit_MQTT_Publish(&mqtt, "emon/ct3_amp", 1);
Adafruit_MQTT_Publish ct4i_pub      = Adafruit_MQTT_Publish(&mqtt, "emon/ct4_amp", 1);

Adafruit_MQTT_Publish Version_pub     = Adafruit_MQTT_Publish(&mqtt, "emonTX/Version", 1);
Adafruit_MQTT_Publish Status_pub      = Adafruit_MQTT_Publish(&mqtt, "emonTX/Status", 1);


/* ******************************** functions declaration *************** */
void Wh_pulse();
void (* resetFunc) (void) = 0; //declare reset function @ address 0
void eth_connect();
void setup_mqtt();

/* *********************** SETUP() ************************************** */
void setup() {
	Serial.begin(115200);
	Serial.println("EMONTX Started!");
	eth_connect();
	Serial.println("EMONTX HTTP client started!");
	//void ntp_sync()
	void setup_mqtt();
	// First argument is analog pin used by CT connection, second is calibration factor.
	// Calibration factor = CT ratio / burden resistance = (100A / 0.05A) / 33 Ohms = 60.606
	// First argument is analog pin used by CT connection
	ct1.current(1, 30); // 30 for SCT-013-030  30A = 1Volt
	ct2.current(2, 30);
	ct3.current(3, 30);
	ct4.current(4, 30);
	// First argument is analog pin used by AC-AC adapter. Second is calibration, third is phase shift.
	// See the OpenEnergyMonitor guides to find the calibration factor of some common AC-AC adapters or calculate it yourself.
	// Use a multimeter to measure the voltage around a resistive load. Compare the voltage measured to the reported voltage of the emonTx here.
	// Recalibrate using: New calibration = existing calibration × (correct reading ÷ emonTx reading)
	// More information: https://learn.openenergymonitor.org/electricity-monitoring/ctac/calibration
	ct1.voltage(0, 243.5, 1.7);
	ct2.voltage(0, 243.5, 1.7);
	ct3.voltage(0, 243.5, 1.7);
	ct4.voltage(0, 243.5, 1.7);
	// Setup indicator LED
	pinMode(LEDpin, OUTPUT);
	attachInterrupt(digitalPinToInterrupt(2),Wh_pulse,FALLING);
}

void loop()
{
	wdt_enable(WDTO_8S);  // watchdog 8sec
// Calculate all. First argument is No.of half wavelengths (crossings),second is time-out
	ct1.calcVI(20, 2000);
	ct2.calcVI(20, 2000);
	wdt_enable(WDTO_8S);
	ct3.calcVI(20, 2000);
	ct4.calcVI(20, 2000);
// If the posting interval has passed since your last connection,
// then connect again and send data:
	wdt_enable(WDTO_8S);
	Serial.println("loop");
	if (millis() - lastConnectionTime > postingInterval) {
		digitalWrite(LEDpin, HIGH);
		void setup_mqtt();
		if (ct1.realPower > 0 && ct1.realPower <10000) ct1p_pub.publish(ct1.realPower);
		if (ct2.realPower > 0 && ct2.realPower <10000) ct2p_pub.publish(ct2.realPower);
		if (ct3.realPower > 0 && ct3.realPower <10000) ct3p_pub.publish(ct3.realPower);
		if (ct4.realPower > 0 && ct4.realPower <10000) ct4p_pub.publish(ct4.realPower);
		if (ct1.Vrms >= 0 && ct1.Vrms <1000) ct1v_pub.publish(ct1.Vrms);
		if (ct2.Vrms >= 0 && ct2.Vrms <1000) ct2v_pub.publish(ct2.Vrms);
		if (ct3.Vrms >= 0 && ct3.Vrms <1000) ct3v_pub.publish(ct3.Vrms);
		if (ct4.Vrms >= 0 && ct4.Vrms <1000) ct4v_pub.publish(ct4.Vrms);
		if (ct1.Irms >= 0 && ct1.Irms <100) ct1i_pub.publish(ct1.Irms);
		if (ct2.Irms >= 0 && ct2.Irms <100) ct2i_pub.publish(ct2.Irms);
		if (ct3.Irms >= 0 && ct3.Irms <100) ct3i_pub.publish(ct3.Irms);
		if (ct4.Irms >= 0 && ct4.Irms <100) ct4i_pub.publish(ct4.Irms);
		if (power > 0 && power <10000) compteurp_pub.publish(power);
		if (((ct1.Vrms+ct2.Vrms+ct3.Vrms+ct4.Vrms)/4) >= 0 && ((ct1.Vrms+ct2.Vrms+ct3.Vrms+ct4.Vrms)/4) <1000) compteurv_pub.publish((ct1.Vrms+ct2.Vrms+ct3.Vrms+ct4.Vrms)/4);
	}
	if (elapsedkWh >= 0.1) {   // every 100Wh, send it
		elapsedkWh_buffer = elapsedkWh;
		pulseCount_buffer = pulseCount;
		pulseCount = 0;
		compteurkwh_pub.publish(elapsedkWh_buffer);
		compteurpulse_pub.publish(pulseCount_buffer);
	}
	if (pulse_occured == true) {
		pulse_occured = false;
		Serial.print("power:");
		Serial.print(power,4);
		Serial.print("W, elapsed kWh: ");
		Serial.println(elapsedkWh,3);
	}
	//To make sure the DHCP lease is properly renewed when needed, be sure to call Ethernet.maintain() regularly.
	Ethernet.maintain();
	delay(500);
	if(Ethernet.linkStatus() == LinkOFF) { //if connexion lost, try reconnect
		Serial.println("Ethernet seems disconnected, let's try reconnect");
		eth_connect();
	}
}  // end Loop



// interrupt function for optical pulse count
void Wh_pulse(){
//	detachInterrupt(digitalPinToInterrupt(2));
	//used to measure time between pulses.
	lastTime = pulseTime;
	pulseTime = micros();
	pulseCount++;
//Calculate power
	power = (3600000000.0 / (pulseTime - lastTime))/ppwh;
//Find kwh elapsed
	elapsedkWh = (1.0*pulseCount/(ppwh*1000)); //multiply by 1000 to convert pulses per wh to kwh
//Print the values.
	pulse_occured = true;
}


void eth_connect() {
	Serial.print("Connecting to Ethernet...");
	// Ethernet setup
	int i = 0;
	int DHCP = 0;
	//Try to get dhcp settings 30 times before giving up
	while( DHCP == 0 && i < 30) {
		DHCP = Ethernet.begin(mac); // return 1 if successfull
		Serial.print(".");
		delay(500);
		i++;
	}
	if(DHCP == 0) {
		Serial.println("DHCP FAILED, reseting...");
		//Serial.flush();
		delay(60000);
		resetFunc(); //call reset
	}
	Serial.println("DHCP Success!");
	Serial.print("IP: "); Serial.println(Ethernet.localIP());
	Serial.print("Gateway "); Serial.println(Ethernet.gatewayIP());
	Serial.print("Subnet mask: "); Serial.println(Ethernet.subnetMask());
	Serial.print("DNS: "); Serial.println(Ethernet.dnsServerIP());
	Ethernet.setRetransmissionTimeout(300);
	Ethernet.setRetransmissionCount(1);
	// Ethernet warmup delay
	delay(2000);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  setup_mqtt() : connexion to mosquitto server
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup_mqtt() {
	int8_t ret;
	Serial.println("entering mqtt function....");
	// Stop if already connected.
	if (mqtt.connected()) {
		Serial.println("mqtt already connected");
		return;
	}
	Serial.println("Connecting to mqtt");
	while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
		Serial.print(".");
		mqtt.disconnect();
		delay(1000); // wait 1 seconds
	}
	Status_pub.publish("Online!");
	delay(5);
	Version_pub.publish(FW_VERSION);
}
