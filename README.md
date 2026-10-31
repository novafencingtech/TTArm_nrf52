# TournaTest Digital User’s Guide

## Introduction
The TournaTest Digital is a compact high performance, armorer’s electronic test box. The test box consists of a base board, an Arduino microprocessor board, a digital color display, rechargeable lithium battery, case, pushbutton switch, and banana jacks.  The device allows testing of the following:

* Épées and foils
* Lamé materials
* Body and mask wires

![Front panels of the desktop and compact TournaTest Digital testers][image1]

Front panel display of the desktop TTArm at idle. The main power and function button is at the upper left. Hold down to power the unit on. (Right: compact case.)

**Left connectors:**  Used for weapon and body cord testing.  For weapon testing, connect a body cord attached to the weapon.  For epee, depress the weapon tip for approximately 3s and the unit will automatically switch mode.

**Right connectors:**  Used for body cord testing.

**Brass posts:**  Used for mask/lame clip tests.  For checking foil/saber cords, use the right post.  These are internally connected to the “A” line.  On the compact case or medium case, use the exposed A connector bodies.

**Lame connectors (Right side)**:  Used for lame testing, these are internally connected to the C lines.

## Power On-Off

To turn on the test box, push the pushbutton switch and hold until the title screen is displayed.  Release the pushbutton switch immediately.  The test box will now go into “idle” mode.  **If it does not power on, press the power button and immediately release (very quick click and release), then press and hold**

At any time when using the test box and you press and hold the pushbutton switch until the screen says “Power Off”, then release, the test box will turn off.

Note: The test box may not power down if it’s warm.  See Errata \#2 below

The test box will go into a “Idle” mode at any time when it is not being used, and after a few minutes, it will go into a power save mode in which the screen is turned off.  If the test box is not used for an extended amount of time, it will automatically turn off power

The battery disconnect switch provides a physical disconnect for the battery.  This can be for shipping or as a reset.  The switch can be left “On (right)” for all normal usage with no impact on battery life.

![Battery disconnect switch][image2]

## Basic/Advanced

The test box can be set to one of two modes: Basic and Advanced.  In Basic Mode, the device automatically goes to the correct display based on what is plugged in, and the push button is only used to power it on and off and get into the Settings menu.  A restricted set of pages are used in this mode (Cable, Lame and Weapon).  In Advanced Mode, more pages are available, and the push button forces the display to move to the next page.  The test box will automatically switch between cable and weapon pages depending on what is plugged in, but tapping the push button will force it to move to the next page

## Menu Structure (Advanced mode only)

The TournaTest Digital test box uses a circular menu structure to go to the various testing modes.  By Pressing the pushbutton switch, the tester software will move to the next operating mode in the sequence.

* Cable
* Weapon
* Capture
* Weapon Test (WpnTest)
* Probe
* Lame

This test box has the capability of automatically switching to and from the Cable, Weapon, or Lame test modes.

## Cable

Insert the 3 pin body wire connector into the left set of jacks.

For epee, insert the other 3 pin connector into the right set of jacks.

For foil body wires, use the 2-pin foil socket and the right-side metal post.  To test bayonet style body wires, you will need a bayonet to 2-pin adaptor (not provided).

To check mask wires, attach the alligator clips to the two metal posts.

The display will show the resistance level and (depending on settings) a sliding bar graph for each wire.  In addition, the display will change color from red (open circuit) to yellow to green (0 to 1Ω).

## Weapon
Plug the 3 pin side of a known good body wire into the left set of jacks.  Plug the weapon into the other end of the body wire. The test box will automatically detect if you are testing a foil or an epee.

The display will now show the resistance level and, depending on settings, a sliding bar graph for each weapon type. Below the sliding bar graph, there will be a graphic display that shows the performance of the weapon tip. In addition, the display will change color from red (open circuit) to yellow to green (0 to 2Ω).

If you look carefully, there will be a simple on-off LED display that is similar to what is found on a Favero A1 tester.  This test mode also has the ability to detect micro breaks too.

## Capture – Advanced Only

Press the pushbutton switch until the test box goes into the “Armed” test mode.  You will need to connect a working body cord and weapon into the left banana jacks.

Press the weapon tip once.  The display will show a graph of the performance of the tip for a single hit. The test box will reset automatically (Armed) and allow you to repeat the test again.

The value display is the duration of the contact in ms.

## Weapon Test (WpnTest) – Advanced Only

Press the pushbutton switch until the test box goes into the “WpnTest” test mode.  You will need to connect a working body cord and weapon into the input banana jacks.  The test box will now operate like the Favero A1 tester.  This test mode also has the ability to detect micro breaks (yellow “lights”).

## Probe
Press the pushbutton switch until the test box goes into the “Probe” page.  You will need to connect a working body cord and weapon into the left input banana jacks and a separate test cord (not provided) plugged into the Right “A” banana jack. You can now test for different component electrical resistances.

For epee, “PrA” and “PrB” will show the wire resistance from the socket to the tip for each blade wire.  “PrC” would show the blade and guard resistance.

For foil, “PrB” and “PrC” will show the wire resistance from the socket to the tip and the blade.

The weapon electrical resistance will also be shown on the display.

## Lame Materials
To test for lame materials, the test box is designed to go into a special test mode that is part of the “Cable” test.  Any indication of a continuity only between the two “C” banana jacks will trigger the test box to go into the lame test function.  The desktop model has two jacks on the side of the box connected to the “C” lines for the lame test.

The display will show the measured resistance, optional sliding bar graph, and graphic display for the lame material.  In addition, the display will change color from red (open circuit) to yellow to green (0 to 5Ω).

### Lame LED (Optional)

