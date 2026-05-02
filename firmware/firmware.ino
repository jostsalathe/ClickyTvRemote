/************************
 *
 * This is builtfor the ClickyRemote running on an ATtiny3226.
 * You need the TinyMegaCore board package and the IRremote library.
 * Set Tools -> Clock -> 8 MHz internal
 *
 ************************/

#include <avr/sleep.h>


#define IR_SEND_PIN PIN_PA5

#define DISABLE_CODE_FOR_RECEIVER // Disables static receiver code like receive timer ISR handler and static IRReceiver and irparams data. Saves 450 bytes program memory and 269 bytes RAM if receiving functions are not required.
#define IR_USE_AVR_TIMER_A        // Fix because ATtiny3226 is not explicitly supported by the IRremote library, yet...

#undef LED_BUILTIN                // No LED defined in megaTinyCore
#define LED_BUILTIN PIN_PA1
//#define NO_LED_FEEDBACK_CODE      // Saves 216 bytes program memory

#ifdef NO_LED_FEEDBACK_CODE
#define ON_PRINT(A) A
#else
#define ON_PRINT(A)
#endif

#include <IRremote.hpp>


/* function prototypes */

void setPinModes(bool prepareSleep);
void powerDown();
void reportBattery(bool print);


/* structure definitions */

struct Command {
  int keyPin;
  uint8_t rc6Address;
  uint8_t rc6Command;
  const char* name;
};

struct VoltageToSoc {
    uint16_t voltageMv;
    uint8_t soc; // in %
};


/* static variable definitions */

static constexpr int TIME_DEBOUNCE_MS = 10;
static constexpr int TIME_SINGLE_PRESS_MS = 500;
static constexpr int TIME_REPEAT_PRESS_MS = 100;

// codes for Philips 32PFL5403D according to IRDB
// https://github.com/probonopd/irdb/blob/master/codes/Philips/Unknown_32PFL5403D/0%2C-1.csv
static constexpr Command commands[] = {
  {PIN_PB0, 0xff, 0xff, "KEY_BAT"},        // KEY_0_0
  {PIN_PB5, 0, 12, "KEY_PWR"},             // KEY_1_0
  {PIN_PA2, 0, 12, "KEY_PWR"},             // KEY_2_0
  {PIN_PC0, 0, 32, "KEY_CHANNELUP"},       // KEY_0_1
  {PIN_PB4, 0, 16, "KEY_VOLUMEUP"},        // KEY_1_1
  {PIN_PA3, 0, 245, "KEY_Picture_Format"}, // KEY_2_1
  {PIN_PC1, 0, 33, "KEY_CHANNELDOWN"},     // KEY_0_2
  {PIN_PB3, 0, 17, "KEY_VOLUMEDOWN"},      // KEY_1_2
  {PIN_PA4, 0, 13, "KEY_MUTE"},            // KEY_2_2
  {PIN_PC2, 0, 84, "KEY_MENU"},            // KEY_0_3
  {PIN_PB2, 0, 88, "KEY_UP"},              // KEY_1_3
  {PIN_PA6, 0, 92, "KEY_OK"},              // KEY_2_3
  {PIN_PC3, 0, 90, "KEY_LEFT"},            // KEY_0_4
  {PIN_PB1, 0, 89, "KEY_DOWN"},            // KEY_1_4
  {PIN_PA7, 0, 91, "KEY_RIGHT"},           // KEY_2_4
};

// millivolt -> state of charge look up table for two NiMH cells
static constexpr VoltageToSoc voltSocLUT[] = {
    {2900, 100},
    {2760, 95},
    {2680, 90},

    // Plateau (wichtigster Bereich!)
    {2600, 80},
    {2570, 70},
    {2540, 60},
    {2520, 50},
    {2500, 40},
    {2480, 30},

    // Entladungsknick
    {2440, 20},
    {2400, 10},
    {2360, 5},
    {2300, 2},
    {2200, 0}
};


