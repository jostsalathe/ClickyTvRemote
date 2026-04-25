#define IR_SEND_PIN PIN_PA5       // 3 for Arduino Nano but PIN_PA5 on ATtiny3226...

#if !defined(ARDUINO_ESP32C3_DEV) // This is due to a bug in RISC-V compiler, which requires unused function sections :-(.
#define DISABLE_CODE_FOR_RECEIVER // Disables static receiver code like receive timer ISR handler and static IRReceiver and irparams data. Saves 450 bytes program memory and 269 bytes RAM if receiving functions are not required.
#endif

#undef LED_BUILTIN                // No LED defined in megaTinyCore
#define LED_BUILTIN PIN_PA1
//#define NO_LED_FEEDBACK_CODE      // Saves 216 bytes program memory

#define ON_PRINT(A)

#include <IRremote.hpp>

// codes for Philips 32PFL5403D according to IRDB
// https://github.com/probonopd/irdb/blob/master/codes/Philips/Unknown_32PFL5403D/0%2C-1.csv


struct Command {
  int keyPin;
  uint8_t rc6Address;
  uint8_t rc6Command;
  const char* name;
};

static constexpr int TIME_DEBOUNCE_MS = 10;
static constexpr int TIME_SINGLE_PRESS_MS = 500;
static constexpr int TIME_REPEAT_PRESS_MS = 100;

static constexpr Command commands[] = {
  {PIN_PB0, 0, 12, "KEY_PWR"},             // KEY_0_0
  {PIN_PB5, 0, 12, "KEY_PWR"},             // KEY_1_0
  // {4, 0, 12, "KEY_PWR"}, // on Nano prototype
  {PIN_PA2, 0, 12, "KEY_PWR"},             // KEY_2_0
  {PIN_PC0, 0, 32, "KEY_CHANNELUP"},       // KEY_0_1
  // {A3, 0, 16, "KEY_VOLUMEUP"}, // on Nano prototype
  {PIN_PB4, 0, 16, "KEY_VOLUMEUP"},        // KEY_1_1
  {PIN_PA3, 0, 245, "KEY_Picture_Format"}, // KEY_2_1
  {PIN_PC1, 0, 33, "KEY_CHANNELDOWN"},     // KEY_0_2
  // {A0, 0, 17, "KEY_VOLUMEDOWN"}, // on Nano prototype
  {PIN_PB3, 0, 17, "KEY_VOLUMEDOWN"},      // KEY_1_2
  {PIN_PA4, 0, 13, "KEY_MUTE"},            // KEY_2_2
  {PIN_PC2, 0, 84, "KEY_MENU"},            // KEY_0_3
  {PIN_PB2, 0, 88, "KEY_UP"},              // KEY_1_3
  {PIN_PA6, 0, 92, "KEY_OK"},              // KEY_2_3
  {PIN_PC3, 0, 90, "KEY_LEFT"},            // KEY_0_4
  {PIN_PB1, 0, 89, "KEY_DOWN"},            // KEY_1_4
  {PIN_PA7, 0, 91, "KEY_RIGHT"},           // KEY_2_4
};


void setup() {
  // put your setup code here, to run once:
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(IR_SEND_PIN, OUTPUT);
  for (auto cmd : commands) {
    pinMode(cmd.keyPin, INPUT_PULLUP);
  }

  ON_PRINT(Serial.pins(PIN_PA1, PIN_PA2));
  ON_PRINT(Serial.begin(2000000));

  ON_PRINT(Serial.println());
  ON_PRINT(Serial.println());
  ON_PRINT(Serial.println(F("START " __FILE__ "\r\n from " __DATE__ " " __TIME__ "\r\n using library version " VERSION_IRREMOTE)));
  ON_PRINT(Serial.print(F("Send IR signals at pin ")));
  ON_PRINT(Serial.println(IR_SEND_PIN));
  ON_PRINT(Serial.println());
}

void loop() {
  // put your main code here, to run repeatedly:
  // read buttons and send codes defined above

  for (auto cmd : commands) {
    if (!digitalRead(cmd.keyPin)) {
      // this commands key was pressed
      auto timeNextSendMs = millis();
      int sendCount = 0;

      ON_PRINT(Serial.print(timeNextSendMs));
      ON_PRINT(Serial.print(" pressed "));
      ON_PRINT(Serial.print(cmd.name));

      // debounce
      while (millis() - timeNextSendMs < TIME_DEBOUNCE_MS) {
        if (digitalRead(cmd.keyPin)) {
          ON_PRINT(Serial.println(" - debounce abort"));
          return; // the key was released
        }
      }

      // still pressed - commence action
      ON_PRINT(Serial.print(" - sending "));
      ON_PRINT(Serial.print(cmd.rc6Command));

      // send once
      IrSender.sendRC6(cmd.rc6Address, cmd.rc6Command, 0);
      ++sendCount;
      ON_PRINT(Serial.print("."));
      
      // wait for single send release
      timeNextSendMs += TIME_SINGLE_PRESS_MS;
      while (millis() < timeNextSendMs) {
        if (digitalRead(cmd.keyPin)) {
          ON_PRINT(Serial.println(" 1 time"));
          return; // the key was released
        }
      }

      // enter repeat mode for as long as key continues to be pressed
      while (true) {
        IrSender.sendRC6(cmd.rc6Address, cmd.rc6Command, 0);
        ++sendCount;
        ON_PRINT(Serial.print("."));
        timeNextSendMs += TIME_REPEAT_PRESS_MS;
        while (millis() < timeNextSendMs) {
          if (digitalRead(cmd.keyPin)) {
            ON_PRINT(Serial.print(" "));
            ON_PRINT(Serial.print(sendCount));
            ON_PRINT(Serial.println(" times"));
            ON_PRINT(Serial.flush());
            return; // the key was released
          }
        }
      }
    }
  }
}
