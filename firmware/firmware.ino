#define IR_SEND_PIN 3       // 3 for Arduino Nano but probably PIN_PA3 or PIN_PA5 on ATtiny3226...

#if !defined(ARDUINO_ESP32C3_DEV) // This is due to a bug in RISC-V compiler, which requires unused function sections :-(.
#define DISABLE_CODE_FOR_RECEIVER // Disables static receiver code like receive timer ISR handler and static IRReceiver and irparams data. Saves 450 bytes program memory and 269 bytes RAM if receiving functions are not required.
#endif

//#undef LED_BUILTIN          // No LED defined in megaTinyCore
//#define LED_BUILTIN PIN_PB5 // 4
//#define NO_LED_FEEDBACK_CODE      // Saves 216 bytes program memory

#include <IRremote.hpp>

// codes for Philips 32PFL5403D according to IRDB
// https://github.com/probonopd/irdb/blob/master/codes/Philips/Unknown_32PFL5403D/0%2C-1.csv


struct Function {
  int keyPin;
  uint8_t rc6Address;
  uint8_t rc6Command;
  const char* name;
};

static constexpr int TIME_DEBOUNCE_MS = 10;
static constexpr int TIME_SINGLE_PRESS_MS = 500;
static constexpr int TIME_REPEAT_PRESS_MS = 100;
static constexpr int N_FUNCTIONS = 3;

static constexpr Function functions[N_FUNCTIONS] = {
  {4, 0, 12, "KEY_PWR"},
  {A3, 0, 16, "KEY_VOLUMEUP"},
  {A0, 0, 17, "KEY_VOLUMEDOWN"},
};


void setup() {
  // put your setup code here, to run once:
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(IR_SEND_PIN, OUTPUT);
  for (auto fn : functions) {
    pinMode(fn.keyPin, INPUT_PULLUP);
  }

  Serial.begin(2000000);

  Serial.println();
  Serial.println();
  Serial.println(F("START " __FILE__ "\r\n from " __DATE__ " " __TIME__ "\r\n using library version " VERSION_IRREMOTE));
  Serial.print(F("Send IR signals at pin "));
  Serial.println(IR_SEND_PIN);
  Serial.println();
}

void loop() {
  // put your main code here, to run repeatedly:
  // read buttons and send codes defined above

  for (auto fn : functions) {
    if (!digitalRead(fn.keyPin)) {
      // this functions key was pressed
      auto timeNextSendMs = millis();
      int sendCount = 0;

      Serial.print(timeNextSendMs);
      Serial.print(" pressed ");
      Serial.print(fn.name);

      // debounce
      while (millis() - timeNextSendMs < TIME_DEBOUNCE_MS) {
        if (digitalRead(fn.keyPin)) {
          Serial.println(" - debounce abort");
          return; // the key was released
        }
      }

      // still pressed - commence action
      Serial.print(" - sending ");
      Serial.print(fn.rc6Command);

      // send once
      IrSender.sendRC6(fn.rc6Address, fn.rc6Command, 0);
      ++sendCount;
      Serial.print(".");
      
      // wait for single send release
      timeNextSendMs += TIME_SINGLE_PRESS_MS;
      while (millis() < timeNextSendMs) {
        if (digitalRead(fn.keyPin)) {
          Serial.println(" 1 time");
          return; // the key was released
        }
      }

      // enter repeat mode for as long as key continues to be pressed
      while (true) {
        IrSender.sendRC6(fn.rc6Address, fn.rc6Command, 0);
        ++sendCount;
        Serial.print(".");
        timeNextSendMs += TIME_REPEAT_PRESS_MS;
        while (millis() < timeNextSendMs) {
          if (digitalRead(fn.keyPin)) {
            Serial.print(" ");
            Serial.print(sendCount);
            Serial.println(" times");
            Serial.flush();
            return; // the key was released
          }
        }
      }
    }
  }
}
