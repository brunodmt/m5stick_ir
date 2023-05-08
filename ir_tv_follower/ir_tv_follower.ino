
#include <M5StickCPlus.h>
#include <EEPROM.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>

// EEPROM size (we only need 8 bytes)
const size_t kEepromSize = 8;
// Address to store the IR code, const as we only need one value
const int kIrCodeAddr = 0;

// USB treshsold to trigger a code and turn off the device
const float kUsbVTresh = 1;

// Default delay to use in most cases
const uint32_t kDelayMs = 100;
// IR Receiver Pin
const uint16_t kRecvPin = 33;
// IR Emitter Pin
const uint16_t kSendPin = 9;

// IRemote classes for sending and receiving
IRrecv irrecv(kRecvPin);
IRsend irsend(kSendPin);

// "Saved" code to send
uint64_t ir_code;
// Last received code (uncofirmed)
uint64_t temp_code;
// Result struct to receive decoding from IRremote
decode_results ir_results;

// Current USB Voltage
float usb_voltage;

// Programming mode to register a new code
void program_mode() {
  // Turn on the screen
  M5.Axp.SetLDO2(true);
  // Clear the screen and add some instructions
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextDatum(MC_DATUM);
  M5.Lcd.drawString("Press", 68, 75, 4);
  M5.Lcd.drawString("remote", 68, 105, 4);
  M5.Lcd.drawString("button", 68, 135, 4);
  M5.Lcd.drawString("to clone", 68, 165, 4);

  // Loop until a new code is confirmed (or process is cancelled)
  while (true) {
    // Check if a code is available (and it's a hold)
    if (irrecv.decode(&ir_results) && !ir_results.repeat) {
      // Keep for later
      temp_code = ir_results.value;

      // Display it on the screen with instructions
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setTextDatum(MC_DATUM);
      M5.Lcd.drawString("Received", 68, 20, 4);
      M5.Lcd.drawString(uint64ToString(ir_results.value, HEX), 68, 50, 4);

      M5.Lcd.setTextDatum(TR_DATUM);
      M5.Lcd.drawString("cancel", 130, 105, 2);

      M5.Lcd.setTextDatum(BC_DATUM);
      M5.Lcd.drawString("SAVE", 68, 240, 4);

      // Start listening for the next code
      irrecv.resume();
    }

    // Read the buttons state
    M5.update();

    // If "A" was pressed
    if (M5.BtnA.wasReleased()) {
      // Save the code
      ir_code = temp_code;
      // Write code to EEPROM
      EEPROM.writeULong64(kIrCodeAddr, ir_code);
      // And commit the changes
      EEPROM.commit();
      // Confirm on screen
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setTextDatum(MC_DATUM);
      M5.Lcd.drawString("SAVED!", 68, 120, 4);
      delay(10 * kDelayMs);
      // And get out of program mode
      break;
    // If "B" was pressed
    } else if (M5.BtnB.wasReleased()) {
      // Exit program mode without updating the code
      break;
    }

    delay(kDelayMs);
  }

  // Clear and turn off the screen
  M5.Lcd.fillScreen(BLACK);
  M5.Axp.SetLDO2(false);
}

void setup() {
  // Initialize with LCD and Power, but no Serial
  M5.begin(true, true, false);
  // Open the EEPROM
  EEPROM.begin(kEepromSize);
  // Turn off the screen (just the backlight...)
  M5.Axp.SetLDO2(false);
  // Initialize the IR sender
  irsend.begin();
  // Enable IR receiver (it would be better to keep it enabled only during programming, but there
  // seems to be an issue managing the interruption so doing it here for now instead)
  irrecv.enableIRIn();
  // Read the IR code from EEPROM
  ir_code = EEPROM.readULong64(kIrCodeAddr);
  // Send a code when turning on, as this will be triggered by the TV turning on
  irsend.sendNEC(ir_code);
}

void loop() {
  // Update the buttons state
  M5.update();

  // If "A" was pressed, send the saved code
  if (M5.BtnA.wasReleased()) {
    irsend.sendNEC(ir_code);
  // If "B" was pressed, enter program mode
  } else if (M5.BtnB.wasReleased()) {
    program_mode();
  }

  // Get updated USB voltage
  usb_voltage = M5.Axp.GetVBusVoltage();
  // If it went below the treshold
  if (usb_voltage < kUsbVTresh) {
    // Send a code to turn the speakers off
    irsend.sendNEC(ir_code);
    // and turn off itself
    M5.Axp.PowerOff();
  }

  delay(kDelayMs);
}
