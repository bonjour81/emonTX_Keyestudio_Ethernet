/*
   EmonTx Shield and Ethernet Shield Sketch
   Sends CT information over Ethernet to EMONCMS server that is running locally.
   Licence: GNU GPL V3

   Edit Author: Wouter Jansen. All credit goes to OpenEnergyMonitor.org and original Ethernet Example
 */

#include "EmonLib.h"
#include <SPI.h>
#include <Ethernet.h>


#include "passwords.h"

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
float power, elapsedkWh, elapsedkWh_buffer;
//Number of pulses per wh - found or set on the meter.
int ppwh = 1; //1000 pulses/kwh = 1 pulse per wh



// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
	0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 1, 196);
// Initialize the Ethernet client instance
EthernetClient client;
//EthernetServer server(80);

// EMONCMS server IP
char emoncmsserver[] = "192.168.1.183";
// EMONCMS API write key
//String emoncmsapikey = APIKEY;
// Update frequency
const unsigned long postingInterval = 20000;
unsigned long lastConnectionTime = 0;

void printData(String node, float realPower, float supplyVoltage, float Irms);
void sendData(String node, float realPower, float supplyVoltage, float Irms);
void sendData_Compteur(String node, float realWh);
void Wh_pulse();



void setup()
{
	Serial.begin(115200);
	Serial.println("EMONTX Started!");
	// start the Ethernet connection:
	if (Ethernet.begin(mac) == 0) {
		Serial.println("Failed to configure Ethernet using DHCP");
		// try to congifure using IP address instead of DHCP:
		Ethernet.begin(mac, ip);
	}
	Serial.println("******************************");
	Serial.print("IP: "); Serial.println(Ethernet.localIP());
	Serial.print("Gateway "); Serial.println(Ethernet.gatewayIP());
	Serial.print("Subnet mask: "); Serial.println(Ethernet.subnetMask());
	Serial.print("DNS: "); Serial.println(Ethernet.dnsServerIP());
	Serial.println("******************************");
	Ethernet.setRetransmissionTimeout(300);
	Ethernet.setRetransmissionCount(1);
	// Ethernet warmup delay
	delay(2000);
	Serial.println("EMONTX HTTP client started!");
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
	//pinMode(2, INPUT_PULLUP);
	attachInterrupt(digitalPinToInterrupt(2),Wh_pulse,FALLING);
}

void loop()
{
	// Calculate all. First argument is No.of half wavelengths (crossings),second is time-out
	ct1.calcVI(20, 2000);
	ct2.calcVI(20, 2000);
	ct3.calcVI(20, 2000);
	ct4.calcVI(20, 2000);

	//Calculate power
	power = (3600000000.0 / (pulseTime - lastTime))/ppwh;
	//Find kwh elapsed
	elapsedkWh = (1.0*pulseCount/(ppwh*1000));         //multiply by 1000 to convert pulses per wh to kwh

	if (pulse_occured == true) {
		pulse_occured = false;
		Serial.print("power:");
		Serial.print(power,4);
		Serial.print("W, elapsed kWh: ");
		Serial.println(elapsedkWh,3);
	}


	if (elapsedkWh >= 0.1) {           // every 100Wh, send it
		elapsedkWh_buffer = elapsedkWh;
		pulseCount_buffer = pulseCount;
		pulseCount = 0;
		sendData_Compteur("Compteur",elapsedkWh_buffer);
	}



	// If the posting interval has passed since your last connection,
	// then connect again and send data:
	if (millis() - lastConnectionTime > postingInterval) {
		digitalWrite(LEDpin, HIGH);
		// Extract individual elements (realpower,Vrms and Irms)
		// and use them as arguments for printing and sending the data. First argument is node name.
		printData("ct1", ct1.realPower, ct1.Vrms, ct1.Irms);
		sendData("ct1", ct1.realPower, ct1.Vrms, ct1.Irms);
		delay(50);
		printData("ct2", ct2.realPower, ct2.Vrms, ct2.Irms);
		sendData("ct2", ct2.realPower, ct2.Vrms, ct2.Irms);
		delay(50);
		printData("ct3", ct3.realPower, ct3.Vrms, ct3.Irms);
		sendData("ct3", ct3.realPower, ct3.Vrms, ct3.Irms);
		delay(50);
		printData("ct4", ct4.realPower, ct4.Vrms, ct4.Irms);
		sendData("ct4", ct4.realPower, ct4.Vrms, ct4.Irms);
		delay(50);
		printData("Compteur", power, ct4.Vrms, 0);
		sendData("Compteur", power, ct4.Vrms, 0);
		digitalWrite(LEDpin, LOW);
		lastConnectionTime = millis();
	}
}

