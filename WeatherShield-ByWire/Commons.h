#include <avr/wdt.h>

#define TEST 0      //1 = Use Serial
#define DEBUG 0     //1 = Only 1 loop

#define PRESSURE 1  //1 = Use pressure sensor
#define HUMIDITY 1  //1 = Use humidity sensor

#define Reset_AVR() \
  wdt_enable(WDTO_30MS); \
  while (1) {}

#if TEST == 1
#define Sprintln(a) Serial.println(a)
#define Sprint(a) Serial.print(a)
#define SPrint(a, b) Serial.print(a, b)
#define Cprintln(a) \
  Serial.println(a); \
  client.println(a)
#define Cprint(a) \
  Serial.print(a); \
  client.print(a)
#define CPrint(a, b) \
  Serial.print(a, b); \
  client.print(a, b)
#define Wprintln(a) \
  Serial.println(a); \
  server.println(a)
#define Wprint(a) \
  Serial.print(a); \
  server.print(a)
#else
#define Sprintln(a)
#define Sprint(a)
#define SPrint(a, b)
#define Cprintln(a) client.println(a)
#define Cprint(a) client.print(a)
#define CPrint(a, b) client.print(a, b)
#define Wprintln(a) server.println(a)
#define Wprint(a) server.print(a)
#endif

/* SparkFun Weather Shield */
const PROGMEM byte STAT1 = 7;  //Status LED Blue
const PROGMEM byte STAT2 = 8;  //Status LED Green

void Blink(byte errorNumber) {
  for (int index = 0; index < errorNumber; index++) {
    digitalWrite(STAT2, HIGH);
    delay(500);
    digitalWrite(STAT2, LOW);
    delay(500);
  }
}  //Blink