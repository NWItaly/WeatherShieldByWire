#include "Ethernet.h"
#include <Wire.h>  //I2C needed for sensors

#if PRESSURE == 1
#include "SparkFunMPL3115A2.h"  //Pressure sensor - Search "SparkFun MPL3115" and install from Library Manager
#endif

#if HUMIDITY == 1
#include "SparkFun_Si7021_Breakout_Library.h"  //Humidity sensor - Search "SparkFun Si7021" and install from Library Manager
#endif

#if PRESSURE == 1
MPL3115A2 myPressure;  // Create an instance of the pressure sensor
#endif
#if HUMIDITY == 1
Weather myHumidity;  // Create an instance of the humidity sensor
#endif

// Hardware pin definitions
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  digital I/O pins
const PROGMEM byte WSPEED = 3;
const PROGMEM byte RAIN = 2;

// analog I/O pins
const PROGMEM byte REFERENCE_3V3 = A3;
const PROGMEM byte LIGHT = A1;
const PROGMEM byte BATT = A2;
const PROGMEM byte WDIR = A0;
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

/* NET */
unsigned long lastConnectionTime = 0;      // last time you connected to the server, in milliseconds
unsigned int postingInterval = 10 * 1000;  // delay between updates, in milliseconds
byte failedAttempts = 0;
const PROGMEM byte retryBeforeReboot = 10;  // approximately 10 minutes

// Global Variables
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

unsigned long lastWindCheck = 0;
volatile unsigned long lastWindIRQ = 0;
volatile byte windClicks = 0;

// volatiles are subject to modification by IRQs
volatile unsigned long rainlastIRQ;
volatile float rain;
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// Interrupt routines (these are called by the hardware interrupts, not by the main code)
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void rainIRQ()
// Count rain gauge bucket tips as they occur
// Activated by the magnet and reed switch in the rain gauge, attached to input D2
{
  if (millis() - rainlastIRQ > 10)  // ignore switch-bounce glitches less than 10mS after initial edge
  {
    rain += 0.2794;          // Each dump is 0.011" of water -> 0.2794 mm
    rainlastIRQ = millis();  // set up for next event
  }
}  // rainIRQ

void wspeedIRQ()
// Activated by the magnet in the anemometer (2 ticks per rotation), attached to input D3
{
  if (millis() - lastWindIRQ > 10)  // Ignore switch-bounce glitches less than 10ms after the reed switch closes
  {
    windClicks++;
    lastWindIRQ = millis();  // Grab the current time
  }
}  // wspeedIRQ

#if PRESSURE == 1
void initPressure() {
  myPressure.begin();
  myPressure.setModeBarometer();
  myPressure.setOversampleRate(7);
  myPressure.enableEventFlags();
  Sprintln(F("Set pressure sensor: ok"));
}  // initPressure
#endif

void setup() {

  //  turn on serial (for debugging)
  Serial.begin(9600);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for Leonardo only
  }

  // Setup indicator LED
  Sprintln(F("Setup LED to inform when it sends data."));
  pinMode(STAT1, OUTPUT);  // Status LED Blue
  pinMode(STAT2, OUTPUT);  // Status LED Green

  pinMode(WSPEED, INPUT_PULLUP);  // input from wind meters windspeed sensor
  pinMode(RAIN, INPUT_PULLUP);    // input from wind meters rain gauge sensor
  pinMode(REFERENCE_3V3, INPUT);
  pinMode(LIGHT, INPUT);

  InitConfig();
  InitEthernet();

#if PRESSURE == 1
  // Configure the pressure sensor
  initPressure();
#endif

#if HUMIDITY == 1
  // Configure the humidity sensor
  myHumidity.begin();
  Sprintln(F("Set humidity sensor: ok"));
#endif

  // attach external interrupt pins to IRQ functions
  attachInterrupt(0, rainIRQ, FALLING);
  attachInterrupt(1, wspeedIRQ, FALLING);

  // turn on interrupts
  interrupts();

  Sprintln(F("Set interrupts: ok"));

  Sprintln(F("Setup ends"));

}  // setup

void loop() {

  if (!EthernetManager()) {
    failedAttempts++;
    Sprint(F("Failed attempt: "));
    Sprintln(failedAttempts);
    Blink(5);
  } else if (!client.connected() && (millis() - lastConnectionTime > postingInterval)) {
    // if you're not connected, and at least <postingInterval> seconds have
    // passed sinceyour last connection, then connect again and send data

    bool sendData = SendData();

    lastConnectionTime = millis();
    if (sendData) {
      failedAttempts = 0;
      Blink(1);
    } else {
      failedAttempts++;  // Count how many error happens
    }
  } else if (client.connected() && (millis() - lastConnectionTime > postingInterval * 2)) {
    // Still connected, wait to retry or reboot
    while (client.connected()) {
      if (client.available() > 0) {
        char c = client.read();
        Sprint(c);
      }
    }
    client.stop();
    failedAttempts++;
  }

  ListenWebServer();

  // Check error and Reboot
  if (failedAttempts >= retryBeforeReboot) {
    Reset_AVR();
  }

#if DEBUG == 1
  Sprintln("stop running for debug");
  while (true)
    ;
#endif
}  // loop

// Return brightness percentage
float get_light_level() {
  int operatingVoltage = analogRead(REFERENCE_3V3);
  int lightSensor = analogRead(LIGHT);

  // Convert in %
  lightSensor = map(lightSensor, 0, operatingVoltage, 0, 100);

  return (lightSensor);
}  // get_light_level

