// V0.11 in dev swith to MQTT

#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
//#include <Time.h>
#include <PubSubClient.h>
#include "EmonLib.h"
#include <avr/wdt.h>




#include "passwords.h"

#define FW_VERSION  "20"

const unsigned long postingInterval = 20000; // Update frequency
unsigned long lastConnectionTime = 0;

// Create  instances for each CT channel
EnergyMonitor ct1;
EnergyMonitor ct2;
EnergyMonitor ct3;
EnergyMonitor ct4;
// On-board emonTx LED
const int LEDpin = 9;

int i = 0;

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
EthernetClient ethclient;


/* *********************************** MQTT ************************* */
void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
}

PubSubClient mqtt(broker, BROKER_SERVERPORT, callback, ethclient);



/* *********************************** MQTT *************************
Adafruit_MQTT_Client mqtt(&ethclient, AIO_SERVER, AIO_SERVERPORT, "emonTX", AIO_USERNAME, AIO_KEY);
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
*/

/* ******************************** functions declaration *************** */
void Wh_pulse();
//void (* resetFunc) (void) = 0; //declare reset function @ address 0
//void eth_connect();
void setup_mqtt();




void setup() {
	// put your setup code here, to run once:
	Serial.begin(115200);
	Serial.println("KEYESTUDIO EMONTX Booting!");
	Serial.print("Connecting to Ethernet...Status DCHP:");
	int DHCP_stat = 0;
	wdt_enable(WDTO_8S);
	DHCP_stat = Ethernet.begin(mac);
	if (DHCP_stat == 1) {
		Serial.println("successfull"); // return 1 if successfull
	}
	else if (DHCP_stat == 0) {
		Serial.println("failed"); // return 1 if successfull
	}
	Serial.print("IP: "); Serial.println(Ethernet.localIP());
	Serial.print("Gateway "); Serial.println(Ethernet.gatewayIP());
	Serial.print("Subnet mask: "); Serial.println(Ethernet.subnetMask());
	Serial.print("DNS: "); Serial.println(Ethernet.dnsServerIP());
	Ethernet.setRetransmissionTimeout(300);
	Ethernet.setRetransmissionCount(1);
	wdt_disable();
	delay(4000);  // ethernet warmup

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
	// Recalibrate using: New calibration = existing calibration Ã— (correct reading Ã· emonTx reading)
	// More information: https://learn.openenergymonitor.org/electricity-monitoring/ctac/calibration
	ct1.voltage(0, 243.5, 1.7);
	ct2.voltage(0, 243.5, 1.7);
	ct3.voltage(0, 243.5, 1.7);
	ct4.voltage(0, 243.5, 1.7);
	// Setup indicator LED
	pinMode(LEDpin, OUTPUT);
	// attachInterrupt for optical pulse counter
	//attachInterrupt(digitalPinToInterrupt(2),Wh_pulse,FALLING);
	Serial.println("boot done");
}

void loop() {
	Ethernet.maintain();
	delay(500);
	setup_mqtt();
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

	if (millis() - lastConnectionTime > 10) {   // publish every 10sec, also work as "keep alive" mqtt connexion
	  mqtt.publish("emonTX/Version",FW_VERSION);
	  delay(5);
	  mqtt.publish("emonTX/Status","Online!");
	}

	if (millis() - lastConnectionTime > postingInterval) {
		if (ct1.realPower > 0 && ct1.realPower <10000) mqtt.publish("emonx/ct1_power", String(ct1.realPower).c_str());
		if (ct2.realPower > 0 && ct2.realPower <10000) mqtt.publish("emonx/ct2_power", String(ct2.realPower).c_str());
		if (ct3.realPower > 0 && ct3.realPower <10000) mqtt.publish("emonx/ct3_power", String(ct3.realPower).c_str());
		if (ct4.realPower > 0 && ct4.realPower <10000) mqtt.publish("emonx/ct4_power", String(ct4.realPower).c_str());

		if (ct1.Vrms >= 0 && ct1.Vrms <1000)           mqtt.publish("emonx/ct1_volt", String(ct1.Vrms).c_str());
		if (ct2.Vrms >= 0 && ct2.Vrms <1000)           mqtt.publish("emonx/ct2_volt", String(ct2.Vrms).c_str());
		if (ct3.Vrms >= 0 && ct3.Vrms <1000)           mqtt.publish("emonx/ct3_volt", String(ct3.Vrms).c_str());
		if (ct4.Vrms >= 0 && ct4.Vrms <1000)           mqtt.publish("emonx/ct4_volt", String(ct4.Vrms).c_str());

		if (ct1.Irms >= 0 && ct1.Irms <100)            mqtt.publish("emonx/ct1_amp", String(ct1.Irms).c_str());
		if (ct2.Irms >= 0 && ct2.Irms <100)            mqtt.publish("emonx/ct2_amp", String(ct2.Irms).c_str());
		if (ct3.Irms >= 0 && ct3.Irms <100)            mqtt.publish("emonx/ct3_amp", String(ct3.Irms).c_str());
		if (ct4.Irms >= 0 && ct4.Irms <100)            mqtt.publish("emonx/ct4_amp", String(ct4.Irms).c_str());

		if (power > 0 && power <10000)                 mqtt.publish("emonx/compteur_power", String(power).c_str());
		if ( ((ct1.Vrms+ct2.Vrms+ct3.Vrms+ct4.Vrms)/4) >= 0 && ((ct1.Vrms+ct2.Vrms+ct3.Vrms+ct4.Vrms)/4) <1000) mqtt.publish("emonx/compteur_volt", String((ct1.Vrms+ct2.Vrms+ct3.Vrms+ct4.Vrms)/4 ).c_str());
	}


	if (pulse_occured == true) {
		pulse_occured = false;
		Serial.print("power:");
		Serial.print(power,4);
		Serial.print("W, elapsed kWh: ");
		Serial.println(elapsedkWh,3);
	}






	lastConnectionTime = millis();
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  setup_mqtt() : connexion to mosquitto server
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup_mqtt() {
	Serial.println("entering mqtt function..."); Serial.flush();

	if (mqtt.connected()) {
		Serial.println("Already connected to MQTT broker");
		return;
	}
	Serial.print("connecting to MQTT broker");
    mqtt.connect("emonTX", BROKER_USERNAME, BROKER_KEY);
	Serial.print("MQTT connexion state is:");Serial.println(mqtt.state());
	delay(100);
	if (!mqtt.connected()) {
		Serial.println("MQTT connexion failed");
		wdt_enable(WDTO_8S); while (1);
    }
}
