# V0.O1 in dev


#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include "EmonLib.h"
#include <Dns.h> // for ntp


// Create  instances for each CT channel
EnergyMonitor ct1;
EnergyMonitor ct2;
EnergyMonitor ct3;
EnergyMonitor ct4;
// On-board emonTx LED
const int LEDpin = 9;
// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
	0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 1, 196);
// Initialize the Ethernet client instance
EthernetClient client;
// EMONCMS server IP
char emoncmsserver[] = "192.168.0.178";
// EMONCMS API write key
String emoncmsapikey = "YOUR API KEY";
// Update frequency
const unsigned long postingInterval = 10000;
unsigned long lastConnectionTime = 0;


//optical pulse counter settings
//Number of pulses, used to measure energy.
long pulseCount = 0;
//Used to measure power.
unsigned long pulseTime,lastTime;
//power and energy
double power, elapsedkWh;
//Number of pulses per wh - found or set on the meter.
int ppwh = 1; //1000 pulses/kwh = 1 pulse per wh


/* ******** NTP Server Settings ******** */
/* us.pool.ntp.org NTP server
   (Set to your time server of choice) */
IPAddress timeServer(216, 23, 247, 62);

/* Set this to the offset (in seconds) to your local time
   This example is GMT - 4 */
const long timeZoneOffset = -14400L;

/* Syncs to NTP server every 15 seconds for testing,
   set to 1 hour or more to be reasonable */
unsigned int ntpSyncTime = 3600;







void sendData(String node, float realPower, float supplyVoltage, float Irms);
void printData(String node, float realPower, float supplyVoltage, float Irms);
void Wh_pulse();



void setup() {
	Serial.begin(115200);
	Serial.println("EMONTX Started!");
	// start the Ethernet connection:
	if (Ethernet.begin(mac) == 0) {
		Serial.println("Failed to configure Ethernet using DHCP");
		// try to congifure using IP address instead of DHCP:
		Ethernet.begin(mac, ip);
	}
	Ethernet.setRetransmissionTimeout(300);
	Ethernet.setRetransmissionCount(1);
	// Ethernet warmup delay
	delay(2000);
	Serial.println("EMONTX HTTP client started!");
	// First argument is analog pin used by CT connection, second is calibration factor.
	// Calibration factor = CT ratio / burden resistance = (100A / 0.05A) / 33 Ohms = 60.606
	// First argument is analog pin used by CT connection
	ct1.current(1, 30);
	ct2.current(2, 30);
	ct3.current(3, 30);
	ct4.current(4, 30);
	// First argument is analog pin used by AC-AC adapter. Second is calibration, third is phase shift.
	// See the OpenEnergyMonitor guides to find the calibration factor of some common AC-AC adapters or calculate it yourself.
	// Use a multimeter to measure the voltage around a resistive load. Compare the voltage measured to the reported voltage of the emonTx here.
	// Recalibrate using: New calibration = existing calibration × (correct reading ÷ emonTx reading)
	// More information: https://learn.openenergymonitor.org/electricity-monitoring/ctac/calibration
	ct1.voltage(0, 260, 1.7);
	ct2.voltage(0, 260, 1.7);
	ct3.voltage(0, 260, 1.7);
	ct4.voltage(0, 260, 1.7);
	// Setup indicator LED
	pinMode(LEDpin, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(3),Wh_pulse,RISING);

  IPAddress testIP;
DNSClient dns;
dns.begin(Ethernet.dnsServerIP());
dns.getHostByName("fr.pool.ntp.org",testIP);
Serial.print("NTP IP from the pool: ");
Serial.println(testIP);


}

void loop()
{


// Calculate all. First argument is No.of half wavelengths (crossings),second is time-out
	ct1.calcVI(20, 2000);
	ct2.calcVI(20, 2000);
	ct3.calcVI(20, 2000);
	ct4.calcVI(20, 2000);
// If the posting interval has passed since your last connection,
// then connect again and send data:
	if (millis() - lastConnectionTime > postingInterval) {
    //To make sure the DHCP lease is properly renewed when needed, be sure to call Ethernet.maintain() regularly.
    	Ethernet.maintain();
		digitalWrite(LEDpin, HIGH);
		// Extract individual elements (realpower,Vrms and Irms)
		// and use them as arguments for printing and sending the data. First argument is node name.
		printData("ct1", ct1.realPower, ct1.Vrms, ct1.Irms);
		sendData("ct1", ct1.realPower, ct1.Vrms, ct1.Irms);
		printData("ct2", ct2.realPower, ct2.Vrms, ct2.Irms);
		sendData("ct2", ct2.realPower, ct2.Vrms, ct2.Irms);
		printData("ct3", ct3.realPower, ct3.Vrms, ct3.Irms);
		sendData("ct3", ct3.realPower, ct3.Vrms, ct3.Irms);
		printData("ct4", ct4.realPower, ct4.Vrms, ct4.Irms);
		sendData("ct4", ct4.realPower, ct4.Vrms, ct4.Irms);
		digitalWrite(LEDpin, LOW);
	}
}  // end Loop

void Wh_pulse(){
  //used to measure time between pulses.
lastTime = pulseTime;
pulseTime = micros();
//pulseCounter
pulseCount++;
//Calculate power
power = (3600000000.0 / (pulseTime - lastTime))/ppwh;
//Find kwh elapsed
elapsedkWh = (1.0*pulseCount/(ppwh*1000)); //multiply by 1000 to convert pulses per wh to kwh
//Print the values.
Serial.print(power,4);
Serial.print(" ");
Serial.println(elapsedkWh,3);
}



// this method makes a HTTP connection to the EmonCMS server and sends the data in correct format
void sendData(String node, float realPower, float supplyVoltage, float Irms) {
  // if there's a successful connection:
  if (client.connect(emoncmsserver, 80)) {
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
