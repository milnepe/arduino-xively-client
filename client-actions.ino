/*

  client-actions.ino

  Author: Pete Milne
  Date: 11-07-2017
  Version: 0.4

  Xively Client Finite State Machine Actions

*/

/*******************************************************************/
/* IDLE ACTION                                                     */
/* Transition to IDLE state by default                             */
/* Transition to RECEIVING state if buffer received Xively response*/
/* Transition to SAMPLING if sampling interval has elapsed         */
/*******************************************************************/
byte action_idle() {
  DEBUG_PRINT("IDLE ACTION...");

  // Transition to IDLE
  byte event = EVENT_IDLE;

  // Get millis
  unsigned long currentMillis = millis();

  // Transition to RECEIVING
  if (client.available()) {
    event = EVENT_RECEIVE;
  }

  // Transition to CONNECTING
  static unsigned long previousConnectingMillis = 0;

  // Check for millis overflow and reset
  if (currentMillis < previousConnectingMillis) previousConnectingMillis = 0;

  // Delay for interval without blocking
  if (currentMillis - previousConnectingMillis >= CONNECT_INTERVAL) {
    previousConnectingMillis = currentMillis;  // Update comparison
    event = EVENT_CONNECT;
  }

  // Transition to SAMPLING
  static unsigned long previousSamplingMillis = 0;

  // Check for millis overflow and reset
  if (currentMillis < previousSamplingMillis) previousSamplingMillis = 0;

  // Delay for interval without blocking
  if (currentMillis - previousSamplingMillis >= SAMPLE_INTERVAL) {
    previousSamplingMillis = currentMillis;  // Update comparison
    event = EVENT_SAMPLE;
  }

  return event;
}

/*******************************************************************/
/* RECEIVE ACTION                                                  */
/* Always fires DISCONNECT events                                  */
/* Reads buffer until empty - outputs response for debugging       */
/*******************************************************************/
byte action_receive() {
  DEBUG_PRINT("RECEIVE ACTION...");

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
  DEBUG_PRINT("SAMPLE ACTION...");

  byte event = EVENT_IDLE;    // Testing

  // Sample sensors
  sampleSensors();

  // Update display
  updateDisplay(tempsBuf);

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
  DEBUG_PRINT("CONNECT ACTION...");

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

    alertFlag = false; // End an alert
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
  DEBUG_PRINT("FAIL ACTION...");

  byte event = EVENT_DISCONNECT; // Default

  alertFlag = true; // Start an alert
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
  DEBUG_PRINT("DISCONNECT ACTION...");

  byte event = EVENT_IDLE;

  if (!client.connected()) {
    client.stop();
    Serial.println("Disconnected...");
  } else Serial.println("Still connected...");
  return event;
}

/**********************************************************************/
/* Returns number of digits incl minus sign for int n in range        */
/* INT_MIN <= n <= INT_MAX defined in <limits.h>                      */
/**********************************************************************/
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
/* Sample sensors                                                  */
/*******************************************************************/
void sampleSensors() {
  sensors.requestTemperatures();

  for (int i = 0; i < NUM_DEVICES; i++) {
    tempsBuf[i] = (getCelsius(deviceAddress[i]));
#ifdef DEBUG_ONEWIRE
    printAddress(deviceAddress[i]);
    Serial.print(" Celsius: ");
    Serial.println(tempsBuf[i]);
#endif
  }
}

/*******************************************************************/
/* Return sensor reading in celcius as 16 bit signed int using     */
/* device address                                                  */
/*******************************************************************/
int16_t getCelsius(DeviceAddress device)
{
  int16_t raw = sensors.getTemp(device);
  return raw / 128;
}

/*******************************************************************/
/* Update TFT display using values passed as pointer               */
/* Dynamic text must first be cleared by writing over previous     */
/* value in black, then new values can be written                  */
/*******************************************************************/
void updateDisplay(int *value) {
  // TFT output buffers
  static char previousInside[4], previousOutside[4];
  char insidePrintout[4], outsidePrintout[4];

  // Converts ints to char arrays for use by TFT
  String inside = String(value[0]);
  inside.toCharArray(insidePrintout, 4);
  String outside = String(value[1]);
  outside.toCharArray(outsidePrintout, 4);

  // Clear screen by erasing writing out previous text
  TFTscreen.stroke(0, 0, 0);
  TFTscreen.text(previousInside, 0, 20);
  TFTscreen.text(previousOutside, 0, 80);

  // set the font color
  tempColour(value[0], 18, 16);
  // Write out new values
  TFTscreen.text(insidePrintout, 0, 20);
  tempColour(value[1], 12, 4);
  TFTscreen.text(outsidePrintout, 0, 80);

  // Save text for next time
  for (int i = 0; i < 4; i++) {
    previousInside[i] = insidePrintout[i];
    previousOutside[i] = outsidePrintout[i];
  }
}

/*******************************************************************/
/* Helper to set RGB font colour according to normal, medium or    */
/* low inputs                                                      */
/*******************************************************************/
void tempColour(int normal, int medium, int low) {
  TFTscreen.stroke(125, 250, 5);
  if (normal < medium)   TFTscreen.stroke(250, 164, 5);
  if (normal < low)   TFTscreen.stroke(255, 0, 0);
}


