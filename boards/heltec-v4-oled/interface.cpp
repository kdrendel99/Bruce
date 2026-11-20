#include "core/mykeyboard.h" // powerOff(), goToDeepSleep()
#include "core/powerSave.h"
#include "core/settings.h" // startBLEAPIOnBoot()
#include "hal/device.h"
#include "hal/inputs/encoder.h"
#include <Wire.h>
#include <interface.h>

#ifndef VEXT_PIN
#define VEXT_PIN 36
#endif

/*********************************************************************
** Rotary encoder (EC11) — replaces the single PRG button.
**   A/B quadrature on ENCODER_INA/INB; integrated push-button on
**   ENCODER_KEY (GPIO0, parallel with PRG). A turn -> Prev/Next,
**   press -> Select. All handled by the shared encoder HAL.
*********************************************************************/
static DeviceEncoder encoderCfg() {
    DeviceEncoder cfg;
    cfg.pin_a = ENCODER_INA;
    cfg.pin_b = ENCODER_INB;
    // Button is handled manually in InputHandler() so we can distinguish a short press
    // (Select) from a long press (Back). pin_sel = -1 keeps the HAL from also firing
    // Select off the same pin, which would double up with our handling.
    cfg.pin_sel = -1;
    return cfg;
}

/*********************************************************************
** _setup_gpio() - pins + buses, runs before the display is initialised.
*********************************************************************/
void _setup_gpio() {
    // Vext powers the onboard OLED on the Heltec V4 (active-LOW). Must be enabled
    // before the I2C bus / display come up or the panel stays dark.
    pinMode(VEXT_PIN, OUTPUT);
    digitalWrite(VEXT_PIN, LOW);
    delay(60);

    Wire.begin(TFT_SDA, TFT_SCL);
    Wire.setClock(400000);

    // Hold the CC1101 chip-select high to avoid SPI bus contention before first use.
    pinMode(CC1101_SS_PIN, OUTPUT);
    digitalWrite(CC1101_SS_PIN, HIGH);

    // Connect the battery-sense divider (ADC-enable MOSFET, HIGH = on). Bruce's
    // ANALOG_BAT reader just reads GPIO1; it doesn't toggle this, so hold it enabled.
    pinMode(BAT_ADC_CTRL, OUTPUT);
    digitalWrite(BAT_ADC_CTRL, HIGH);

    // Default RF module + proven M5 Cap CC1101 wiring (CS must == CAP_CC1101_SS_PIN
    // for the mainline antenna-switch block to engage).
    bruceConfigPins.rfModule = CC1101_SPI_MODULE;
    bruceConfigPins.CC1101_bus.sck = (gpio_num_t)CC1101_SCK_PIN;
    bruceConfigPins.CC1101_bus.miso = (gpio_num_t)CC1101_MISO_PIN;
    bruceConfigPins.CC1101_bus.mosi = (gpio_num_t)CC1101_MOSI_PIN;
    bruceConfigPins.CC1101_bus.cs = (gpio_num_t)CC1101_SS_PIN;
    bruceConfigPins.CC1101_bus.io0 = (gpio_num_t)CC1101_GDO0_PIN;

    // M5 Cap's second chip: ST25R3916 NFC on the SAME SPI bus as the CC1101 above
    // (shared SCK/MOSI/MISO), with its own chip-select (CAP_NFC_SS_PIN) and a
    // dedicated interrupt line (CAP_NFC_IRQ_PIN, io0). Default to it so RFID > Read
    // tag works out of the box; /brucePins.conf still overrides if the user remaps.
    bruceConfigPins.rfidModule = ST25R3916_SPI_MODULE;
    bruceConfigPins.ST25R_bus.sck = (gpio_num_t)CC1101_SCK_PIN;
    bruceConfigPins.ST25R_bus.miso = (gpio_num_t)CC1101_MISO_PIN;
    bruceConfigPins.ST25R_bus.mosi = (gpio_num_t)CC1101_MOSI_PIN;
    bruceConfigPins.ST25R_bus.cs = (gpio_num_t)CAP_NFC_SS_PIN;
    bruceConfigPins.ST25R_bus.io0 = (gpio_num_t)CAP_NFC_IRQ_PIN;

    // IR: transmit LED on GPIO15, 3-pin receiver OUT on GPIO16. Board default; the
    // IR Tx/Rx pin pickers in Config > Set Device Pins can still override + persist.
    bruceConfigPins.irTx = 15;
    bruceConfigPins.irRx = 16;

    // HAL sets up A/B (and their interrupts); the push-button is ours, so pull it up here.
    pinMode(ENCODER_KEY, INPUT_PULLUP);
    hal_encoder_init(encoderCfg());
}

/*********************************************************************
** InputHandler() - runs ~every 10ms in the input task. The encoder HAL
** reads the quadrature + button and sets the nav globals (Prev/Next on
** turn, Select on press) and handles wake-from-powersave itself.
*********************************************************************/
void InputHandler(void) {
    // Rotation -> Prev/Next via the shared HAL (button disabled there, see encoderCfg).
    hal_encoder_poll(encoderCfg());

    // Encoder push-button on ENCODER_KEY (GPIO0): short press = Select, hold = Back.
    static bool wasDown = false;
    static bool swallow = false; // a press that only woke the screen is not a nav action
    static bool longFired = false;
    static uint32_t downAt = 0;
    const uint32_t now = millis();
    const bool down = digitalRead(ENCODER_KEY) == LOW;

    if (down && !wasDown) { // press begins
        wasDown = true;
        downAt = now;
        longFired = false;
        swallow = wakeUpScreen();
    } else if (down && wasDown) { // held
        if (!swallow && !longFired && now - downAt >= 400) {
            longFired = true; // fire Back at the threshold, no need to wait for release
            EscPress = true;
            AnyKeyPress = true;
        }
    } else if (!down && wasDown) { // released
        if (!swallow && !longFired) { // quick tap -> Select
            SelPress = true;
            AnyKeyPress = true;
        }
        wasDown = false;
    }
}

/*********************************************************************
** _late_setup_gpio() - runs after display + input task are up.
** Auto-start the BLE companion API on boot. The core's enable flag is
** RAM-only (resets every power-cycle), so without this you'd have to
** toggle Config > ... > BLE API by hand after every reboot.
*********************************************************************/
void _late_setup_gpio() { startBLEAPIOnBoot(); }

/*********************************************************************
** powerOff() - overrides the weak "Not available" default. The Heltec V4.3 has
** no software power-kill (main 3V3 rail is always on), so true power-off is
** impossible on this platform; the lowest-power "off" is deep sleep, which on
** battery draws ~tens of µA and wakes on the GPIO0 button. Reuses Bruce's
** existing goToDeepSleep() — nothing new implemented here.
*********************************************************************/
void powerOff() { goToDeepSleep(); }