void setup() {
  // put your setup code here, to run once:

  // upon waking if you plan to use the ADC
  ADC0.CTRLA |= ADC_ENABLE_bm;
  analogReference(INTERNAL1V024); // set reference to the desired voltage, and set that as the ADC reference.
  uint16_t reading = analogRead(ADC_VDDDIV10); //first reading might be inaccturate
  (void) reading; // to suppress unused variable warning

  // after wakeup, disable interrupts and enable Pullups for all buttons
  setPinModes(false);

  ON_PRINT(Serial.pins(PIN_PA1, PIN_PA2));
  ON_PRINT(Serial.begin(230400, SERIAL_TX_ONLY));

  ON_PRINT(Serial.println());
  ON_PRINT(Serial.println());
  ON_PRINT(Serial.println(F("START " __FILE__ "\r\n from " __DATE__ " " __TIME__ "\r\n using library version " VERSION_IRREMOTE)));
  ON_PRINT(Serial.print(F("Send IR signals at pin ")));
  ON_PRINT(Serial.println(IR_SEND_PIN));
  ON_PRINT(Serial.println());

  // read buttons and find out what woke up the controller
  const Command *triggeredCmd = nullptr;
  for (const Command &cmd : commands) {
    if (!digitalRead(cmd.keyPin)) {
      // this commands key was pressed
      triggeredCmd = &cmd;
      break;
    }
  }

  // execute triggered command
  if (triggeredCmd != nullptr) {
    auto timeNextSendMs = millis();
    int sendCount = 0;

    ON_PRINT(Serial.print(millis()));
    ON_PRINT(Serial.print(" ms - pressed "));
    ON_PRINT(Serial.print(triggeredCmd->name));

    // debounce
    while (millis() - timeNextSendMs < TIME_DEBOUNCE_MS) {
      if (digitalRead(triggeredCmd->keyPin)) {
        ON_PRINT(Serial.println(" - debounce abort"));
        powerDown();
      }
    }

    // still pressed - commence action
    if (triggeredCmd->rc6Address == 0xff && triggeredCmd->rc6Command == 0xff) {
      // handle battery readout
      reportBattery(true);
      while (digitalRead(triggeredCmd->keyPin) == LOW) {
        reportBattery(false);
      }
      ON_PRINT(Serial.println());
      powerDown();
    } else {
      // send command
      ON_PRINT(Serial.print(" - sending "));
      ON_PRINT(Serial.print(triggeredCmd->rc6Command));

      // send once
      IrSender.sendRC6(triggeredCmd->rc6Address, triggeredCmd->rc6Command, 0);
      ++sendCount;
      ON_PRINT(Serial.print("."));

      // wait for single send release
      timeNextSendMs += TIME_SINGLE_PRESS_MS;
      while (millis() < timeNextSendMs) {
        if (digitalRead(triggeredCmd->keyPin)) {
          ON_PRINT(Serial.println(" 1 time"));
          powerDown();
        }
      }

      // enter repeat mode for as long as key continues to be pressed
      while (true) {
        IrSender.sendRC6(triggeredCmd->rc6Address, triggeredCmd->rc6Command, 0);
        ++sendCount;
        ON_PRINT(Serial.print("."));
        timeNextSendMs += TIME_REPEAT_PRESS_MS;
        while (millis() < timeNextSendMs) {
          if (digitalRead(triggeredCmd->keyPin)) {
            ON_PRINT(Serial.print(" "));
            ON_PRINT(Serial.print(sendCount));
            ON_PRINT(Serial.println(" times"));
            ON_PRINT(Serial.flush());
            powerDown();
          }
        }
      }
    }
  }
  powerDown();
}

void loop() {
  // put your main code here, to run repeatedly:
  ON_PRINT(Serial.println("How the heck did I end up in loop()?!"));
  powerDown();
}