// this method makes a HTTP connection to the EmonCMS server and sends the data in correct format
void sendData(String node, float realPower, float supplyVoltage, float Irms) {
	// if there's a successful connection:
	if (client.connect(emoncmsserver, 8082)) {
		if ( (realPower>= -20) && (realPower <15000) && (supplyVoltage >= -20)  && (supplyVoltage <500) && (Irms >= -0.1) && (Irms < 60) ) {
			Serial.print("Connecting and sending JSON packet for Node ");
			Serial.println(node);
			// send the HTTP PUT request:
			client.print("GET /emoncms/input/post?node=");
			client.print(node);
			client.print("&json={");
			client.print("realPower:");
			client.print(realPower);
			client.print(",supplyVoltage:");
			client.print(supplyVoltage);
			client.print(",Irms:");
			client.print(Irms);
			client.print("}&apikey=");
			client.print(emoncmsapikey);
			client.println(" HTTP/1.1");
			client.print("Host: ");
			client.println(emoncmsserver);
			client.println("Connection: close");
			client.println();
			client.stop();
			lastConnectionTime = millis();
		} else {
			Serial.println("Some value did not pass quality check!");
		}
	}
	else {
		Serial.println("Could not connect to server. Disconnecting!");
		client.stop();
	}
}

void sendData_Compteur(String node, float realWh) {
	// if there's a successful connection:
	if (client.connect(emoncmsserver, 8082)) {
		Serial.print("Connecting and sending JSON packet for Node ");
		Serial.println(node);
		// send the HTTP PUT request:
		client.print("GET /emoncms/input/post?node=");
		client.print(node);
		client.print("&json={");
		client.print("realWh:");
		client.print(realWh);
		client.print("}&apikey=");
		client.print(emoncmsapikey);
		client.println(" HTTP/1.1");
		client.print("Host: ");
		client.println(emoncmsserver);
		client.println("Connection: close");
		client.println();
		client.stop();
		lastConnectionTime = millis();
	}
	else {
		Serial.println("Could not connect to server. Disconnecting!");
		client.stop();
	}
}






// Serial print out information on the node for debugging
void printData(String node, float realPower, float supplyVoltage, float Irms) {
	Serial.print("Measurement taken for Node ");
	Serial.print(node);
	Serial.print(". RealPower: ");
	Serial.print(realPower);
	Serial.print(" W | SupplyVoltage: ");
	Serial.print(supplyVoltage);
	Serial.print(" V | Irms: ");
	Serial.print(Irms);
	Serial.println(" A");
}



// interrupt function for optical pulse count
void Wh_pulse(){
	//detachInterrupt(digitalPinToInterrupt(2));
	//used to measure time between pulses.
	lastTime = pulseTime;
	pulseTime = micros();
	pulseCount++;
/*//Calculate power
        power = (3600000000.0 / (pulseTime - lastTime))/ppwh;
   //Find kwh elapsed
        elapsedkWh = (1.0*pulseCount/(ppwh*1000)); //multiply by 1000 to convert pulses per wh to kwh   */
	pulse_occured = true;
	//Serial.print("*");
	//attachInterrupt(digitalPinToInterrupt(2),Wh_pulse,FALLING);

}
