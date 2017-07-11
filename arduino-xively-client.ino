/*

  arduino-xively-client.ino

  Author: Pete Milne
  Date: 11-07-2017
  Version: 0.4

  Xively Client Finite State Machine
  Sends DS18S20 temperature readings to Xively API using
  Ethernet Shield. Displays status on Green & Red LED's
  Displays first 2 readings on Arduino TFT display

  Based on code from http://www.arduino.cc
  by David A. Mellis, Tom Igoe, Adrian McEwen

*/

#include <SPI.h>
#include <Ethernet.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TFT.h>  // Arduino LCD library
#include "client-conf.h"

// Counters
unsigned long successes = 0;  // Number of successful connections
unsigned long failures = 0;   // Number of failed connections
boolean alertFlag = false;    // Indicates failed connection

// LED pins
#define HEARTBEAT_LED 2 // Heartbeat LED pin 2
#define WARNING_LED 5    // Warning LED pin 5
#define RESET 8     // Ethernet reset pin 8

// System states
#define STATE_IDLE 0
#define STATE_RECEIVING 1
#define STATE_SAMPLING 2
#define STATE_CONNECTING 3
#define STATE_FAILING 4

// System events
#define EVENT_IDLE 0
#define EVENT_RECEIVE 1
#define EVENT_SAMPLE 2
#define EVENT_CONNECT 3
#define EVENT_DISCONNECT 4
#define EVENT_FAIL 5

// Dallas OneWire config
#define ONEWIRE_BUS 7   // Pin for bus 
#define NUM_DEVICES 2   // Number of devices on bus
#define ADDRESS_SIZE 8  // 8 byte device address
#define TEMPERATURE_PRECISION 9 // device precision

OneWire  oneWire(ONEWIRE_BUS);  // (a 4.7K resistor is necessary)
DallasTemperature sensors(&oneWire);
DeviceAddress DeviceAddresses[NUM_DEVICES]; // Buffer for each device address
int16_t tempsBuf[NUM_DEVICES]; // Buffer for each temperature reading

// Ethernet config
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // MAC address for ethernet controller
EthernetClient client; // initialize instance

// If you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
// IPAddress server(216,52,233,122);
char server[] = "api.xively.com";   // name address for xively API

// TFT display config
#define dc   9
#define cs   6
#define rst  3

TFT TFTscreen = TFT(cs, dc, rst);

// Declare reset function @ address 0
void(* resetFunc) (void) = 0;

/*******************************************************************/
/* Runs once to initialise sensors and Ethernet shield             */
/*******************************************************************/
void setup() {

  // Setup I/O pins
  pinMode(HEARTBEAT_LED, OUTPUT);
  pinMode(WARNING_LED, OUTPUT);
  pinMode(RESET, OUTPUT);

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  Serial.println("Starting...");

  // Initialise OneWire bus
  sensors.begin();
  oneWire.reset_search();
  for (int i = 0; i < NUM_DEVICES ; i++ ) {
    if (oneWire.search(DeviceAddresses[i])) { // Load each address into array
      printAddress(DeviceAddresses[i]); // Print address from array
      Serial.println();
    }
  }
  Serial.println("No more addresses.");

  for (int i = 0; i < NUM_DEVICES ; i++ ) {
    sensors.setResolution(DeviceAddresses[i], TEMPERATURE_PRECISION);
    Serial.print("Device 0 Resolution: ");
    Serial.print(sensors.getResolution(DeviceAddresses[i]), DEC);
    Serial.println();
  }

  /*
    // Initialise Ethernet Shield (DHCP)

    // Forced a hardware reset for some Ethernet Shields that
    // don't initialise correctly on power up.
    // Fixed by bending shield reset pin out of header and
    // connecting to RESET pin on Arduino
    digitalWrite(RESET, LOW); // Take reset line low
    delay(200);
    digitalWrite(RESET, HIGH); // Take reset line high again
    // End of fix

    delay(2000);  // Allow Shield time to boot
    if (Ethernet.begin(mac) == 0) {
      Serial.println("Failed to configure Ethernet using DHCP");
      // DHCP failed, keep resetting Arduino
      Serial.println("resetting");
      delay(1000);
      resetFunc();  //call reset
    }
  */
  // Initialise TFT display
  TFTscreen.begin();
  // Write static text to TFT
  // clear the screen with a black background
  TFTscreen.background(0, 0, 0);
  // set the font color to white
  TFTscreen.stroke(255, 255, 255);
  // set the font size
  TFTscreen.setTextSize(2);
  // write the text to the top left corner of the screen
  TFTscreen.text("Inside C:\n ", 0, 0);
  TFTscreen.text("Outside C:\n ", 0, 60);
  // set the font size very large for the loop
  TFTscreen.setTextSize(3);

}