void setPinModes(bool prepareSleep) {
  const uint8_t newPinMode = 0b1000 | (prepareSleep ? PORT_ISC_LEVEL_gc : PORT_ISC_INTDISABLE_gc);
  
  // enable pin change interrupt for all buttons
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(IR_SEND_PIN, OUTPUT);
  digitalWrite(IR_SEND_PIN, LOW);

  PORTA.PIN2CTRL = newPinMode;
  PORTA.PIN3CTRL = newPinMode;
  PORTA.PIN4CTRL = newPinMode;
  PORTA.PIN6CTRL = newPinMode;
  PORTA.PIN7CTRL = newPinMode;
  PORTB.PIN0CTRL = newPinMode;
  PORTB.PIN1CTRL = newPinMode;
  PORTB.PIN2CTRL = newPinMode;
  PORTB.PIN3CTRL = newPinMode;
  PORTB.PIN4CTRL = newPinMode;
  PORTB.PIN5CTRL = newPinMode;
  PORTC.PIN0CTRL = newPinMode;
  PORTC.PIN1CTRL = newPinMode;
  PORTC.PIN2CTRL = newPinMode;
  PORTC.PIN3CTRL = newPinMode;
}

void powerDown() {
  // Before sleeping
  ON_PRINT(Serial.print(millis()));
  ON_PRINT(Serial.println(" ms - Entering deep sleep..."));
  ADC0.CTRLA &= ~ADC_ENABLE_bm; //Very important on the tinyAVR 2-series

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();

  // and clear interrupt flags
  PORTA.INTFLAGS = 0xff;
  PORTB.INTFLAGS = 0xff;
  PORTC.INTFLAGS = 0xff;

  // enable pin change interrupt for all buttons
  ON_PRINT(Serial.flush());
  ON_PRINT(Serial.end());
  setPinModes(true);

  sleep_cpu();
}

uint8_t voltageToSoc(uint16_t mv) {
    constexpr uint8_t n = sizeof(voltSocLUT) / sizeof(voltSocLUT[0]);

    for (int i = 0; i < n - 1; ++i) {
        const uint16_t mvHigh = voltSocLUT[i].voltageMv;
        const uint8_t socHigh = voltSocLUT[i].soc;
        const uint16_t mvLow = voltSocLUT[i+1].voltageMv;
        const uint8_t socLow = voltSocLUT[i+1].soc;

        if (mv <= mvHigh && mv >= mvLow) {
            const uint16_t dmv = mv - mvLow;
            const uint16_t ds = socHigh - socLow;

            // nur hier kurz 32-bit für Sicherheit
            int32_t interp = (int32_t)dmv * ds;

            return socLow + interp / (mvHigh - mvLow);
        }
    }

    return (mv > voltSocLUT[0].voltageMv) ? 100 : 0;
}

void dimLedBuiltinDelay(uint16_t ms, uint8_t brightness) {
  auto end = millis() + ms;
  while(millis() < end) {
    if ( ((micros() / 4) % 256) < brightness) {
      digitalWriteFast(LED_BUILTIN, HIGH);
    } else {
      digitalWriteFast(LED_BUILTIN, LOW);
    }
  }
}

void reportBattery(bool print) {
  uint16_t uBatMv = analogReadEnh(ADC_VDDDIV10, 14, 2) * 10 / 32; // read battery voltage
  uBatMv -= 20; // offset calibration
  uint8_t soc = voltageToSoc(uBatMv);// calculate battery level
  // report battery level
#ifdef NO_LED_FEEDBACK_CODE
  if (print) {
    ON_PRINT(Serial.print(" - UBat = "));
    ON_PRINT(Serial.print(uBatMv));
    ON_PRINT(Serial.print(" mV ("));
    ON_PRINT(Serial.print(soc));
    ON_PRINT(Serial.println(" %)"));
  }
#else
  // via blink code on LED_BUILTIN
  soc = (soc + 5) / 10; // for correct roundling with step size of 10%
  for (int i = 0; i < 10; ++i) {
    bool on = soc > i;
    dimLedBuiltinDelay(on ? 200 : 10, 32);
    digitalWrite(LED_BUILTIN, LOW);
    delay(on ? 300 : 490);
  }
#endif
}

