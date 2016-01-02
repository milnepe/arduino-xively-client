/*

 arduino-xively-client-actions.ino

 Author: Pete Milne
 Date: 02-01-2016
 Version: 0.2

 Xively Client Finite State Machine Actions

*/

/*******************************************************************/
/* IDLE ACTION                                                     */
/* Fires IDLE events by default                                    */
/* RECEIVE events if buffer has received response from Xively      */
/* SAMPLE events if time interval has elapsed                      */
/*******************************************************************/
byte action_idle() {
  Serial.println("IDLE ACTION...");

  static unsigned long previousMillis = 0;
  unsigned long currentMillis = millis();

  byte event = EVENT_IDLE; // Default

  // Check for millis overflow and reset
  if (currentMillis < previousMillis) previousMillis = 0;

  if (client.available()) {
    event = EVENT_RECEIVE;
  } else {
    // Delay for interval without blocking
    if (currentMillis - previousMillis >= UPDATE_INTERVAL) {
      previousMillis = currentMillis;  // Update comparison
      event = EVENT_SAMPLE;
    }
  }
  return event;
}

/*******************************************************************/
/* RECEIVE ACTION                                                  */
/* Always fires DISCONNECT events                                  */
/* Reads buffer until empty - outputs response for debugging       */
/*******************************************************************/
byte action_receive() {
  Serial.println("RECEIVE ACTION...");

  byte event = EVENT_DISCONNECT; // Default

  // output any response
  while (client.available()) {
    char c = client.read();
    Serial.print(c);
  }
  return event;
}

/*******************************************************************/
/* SAMPLE  ACTION                                                  */
/* Always fires CONNECT events - All sensor sampling happens here  */
/*******************************************************************/
byte action_sample() {
  Serial.println("SAMPLE ACTION...");
  byte event = EVENT_CONNECT; // Default

  /* Test - simulate max & min readings
  tempsBuf[0] = 50;
  tempsBuf[1] = -30;
  */

  /* Sample temerature sensors */
  for (int i = 0; i < NUM_DEVICES; i++) {
    tempsBuf[i] = (getCelsius(deviceAddress[i]));
  }
  return event;
}

/*******************************************************************/
/* CONNECT ACTION                                                  */
/* Fires FAIL events by default                                    */
/* DISCONNECT events if connection is established                  */
/* PUT request sends sensor readings to Xively, updates success    */
/* and resets any alerts                                           */
/*******************************************************************/
byte action_connect()
{
  Serial.println("CONNECT ACTION...");

  byte event = EVENT_FAIL; // Default

  if (client.connect(server, 80) == 1) { // new connect method
    client.print("PUT /v2/feeds/"); // Construct PUT request
    client.print(FEEDID);
    client.println(".csv HTTP/1.1"); // Use csv format
    client.println("Host: api.xively.com");
    client.print("X-ApiKey: ");
    client.println(APIKEY);
    client.print("User-Agent: ");
    client.println(USERAGENT);
    client.print("Content-Length: ");

    // calculate number of characters to send
    int dataLength = 0;
    // calculate number of digits for each temp reading
    for (int i = 0; i < NUM_DEVICES; i++) {
      dataLength = dataLength + getDigits(tempsBuf[i]);
    }
    // add 2 bytes for channel number (eg. "0,") and 2 bytes for carriage returns, per channel
    dataLength = dataLength + (4 * NUM_DEVICES);
    dataLength = dataLength + 4 + getDigits(successes); // length of successes
    dataLength = dataLength + 4 + getDigits(failures); // length of failures
    client.println(dataLength);
    Serial.println(dataLength);

    // last pieces of the HTTP PUT request:
    client.println("Content-Type: text/csv");
    client.println("Connection: close");
    client.println();

    // here's the actual content of the PUT request
    for (int i = 0; i < NUM_DEVICES; i++) {
      client.print(i);
      client.print(",");
      client.println(tempsBuf[i]);
      Serial.println(tempsBuf[i]);
    }

    // here's the successes
    client.print(NUM_DEVICES);
    client.print(",");
    client.println(successes++);// Update success counter
    Serial.println(successes);

    // here's the faiures
    client.print(NUM_DEVICES + 1);
    client.print(",");
    client.println(failures);
    Serial.println(failures);

    alert = false; // End an alert
    event = EVENT_DISCONNECT;
  }
  return event;
}

/*******************************************************************/
/* FAIL ACTION                                                     */
/* Always fires DISCONNECT events, updates failure counter and     */
/* triggers an alert                                               */
/*******************************************************************/
byte action_fail() {
  Serial.println("FAIL ACTION...");

  byte event = EVENT_DISCONNECT; // Default

  alert = true; // Start an alert
  failures++; // Update failure counter
  return event;
}

/*******************************************************************/
/* DISCONNECT ACTION                                               */
/* Always fires IDLE events                                        */
/* Disconnects client when connection closes on completion of http */
/* response                                                        */
/* Note - client is considered connected if connection has been    */
/* closed but there is still unread data in buffer.                */
/*******************************************************************/
byte action_disconnect() {
  Serial.println("DISCONNECT ACTION...");

  byte event = EVENT_IDLE;

  if (!client.connected()) {
    client.stop();
    Serial.println("Disconnected...");
  } else Serial.println("Still connected...");
  return event;
}

/*--------------------------------------------------------------------*/
/* Returns number of digits incl minus sign for int n in range        */
/* INT_MIN <= n <= INT_MAX defined in <limits.h>                      */
/*--------------------------------------------------------------------*/
byte getDigits(int n) {
  // there's at least one digit
  byte digits = 1;
  int dividend = abs(n) / 10;
  // continually divide the value by ten,
  // adding one to the digit count for each
  // time you divide, until you're at 0
  while (dividend > 0) {
    dividend = dividend / 10;
    digits++;
  }
  // for negative numbers add 1 for minus sign
  if (n < 0)
    return ++digits;
  else
    return digits;
}

/*******************************************************************/
/* Print Onewire device address - for debugging                    */
/*******************************************************************/
void printAddress(uint8_t *address) {
  for ( int i = 0; i < ADDRESS_SIZE; i++) {
    Serial.print(address[i], HEX);
  }
}

/*******************************************************************/
/* Return sensor reading in celcius as 16 bit signed int using     */
/* device address                                                  */
/*******************************************************************/
int getCelsius(uint8_t *address) {
  uint8_t data[12];

  ds.reset();
  ds.select(address);
  ds.write(0x44, 1); // start conversion, using parasite power

  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.

  ds.reset();
  ds.select(address);
  ds.write(0xBE);         // Read Scratchpad

  for (int i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
    //Serial.print(data[i], HEX);
  }
  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  raw = raw << 3; // 9 bit resolution default
  //raw = (raw & 0xFFF0) + 12 - data[6]; // full 12 bit resolution
  //return (float)raw / 16.0;
  return raw / 16;
}