/*******************************************************************/
/* Runs FSM and displays system state                              */
/*******************************************************************/
void loop() {
  check_state();
  show_state();
  Serial.println();
}

/*******************************************************************/
/* FINITE STATE MACHINE                                            */
/* Checks current state/event and determines next state transition */
/* and corresponding action. Actions trigger next event and so on. */
/* System waits a set interval before starting each cycle by       */
/* triggering SAMPLE events. If buffered response from Xively is   */
/* available RECEIVE events occurs and data is read. Connection    */
/* errors trigger FAIL events which trigger an alarm before        */
/* cleaning-up and returning to IDLE state to recover.             */
/*******************************************************************/
void check_state() {
  // Initial state / event
  static byte state = STATE_IDLE;
  static byte event = EVENT_IDLE;
  switch ( state )  {
    case STATE_IDLE:
      Serial.println("IDLE STATE...");
      switch ( event ) {

        case EVENT_IDLE:
          Serial.println("IDLE EVENT...");
          state = STATE_IDLE;
          event = action_idle();
          break;

        case EVENT_SAMPLE:
          Serial.println("SAMPLE EVENT...");
          state = STATE_SAMPLING;
          event = action_sample();
          break;

        case EVENT_RECEIVE:
          Serial.println("RECEIVE EVENT...");
          state = STATE_RECEIVING;
          event = action_receive();
          break;
      }
      break; // End IDLE state

    case STATE_SAMPLING:
      Serial.println("SAMPLING STATE...");
      switch ( event ) {
        case EVENT_CONNECT:
          Serial.println("CONNECT EVENT...");
          state = STATE_CONNECTING;
          event = action_connect();
          break;

        case EVENT_IDLE:
          Serial.println("IDLE EVENT...");
          state = STATE_IDLE;
          event = action_idle();
          break;
      }
      break; // End SAMPLING state

    case STATE_CONNECTING:
      Serial.println("CONNECTING STATE...");
      switch ( event ) {

        case EVENT_DISCONNECT:
          Serial.println("DISCONNECT EVENT...");
          state = STATE_IDLE;
          event = action_disconnect();
          break;

        case EVENT_FAIL:
          Serial.println("FAIL EVENT...");
          state = STATE_FAILING;
          event = action_fail();
          break;
      }
      break; // End CONNECTING state

    case STATE_FAILING:
      Serial.println("FAILING STATE...");
      switch ( event ) {
        case EVENT_DISCONNECT:
          Serial.println("DISCONNECT EVENT...");
          state = STATE_IDLE;
          event = action_disconnect();
          break;
      }
      break; // End FAILING state

    case STATE_RECEIVING:
      Serial.println("RECEIVING STATE...");
      switch ( event ) {
        case EVENT_DISCONNECT:
          Serial.println("DISCONNECT EVENT...");
          state = STATE_IDLE;
          event = action_disconnect();
          break;
      }
      break;  // End RECEIVING state

  } // End outer switch
} // End check_state

/*******************************************************************/
/* Set LED's according to alert status                             */
/*******************************************************************/
void show_state() {
  static int ledState = HIGH;
  static unsigned long previousLedMillis = 0;
  unsigned long currentLedMillis = millis();

  // Check for millis overflow and reset
  if (currentLedMillis < previousLedMillis) previousLedMillis = 0;

  if (currentLedMillis - previousLedMillis >= 1000 ) {
    // save the last time you blinked the LED
    previousLedMillis = currentLedMillis;

    // if the LED is off turn it on and vice-versa:
    if (ledState == LOW)
      ledState = HIGH;
    else
      ledState = LOW;

    if (!alertFlag) {
      // set the LED with the ledState of the variable:
      digitalWrite(HEARTBEAT_LED, ledState);
      digitalWrite(WARNING_LED, LOW);
    }
    else {
      digitalWrite(HEARTBEAT_LED, LOW);
      digitalWrite(WARNING_LED, ledState);
    }
  }
}

