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

/* *************************** NTP Server Settings ********************* */
/* us.pool.ntp.org NTP server
   (Set to your time server of choice) */
///IPAddress timeServer(216, 23, 247, 62);
///IPAddress ntp_IP;
/* Set this to the offset (in seconds) to your local time
   This example is GMT - 4 */
///const long timeZoneOffset = 7200;
/* Syncs to NTP server every 15 seconds for testing,
   set to 1 hour or more to be reasonable */
///unsigned int ntpSyncTime = 3600;
// local port to listen for UDP packets
///unsigned int localPort = 8888;
// NTP time stamp is in the first 48 bytes of the message
///const int NTP_PACKET_SIZE= 48;
// Buffer to hold incoming and outgoing packets
///byte packetBuffer[NTP_PACKET_SIZE];
// A UDP instance to let us send and receive packets over UDP
///EthernetUDP Udp;
// Keeps track of how long ago we updated the NTP server
///unsigned long ntpLastUpdate = 0;
// Check last time clock displayed (Not in Production)
///time_t prevDisplay = 0;


/* ******************************** functions declaration *************** */
void sendData(String node, float realPower, float supplyVoltage, float Irms);
void sendData_Wh(String node);
void printData(String node, float realPower, float supplyVoltage, float Irms);
void Wh_pulse();
//int getTimeAndDate();
unsigned long sendNTPpacket(IPAddress& address);
void (* resetFunc) (void) = 0; //declare reset function @ address 0
//void ntp_sync();
//void ntp_resync();
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
	if (millis() - lastConnectionTime > postingInterval) {
		digitalWrite(LEDpin, HIGH);
		void setup_mqtt();
		ct1p_pub.publish(ct1.realPower);
		ct2p_pub.publish(ct2.realPower);
		ct3p_pub.publish(ct3.realPower);
		ct4p_pub.publish(ct4.realPower);
		ct1v_pub.publish(ct1.Vrms);
		ct2v_pub.publish(ct2.Vrms);
		ct3v_pub.publish(ct3.Vrms);
		ct4v_pub.publish(ct4.Vrms);
		ct1i_pub.publish(ct1.Irms);
		ct2i_pub.publish(ct2.Irms);
		ct3i_pub.publish(ct3.Irms);
		ct4i_pub.publish(ct4.Irms);
		compteurp_pub.publish(power);

		compteurv_pub.publish((ct1.Vrms+ct2.Vrms+ct3.Vrms+ct4.Vrms)/4);

		// Extract individual elements (realpower,Vrms and Irms)
		// and use them as arguments for printing and sending the data. First argument is node name.
		/*printData("ct1", ct1.realPower, ct1.Vrms, ct1.Irms);
		sendData("ct1", ct1.realPower, ct1.Vrms, ct1.Irms);
		printData("ct2", ct2.realPower, ct2.Vrms, ct2.Irms);
		sendData("ct2", ct2.realPower, ct2.Vrms, ct2.Irms);
		printData("ct3", ct3.realPower, ct3.Vrms, ct3.Irms);
		sendData("ct3", ct3.realPower, ct3.Vrms, ct3.Irms);
		printData("ct4", ct4.realPower, ct4.Vrms, ct4.Irms);
		sendData("ct4", ct4.realPower, ct4.Vrms, ct4.Irms);
		printData("Compteur", power, ct4.Vrms, 0);
		sendData("Compteur", power, ct4.Vrms, 0);
		digitalWrite(LEDpin, LOW);
		ntp_resync(); // has it may take some seconds, let's do it after the periodic emission.*/
	}
	if (elapsedkWh >= 0.1) {   // every 100Wh, send it
		elapsedkWh_buffer = elapsedkWh;
		pulseCount = 0;
		compteurp_pub.publish(elapsedkWh_buffer);
		//sendData_Wh("Compteur");
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
/*
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
}*/
/*
// this method makes a HTTP connection to the EmonCMS server and sends the data in correct format
void sendData_Wh(String node) {
	// if there's a successful connection:
	if (client.connect(emoncmsserver, 80)) {
		elapsedkWh_buffer = elapsedkWh;
		pulseCount = 0;
		Serial.print("Connecting and sending JSON packet for Node ");
		Serial.print(node); Serial.print(" kWh:"); Serial.println(elapsedkWh_buffer);
		// send the HTTP PUT request:
		client.print("GET /emoncms/input/post?node=");
		client.print(node);
		client.print("&json={");
		client.print("realWh:");
		client.print(elapsedkWh_buffer);
		client.print("}&apikey=");
		client.print(emoncmsapikey);
		client.println(" HTTP/1.1");
		client.print("Host: ");
		client.println(emoncmsserver);
		client.println("Connection: close");
		client.println();
		client.stop();
	}
	else {
		Serial.println("Could not connect to server. Disconnecting!");
		client.stop();
	}
}*/



// Serial print out information on the node for debugging
/*void printData(String node, float realPower, float supplyVoltage, float Irms) {
	Serial.print("Measurement taken for Node ");
	Serial.print(node);
	Serial.print(". RealPower: ");
	Serial.print(realPower);
	Serial.print(" W | SupplyVoltage: ");
	Serial.print(supplyVoltage);
	Serial.print(" V | Irms: ");
	Serial.print(Irms);
	Serial.println(" A");
}*/

// NTP update
// Do not alter this function, it is used by the system
/*
int getTimeAndDate() {
	int flag=0;
	Udp.begin(localPort);
	//sendNTPpacket(timeServer);
	sendNTPpacket(ntp_IP);
	delay(1000);
	if (Udp.parsePacket()) {
		Udp.read(packetBuffer,NTP_PACKET_SIZE); // read the packet into the buffer
		unsigned long highWord, lowWord, epoch;
		highWord = word(packetBuffer[40], packetBuffer[41]);
		lowWord = word(packetBuffer[42], packetBuffer[43]);
		epoch = highWord << 16 | lowWord;
		epoch = epoch - 2208988800 + timeZoneOffset;
		flag=1;
		setTime(epoch);
		ntpLastUpdate = now();
	}
	return flag;
}*/
/*
// Do not alter this function, it is used by the system
unsigned long sendNTPpacket(IPAddress& address)
{
	memset(packetBuffer, 0, NTP_PACKET_SIZE);
	packetBuffer[0] = 0b11100011;
	packetBuffer[1] = 0;
	packetBuffer[2] = 6;
	packetBuffer[3] = 0xEC;
	packetBuffer[12]  = 49;
	packetBuffer[13]  = 0x4E;
	packetBuffer[14]  = 49;
	packetBuffer[15]  = 52;
	Udp.beginPacket(address, 123);
	Udp.write(packetBuffer,NTP_PACKET_SIZE);
	Udp.endPacket();
}*/
/*
void ntp_sync() {
	// preparing NTP timeset.
	Serial.print("now:"); Serial.println(now());
	DNSClient dns;
	dns.begin(Ethernet.dnsServerIP());
	dns.getHostByName("fr.pool.ntp.org",ntp_IP);
	Serial.print("NTP IP from fr.pool.ntp.org: "); Serial.println(ntp_IP);
	Serial.print("Trying to get time from NTP, ");
	//Try to get the date and time
	int trys=0;
	while(!getTimeAndDate() && trys<10) {
		Serial.print(".");
		delay(1000);
		trys++;
	}
	Serial.print(" Time, now():"); Serial.println(now());
}
*/
/*
void ntp_resync() {
	if(now()-ntpLastUpdate > ntpSyncTime) {
		int trys=0;
		while(!getTimeAndDate() && trys<10) {
			trys++;
		}
		if(trys<10) {
			Serial.println("ntp server update success");
		}
		else{
			Serial.println("ntp server update failed");
		}
	}
}
*/

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
   // Stop if already connected.
   if (mqtt.connected()) {
      return;
   }
	 Serial.println("Connecting to mqtt");
   while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
      Serial.print(".");
      mqtt.disconnect();
      delay(1000);  // wait 1 seconds
         }
   Status_pub.publish("Online!");
   delay(5);
   Version_pub.publish(FW_VERSION);
}
