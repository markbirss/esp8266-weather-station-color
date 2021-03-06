// Reads a screen image off the TFT and send it to a processing client sketch
// over the serial port. Use a high baud rate, e.g. for an ESP8266:
// Serial.begin(921600);

// At 921600 baud a 320 x 240 image with 16 bit colour transfers can be sent to the
// PC client in ~1.67s and 24 bit colour in ~2.5s which is close to the theoretical
// minimum transfer time.

// This sketch has been created to work with the TFT_eSPI library here:
// https://github.com/Bodmer/TFT_eSPI

// Created by: Bodmer 27/1/17
// Updated by: Bodmer 10/3/17
// Version: 0.06

// MIT licence applies, all text above must be included in derivative works


#define BAUD_RATE 250000      // Maximum Serial Monitor rate for other messages
#define DUMP_BAUD_RATE 921600 // Rate used for screen dumps

#define PIXEL_TIMEOUT 100     // 100ms Time-out between pixel requests
#define START_TIMEOUT 10000   // 10s Maximum time to wait at start transfer

#define BITS_PER_PIXEL 16     // 24 for RGB colour format, 16 for 565 colour format

// Number of pixels to send in a burst (minimum of 1), no benefit above 8
// NPIXELS values and render times: 1 = 5.0s, 2 = 1.75s, 4 = 1.68s, 8 = 1.67s
#define NPIXELS 8  // Must be integer division of both TFT width and TFT height


// Start a screen dump server (serial or network)
boolean screenServer(void)
{
  Serial.end();                 // Stop the serial port (clears buffers too)
  Serial.begin(DUMP_BAUD_RATE); // Force baud rate to be high
  delay(0); // Equivalent to yield() for ESP8266;

  boolean result = serialScreenServer(); // Screenshot serial port server
  //boolean result = wifiScreenServer();   // Screenshot WiFi UDP port server (WIP)

  Serial.end();                 // Stop the serial port (clears buffers too)
#ifdef SERIAL_MESSAGES
  Serial.begin(BAUD_RATE);      // Return baud rate to normal
#endif
  delay(0); // Equivalent to yield() for ESP8266;

  //Serial.println();
  //if (result) Serial.println(F("Screen dump passed :-)"));
  //else        Serial.println(F("Screen dump failed :-("));

  return result;
}

// Screenshot serial port server (Processing sketch acts as client)
boolean serialScreenServer(void)
{

  // Precautionary receive buffer garbage flush for 50ms
  uint32_t clearTime = millis() + 50;
  while ( millis() < clearTime && Serial.read() >= 0) delay(0); // Equivalent to yield() for ESP8266;

  boolean wait = true;
  uint32_t lastCmdTime = millis();     // Initialise start of command time-out

  // Wait for the starting flag with a start time-out
  while (wait)
  {
    delay(0); // Equivalent to yield() for ESP8266;
    // Check serial buffer
    if (Serial.available() > 0) {
      // Read the command byte
      uint8_t cmd = Serial.read();
      // If it is 'S' (start command) then clear the serial buffer for 100ms and stop waiting
      if ( cmd == 'S' ) {
        // Precautionary receive buffer garbage flush for 50ms
        clearTime = millis() + 50;
        while ( millis() < clearTime && Serial.read() >= 0) delay(0); // Equivalent to yield() for ESP8266;

        wait = false;           // No need to wait anymore
        lastCmdTime = millis(); // Set last received command time

        // Send screen size using a simple header with delimiters for client checks
        Serial.write('W');
        Serial.write(tft.width()  >> 8);
        Serial.write(tft.width()  & 0xFF);
        Serial.write('H');
        Serial.write(tft.height() >> 8);
        Serial.write(tft.height() & 0xFF);
        Serial.write('Y');
        Serial.write(BITS_PER_PIXEL);
        Serial.write('?');
      }
    }
    else
    {
      // Check for time-out
      if ( millis() > lastCmdTime + START_TIMEOUT) return false;
    }
  }

  uint8_t color[3 * NPIXELS]; // RGB and 565 format color buffer for N pixels

  // Send all the pixels on the whole screen
  for ( uint32_t y = 0; y < tft.height(); y++)
  {
    // Increment x by NPIXELS as we send NPIXELS for every byte received
    for ( uint32_t x = 0; x < tft.width(); x += NPIXELS)
    {
      delay(0); // Equivalent to yield() for ESP8266;

      // Wait here for serial data to arrive or a time-out elapses
      while ( Serial.available() == 0 )
      {
        if ( millis() > lastCmdTime + PIXEL_TIMEOUT) return false;
        delay(0); // Equivalent to yield() for ESP8266;
      }

      // Serial data must be available to get here, read 1 byte and
      // respond with N pixels, i.e. N x 3 RGB bytes or N x 2 565 format bytes
      if ( Serial.read() == 'X' ) {
        // X command byte means abort, so clear the buffer and return
        clearTime = millis() + 50;
        while ( millis() < clearTime && Serial.read() >= 0) delay(0); // Equivalent to yield() for ESP8266;
        return false;
      }
      // Save arrival time of the read command (for later time-out check)
      lastCmdTime = millis();

#if defined BITS_PER_PIXEL && BITS_PER_PIXEL >= 24
      // Fetch N RGB pixels from x,y and put in buffer
      tft.readRectRGB(x, y, NPIXELS, 1, color);
      // Send buffer to client
      Serial.write(color, 3 * NPIXELS); // Write all pixels in the buffer
#else
      // Fetch N 565 format pixels from x,y and put in buffer
      tft.readRect(x, y, NPIXELS, 1, (uint16_t *)color);
      // Send buffer to client
      Serial.write(color, 2 * NPIXELS); // Write all pixels in the buffer
#endif
    }
  }
  
  Serial.flush(); // Make sure all pixel bytes have been despatched

  return true;
}

