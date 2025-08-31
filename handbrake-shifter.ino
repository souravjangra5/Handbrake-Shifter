// STM32F103C6/C8 (Blue Pill) — Roger Clark STM32duino core — 10 Buttons + 2 Analog Axes (Hall)
// Upload via ST-Link (recommended). USB on PA11/PA12. Buttons active-LOW to GND.

#include <Arduino.h>
#include <USBComposite.h>

// ---------------- HID REPORT: 10 buttons + 2 axes (each 0..4095 -> 16-bit fields) ----------------
static const uint8_t gamepadHIDReport[] = {
  0x05,0x01,       // Usage Page (Generic Desktop)
  0x09,0x05,       // Usage (Gamepad)
  0xA1,0x01,       // Collection (Application)

    // --- 10 Buttons (2 bytes: 10 bits + 6 pad) ---
    0x05,0x09,     //   Usage Page (Button)
    0x19,0x01,     //   Usage Minimum (Button 1)
    0x29,0x0A,     //   Usage Maximum (Button 10)
    0x15,0x00,     //   Logical Minimum (0)
    0x25,0x01,     //   Logical Maximum (1)
    0x95,0x0A,     //   Report Count (10)
    0x75,0x01,     //   Report Size (1)
    0x81,0x02,     //   Input (Data,Var,Abs)
    0x95,0x01,     //   Report Count (1)
    0x75,0x06,     //   Report Size (6)
    0x81,0x03,     //   Input (Const,Var,Abs) padding

    // --- Axis 1 (Z) 0..4095 ---
    0x05,0x01,     //   Usage Page (Generic Desktop)
    0x09,0x32,     //   Usage (Z)   // change to 0x30 for X if you prefer
    0x15,0x00,     //   Logical Min (0)
    0x26,0xFF,0x0F,//   Logical Max (4095)
    0x75,0x10,     //   Report Size (16)
    0x95,0x01,     //   Report Count (1)
    0x81,0x02,     //   Input (Data,Var,Abs)

    // --- Axis 2 (Rz) 0..4095 ---
    0x05,0x01,     //   Usage Page (Generic Desktop)
    0x09,0x35,     //   Usage (Rz)  // pick any axis usage you like
    0x15,0x00,     //   Logical Min (0)
    0x26,0xFF,0x0F,//   Logical Max (4095)
    0x75,0x10,     //   Report Size (16)
    0x95,0x01,     //   Report Count (1)
    0x81,0x02,     //   Input (Data,Var,Abs)

  0xC0
};

USBHID HID;
// 2 bytes buttons + 2 bytes axis1 + 2 bytes axis2 = 6 bytes total
HIDRaw<6,0> gamepad(HID);

// ---------------- PIN MAP ----------------
// Hall sensors on analog-capable pins (change if needed)
const uint8_t HALL1_PIN = PA1;  // Handbrake #1 (SS49E OUT)
const uint8_t HALL2_PIN = PA2;  // Handbrake #2 (SS49E OUT)

// 10 button pins (active-LOW). Use non-ADC pins so your halls stay free.
const uint8_t BTN_PINS[10] = {
  PA3, PA4, PA5, PA6, PA7, PB0, PB1, PA8, PB10, PB11
};

// Invert axes if your mechanics are reversed
const bool INVERT_AXIS1 = false;
const bool INVERT_AXIS2 = false;

// Optional smoothing (IIR). 0=off, 1..4 stronger filtering.
const uint8_t SMOOTH_SHIFT = 3; // alpha = 1/8

struct __attribute__((packed)) Report {
  uint16_t buttons;  // bit0..bit9 = Button1..Button10
  uint16_t axis1;    // 0..4095
  uint16_t axis2;    // 0..4095
} report;

static inline uint16_t readButtons() {
  uint16_t m = 0;
  for (uint8_t i = 0; i < 10; i++) if (digitalRead(BTN_PINS[i]) == LOW) m |= (1u << i);
  return m;
}

// simple IIR filters
uint32_t filt1 = 0, filt2 = 0;

static inline uint16_t readAxis12(uint8_t pin, uint32_t &accum, bool invert) {
  uint16_t v = analogRead(pin);        // 0..4095 (on STM32F1)
  if (invert) v = 4095 - v;
  if (SMOOTH_SHIFT) {                  // y += (x - y)/2^k
    accum += ((uint32_t)v - accum) >> SMOOTH_SHIFT;
    v = (uint16_t)accum;
  }
  return v;
}

void setup() {
  // Buttons
  for (uint8_t i = 0; i < 10; i++) pinMode(BTN_PINS[i], INPUT_PULLUP);

  // Halls (analog in) — add 0.1uF OUT→GND near each sensor for noise reduction
  pinMode(HALL1_PIN, INPUT_ANALOG);
  pinMode(HALL2_PIN, INPUT_ANALOG);
  delay(5);
  filt1 = analogRead(HALL1_PIN);
  filt2 = analogRead(HALL2_PIN);

  USBComposite.clear();
  USBComposite.setManufacturerString("RJT");
  USBComposite.setProductString("Shifter + Handbrake"); // device name

  HID.begin(gamepadHIDReport, sizeof(gamepadHIDReport));
  gamepad.begin();
  USBComposite.begin();
  delay(500); // allow USB enumeration
}

void loop() {
  report.buttons = readButtons();
  report.axis1   = readAxis12(HALL1_PIN, filt1, INVERT_AXIS1); // Z
  report.axis2   = readAxis12(HALL2_PIN, filt2, INVERT_AXIS2); // Rz

  gamepad.send(reinterpret_cast<const uint8_t*>(&report), sizeof(report));
  delay(1);
}
