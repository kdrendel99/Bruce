#include "core/powerSave.h"
#include "ok_button.h"
#include <Wire.h>
#include <interface.h>

#ifndef VEXT_PIN
#define VEXT_PIN 36
#endif

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

    pinMode(SEL_BTN, INPUT_PULLUP);

    // Default RF module + proven M5 Cap CC1101 wiring (CS must == CAP_CC1101_SS_PIN
    // for the mainline antenna-switch block to engage).
    bruceConfigPins.rfModule = CC1101_SPI_MODULE;
    bruceConfigPins.CC1101_bus.sck = (gpio_num_t)CC1101_SCK_PIN;
    bruceConfigPins.CC1101_bus.miso = (gpio_num_t)CC1101_MISO_PIN;
    bruceConfigPins.CC1101_bus.mosi = (gpio_num_t)CC1101_MOSI_PIN;
    bruceConfigPins.CC1101_bus.cs = (gpio_num_t)CC1101_SS_PIN;
    bruceConfigPins.CC1101_bus.io0 = (gpio_num_t)CC1101_GDO0_PIN;
}

/*********************************************************************
** InputHandler() - single PRG button (GPIO0).
**   tap=advance(DownPress)  hold=select(SelPress)  double-tap=back(EscPress)
*********************************************************************/
void InputHandler(void) {
    static OneButtonGesture btn;
    const uint32_t now = millis();
    const auto action = btn.update(digitalRead(SEL_BTN) == BTN_ACT, now);
    if (action == OneButtonGesture::Action::None) return;
    if (wakeUpScreen()) return; // the press that woke the screen is not a nav press

    AnyKeyPress = true;
    switch (action) {
        case OneButtonGesture::Action::Advance: DownPress = true; break; // NO_LR_BUTTONS: grid walks on UP/DOWN
        case OneButtonGesture::Action::Select: SelPress = true; break;
        case OneButtonGesture::Action::Back: EscPress = true; break;
        default: break;
    }
}