The optional Lame LED is a small three color LED on a cable,  It plugs into the jack on the back of the desktop test box.  The LED has a magnet that will stick to a steel test weight.  The LED lights when the LAME test function is running.  Green is less than 3 ohms, orange is between 3.1 and 5 ohms and red is more than 5 ohms.  The Lame LED is only supported on the large case.

## Power Off

There is a power off slide switch in case the software programming gets locked up.  Do not use the power off slide switch unless directed to.  “On” is toward the front of the box on the desktop unit, and to the right on the small case.  The switch is a bit delicate, so be careful when changing its state.

## Charging

The internal battery is charged via USB.  Simply connect any USB power supply or power bank to the micro-USB port to start charging.  The orange LED next to the connector should illuminate.

Charging time:

Small case \~ 24 hrs to full charge

Large case \~ 72 hrs to full charge

Modification of charging value should only be attempted by advanced users comfortable with soldering surface mount devices.  The charging rate is set by the R8 resistor on the Adafruit Feather nrf52832.  The default 10k value sets a charging rate of 100 mA.  The nominal 10k resistor can be replaced by a 0603 with a value of 2.0k to 10k.  A value of 2.0k corresponds to a 500 mA charging rate.


## Magnetic Base

There are two magnets built into the base of the desktop test box.  They are designed to be used with a steel plate taped to a table to keep the box in position at the armorer’s station.  Any sheet steel will work.  One source is a pair of electrical box covers.  Tape the steel down to the table.  The box will stick to the plate, but can easily be pried off to store.

Spring steel build plates for resin 3d printers make lightweight and excellent work-pieces.

An example:  [Resin printer spring steel build plates](https://www.amazon.com/gp/product/B0BJF2K9N9/)

## Hardware

Case screws for the compact case are M3x10mm with a Torx T10 head (T10).  Internal screws are M2.5x5mm with Torx T8.
Torx screws are less prone to rounding than small diameter internal hex.

## Settings

If you continue to press the pushbutton switch after the “Power off” indicator is shown on the display, the test box will go into a Settings page.  There are four menu options on this page:

- Box Mode
- Lame
- Weapon
- Calibration

Quick press:  Next item

Hold:  Select current item

You can select the option by pressing the button briefly, the box will move to the next option, indicated by a “\>”.  When you get to the option you want, hold the button down until the option page is displayed.  Within a settings page, the current selected value is blue text outlined in white.  A short press moves to the next option, a long press selects that option.  The last option in every menu is “Back”.  If you long press it, the test box goes to the previous menu.

### Box Mode

This page sets the mode of the box to Basic or Advanced.  In basic mode, only the cord, lame and weapon pages are available and the box automatically switches between pages depending on what is plugged in and working.  In advanced mode, in addition to auto paging of cable, lame and weapon, the push button forces moving on to the next page, and makes the Capture, WpnTest and Probe pages available.

### Lame

This page controls what is displayed when testing Lames.  In Numeric only the resistance, shown as large digits are displayed.  In Graph, the resistance measurement is shown in a smaller font and a graph of the last few seconds of readings is displayed.

### Weapon

This page controls what is displayed when testing weapons.  In Numeric only the resistance, shown as large digits are displayed.  In Graph, the resistance measurement is shown in a smaller font and a graph of the last few seconds of readings is displayed.

### Calibration

This page runs a calibration routine for the test box.

You will need a short test cord with 4mm banana plugs to be able to do the calibration process.  Try to use a cord with the lowest resistance you have.

The display will prompt you to insert the test cord into various banana jacks in a set sequence.  After all of the procedure steps have been done, the software will prompt you to save or discard the recalibration.  Calibration data is saved across power cycles.

If you accidentally get into Calibration mode, disconnect all wires and press the button a few times, it will show errors and eventually exit without disturbing the current calibration.

## Errata

1) **Test Box fails to power on**

The box may fail to turn on by just pressing the switch when it is off.  If it doesn’t power up, press the button and release quickly (very short “button down” time), then press and hold.  It should start up.

2) **Test box fails to power off.**

When the test box is exposed to elevated temperatures (\>30C/90F) the test box will fail to automatically power off, and will not properly power off when shutdown by software.

This is caused by excess leakage from the battery charging circuitry on the microcontroller board.

To fix:  The box uses very little power in its power save mode, so best to just leave it.  Use the dedicated hardware slide switch to power off if necessary.

3) **Box may not power on when directly connected to USB power.**

The red LED will slowly blink, indicating that the device is in bootloader mode.
To fix, power on the box prior to connecting to USB, if you want to run it from USB.

4) **Calibration values for probe mode are only updated on reboot.**

Reboot the tester after updating the probe calibration values.

5) **Test box doesn’t show 0.0 Ohm after calibration.**

A small offset of 0.1 Ohm \- 0.3 Ohm is normal behavior.  If the offset is larger, check that the connectors are all tight, and try a different cable.  Due to minor differences in how the calibration routine operates, there is sometimes a small residual offset depending on exactly what cable is used.  At higher resistance (\>2 Ohm), this minor capacitance effect vanishes.  If you are unhappy with the calibration performance, reach out to me, as this may indicate that a component is failing.


## Small and Compact Cases

The TournaTest Digital also comes in a compact size designed to be stored in a fencing bag or armorer’s toolkit and used strip-side.  It is the identical circuit and software.  The only differences are:

* There are no dedicated clip posts: use the “A” banana jacks, which are solid metal
* There are no dedicated Lame connectors: use the “C” banana jacks
* The Lame LED is not supported

[image1]: Docs/images/front-panel.png

[image2]: Docs/images/battery-disconnect-switch.png
