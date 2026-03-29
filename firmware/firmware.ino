#define IR_SEND_PIN 3       // 3 for Arduino Nano but probably PIN_PA3 or PIN_PA5 on ATtiny3226...

#if !defined(ARDUINO_ESP32C3_DEV) // This is due to a bug in RISC-V compiler, which requires unused function sections :-(.
#define DISABLE_CODE_FOR_RECEIVER // Disables static receiver code like receive timer ISR handler and static IRReceiver and irparams data. Saves 450 bytes program memory and 269 bytes RAM if receiving functions are not required.
#endif

//#undef LED_BUILTIN          // No LED defined in megaTinyCore
//#define LED_BUILTIN PIN_PB5 // 4
//#define NO_LED_FEEDBACK_CODE      // Saves 216 bytes program memory

#include <IRremote.hpp>

const static int KEY_PWR = 4;
const static int KEY_VOLUMEUP = A3;
const static int KEY_VOLUMEDOWN = A0;

void setup() {
  // put your setup code here, to run once:
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(IR_SEND_PIN, OUTPUT);
  pinMode(KEY_PWR, INPUT_PULLUP);
  pinMode(KEY_VOLUMEUP, INPUT_PULLUP);
  pinMode(KEY_VOLUMEDOWN, INPUT_PULLUP);

  Serial.begin(115200);

  Serial.println();
  Serial.println();
  Serial.println(F("START " __FILE__ "\r\n from " __DATE__ " " __TIME__ "\r\n using library version " VERSION_IRREMOTE));
  Serial.print(F("Send IR signals at pin "));
  Serial.println(IR_SEND_PIN);
  Serial.println();
}

void loop() {
  // put your main code here, to run repeatedly:
  // read buttons and send codes according to IRDB
  // https://github.com/probonopd/irdb/blob/master/codes/Philips/Unknown_32PFL5403D/0%2C-1.csv
  if (!digitalRead(KEY_PWR)) {
    IrSender.sendRC6(0, 12, 1);
    while (!digitalRead(KEY_PWR)) {
      IrSender.sendRC6(0, 12, 1, false);
    }
  }
  if (!digitalRead(KEY_VOLUMEUP)) {
    IrSender.sendRC6(0, 16, 1);
    while (!digitalRead(KEY_VOLUMEUP)) {
      IrSender.sendRC6(0, 16, 1, false);
    }
  }
  if (!digitalRead(KEY_VOLUMEDOWN)) {
    IrSender.sendRC6(0, 17, 1);
    while (!digitalRead(KEY_VOLUMEDOWN)) {
      IrSender.sendRC6(0, 17, 1, false);
    }
  }
  delay(1);
}