// Returns the instataneous wind speed
float get_wind_speed() {
  float deltaTime = millis() - lastWindCheck;

  deltaTime /= 1000.0;  // Covert to seconds

  float windSpeed = (float)windClicks / deltaTime;

  windClicks = 0;  // Reset and start watching for new wind
  lastWindCheck = millis();

  windSpeed *= 1.3;  // Calibrated with anemometer

  Sprintln();
  Sprint("Windspeed:");
  Sprintln(windSpeed);

  return (windSpeed);
}  // get_wind_speed

// Read the wind direction sensor, return heading in degrees
unsigned int get_wind_direction() {
  unsigned int adc;

  adc = analogRead(WDIR);  // get the current reading from the sensor

  // The following table is ADC readings for the wind direction sensor output, sorted from low to high.
  // Each threshold is the midpoint between adjacent headings. The output is degrees for that ADC reading.
  // Note that these are not in compass degree order! See Weather Meters datasheet for more information.

  if (adc < 380)
    return (113);
  if (adc < 393)
    return (68);
  // if (adc < 414) return (90);
  if (adc < 456)
    return (158);
  if (adc < 508)
    return (135);
  if (adc < 551)
    return (203);
  if (adc < 615)
    return (180);
  if (adc < 680)
    return (23);
  if (adc < 746)
    return (45);
  if (adc < 801)
    return (248);
  if (adc < 833)
    return (225);
  if (adc < 878)
    return (338);
  if (adc < 913)
    return (0);
  if (adc < 940)
    return (293);
  if (adc < 967)
    return (315);
  if (adc < 990)
    return (270);
  if (adc < 1024)
    return (90);
  return (620);  // error, disconnected?
}  // get_wind_direction

bool SendData() {
  bool result = false;
  if (config.ip1 == 0 || config.ip2 == 0 || config.ip3 == 0 || config.ip4 == 0) {
    BlinkError(F("EmonCMS server IP not configured"), 3);
  } else if (config.port == 0) {
    BlinkError(F("EmonCMS server Port not configured"), 3);
  } else if (config.node == 0) {
    BlinkError(F("Emon node not configured"), 3);
  } else if (config.apikey[0] == 0) {
    BlinkError(F("API key not configured"), 3);
  } else if (client.connect(IPAddress(config.ip1, config.ip2, config.ip3, config.ip4), config.port)) {
    // if there's a successful connection:
    Sprintln(F("Connecting..."));
    // send the HTTP GET request:
    Cprint("GET /emoncms/input/post.json?apikey=");
    Cprint(config.apikey);

    if (config.node != 0) {
      Cprint("&node=");
      Cprint(config.node);
    }

    Cprint("&fulljson={");

    unsigned int winddir = get_wind_direction();
    Cprint("\"winddir\":");
    Cprint(winddir);  // [0-360 instantaneous wind direction]

    float windspeedkph = get_wind_speed();
    if (windspeedkph > 0) {
      Cprint(",\"windspeedkph\":");
      CPrint(windspeedkph, 1);  // [kph instantaneous wind speed]
    }

    unsigned int rainMM = (rain * 1000);
    rain = 0;  // set zero to restart to count
    Cprint(",\"rainin\":");
    Cprint(rainMM);  // rain mm over the past 10 sec

#if HUMIDITY == 1
    // Calc humidity
    float humidity = myHumidity.getRH();
    if (humidity > 0 && humidity < 120) {
      Cprint(",\"humidity\":");
      CPrint(humidity, 1);  // [%]
    } else if (humidity == 998) {
      // Try re-initializing the I2C comm and the sensors
      initPressure();
    }

    // Calc tempC from humidity sensor
    float tempHuC = myHumidity.getTemp();
    if (tempHuC > -100 && tempHuC < 100) {
      Cprint(",\"tempHuC\":");
      CPrint(tempHuC, 1);  // [temperature C]
    }
#endif

#if PRESSURE == 1
    // Calc altitude
    myPressure.setModeActive();
    myPressure.setModeAltimeter();
    float altitudeM = myPressure.readAltitude();
    if (altitudeM > 0 && altitudeM < 9000) {
      Cprint(",\"altitude\":");
      CPrint(altitudeM, 1);  // [meter]
    } else {
      Sprintln();
      Sprint("Altimeter wrong: ");
      Sprintln(altitudeM);
    }

    // Calc tempC from pressure sensor
    myPressure.setModeBarometer();
    float tempPrC = myPressure.readTemp();
    if (tempPrC > -100 && tempPrC < 100) {
      Cprint(",\"tempPrC\":");
      CPrint(tempPrC, 1);  // [temperature C]
    } else {
      Sprintln();
      Sprint("Temperature of pressure sensor wrong: ");
      Sprintln(tempPrC);
    }

    // Calc pressure
    float pressure = myPressure.readPressure();
    if (pressure > 80000 && pressure < 120000) {
      Cprint(",\"pressure\":");
      CPrint(pressure, 2);
    }
    myPressure.setModeStandby();
#endif

    // Calc light level
    float light_lvl = get_light_level();
    Cprint(",\"light_lvl\":");
    CPrint(light_lvl, 2);  //[%]

    Cprintln("} HTTP/1.1");
    Cprintln("Host:emoncms.org");
    Cprintln("User-Agent: Arduino-ethernet");
    Cprintln("Connection: close");
    Cprintln();
    client.flush();

    Sprintln(F("Data sends"));

    // if there's incoming data from the net connection. Send it out the serial port.
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Sprint(c);
      }
    }

    // if there's no net connection through the loop, then stop the client:
    if (!client.connected()) {
      Sprintln();
      Sprintln(F("Disconnecting..."));
      client.stop();
    }

    result = true;
  } else {
    // if you couldn't make a connection:
    BlinkError(F("Connection failed to server"), 4);
  }
  return result;
}  // SendData
