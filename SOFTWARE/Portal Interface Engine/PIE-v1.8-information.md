## Lamoka1 DEV-Brain — PIE Version V1.8

The **Lamoka1 DEV-Brain V1.8** is the Raspberry Pi RP2354A firmware that operates the Lamoka1-DEV-Brain's user interface, ten WS2812B RGB LEDs, and single control button. V1.8 is built around a simple one-button interface, with the button's different press durations and combinations used to navigate menus, select modes, change settings, play the Fireball game, and return to the default idle state.

The firmware is written using Arduino functionality together with the **FastLED** library for LED control and the Raspberry Pi Pico boot ROM interface for USB BOOTSEL mode.

### Hardware Interface

V1.8 directly defines the following hardware connections:

* **GPIO 11** — WS2812B LED data
* **GPIO 0** — User button
* **GPIO 5** — Additional digital entropy input
* **GPIO 26 / ADC0**
* **GPIO 27 / ADC1**
* **GPIO 28 / ADC2**
* **GPIO 29 / ADC3**

The firmware controls **10 WS2812B LEDs** as a single FastLED array. The LEDs are configured for the WS2812B's **GRB** color order.

The default operating values are:

* **Brightness:** 6
* **Speed:** 100
* **Frame interval:** 16 ms
* **Menu timeout:** 16 seconds
* **Minimum brightness:** 6

The firmware also deliberately disables FastLED dithering.

### One-Button Interface

The entire user interface is controlled through one button connected using `INPUT_PULLUP`. V1.8 does not simply detect whether the button is pressed; it measures the length and timing of each press to determine what the user is trying to do.

The firmware recognizes five useful gestures:

* **Tap** — a short press
* **Double tap** — two taps within 380 ms
* **Hold** — approximately 550 ms
* **Long hold** — approximately 1200 ms
* **Very long hold** — approximately 2800 ms

Button input has a **28 ms debounce period** to prevent electrical switch bounce from being interpreted as multiple presses.

A tap behaves slightly differently depending on whether the user is already inside a menu. Outside a menu, the firmware waits to determine whether the tap becomes a double tap. Inside a menu, the tap is immediately returned as a menu-selection gesture.

This allows a single physical button to provide multiple controls without requiring additional switches.

### User Interface State Machine

V1.8 is organized around several UI states:

* **Mode** — normal lighting operation
* **Main Menu**
* **Games Menu**
* **Settings Menu**
* **Brightness**
* **Speed**
* **Info**
* **In Game**
* **Sleep**

The firmware uses these states to determine how each gesture should behave. For example, a tap in the main menu moves the cursor, while a tap during normal operation is passed to the currently active lighting mode.

The menus also remember their previous cursor positions. This means that returning to the Games or Settings menu does not automatically reset the cursor to the first item.

### Main Menu

The main menu contains six positions:

1. **Status**
2. **Fireplace**
3. **Lightsaber**
4. **Rainbow**
5. **Games**
6. **Settings**

The first four menu positions use the cactus-green menu color. Games is orange and Settings is blue.

The currently selected position is displayed in **hot pink** and uses a breathing brightness animation. This makes the cursor visually distinct from the menu items.

A tap moves forward through the menu, while a double tap moves backward. A hold selects the highlighted item.

A long hold from the menu returns directly to the default idle state.

### Default Status Mode

The default idle display is a **blue bouncing ball**.

The main blue LED moves from LED 0 through LED 9 and then back again. Two additional LEDs are used behind the main LED to create a short blue trail, giving the movement the appearance of a moving light rather than a single LED simply turning on and off.

The movement rate is affected by the global speed setting.

When the system starts, Status mode is selected automatically. Returning from shutdown, timing out of a menu, or using the appropriate long-hold action also returns the system to this default status mode.

### Fireplace Mode

The Fireplace mode is one of the major V1.8 upgrades. Instead of simply displaying a fixed red/orange pattern, it maintains a **10-element heat model** representing the temperature of each LED position.

When Fireplace mode starts, every LED receives an initial amount of heat. A moving heat source then injects energy into the model.

The source continuously chooses new target positions, moves toward them, and can receive a small velocity from the random system. The source is constrained to the middle portion of the LED array so the fire remains concentrated around its base rather than behaving like an unconstrained moving light.

The fire simulation contains several layers:

* **Heat cooling** — every LED gradually loses heat.
* **Source injection** — the moving fire source adds new heat.
* **Neighbor injection** — heat is also added around the source.
* **Diffusion** — neighboring LED temperatures are blended together.
* **Rising heat** — heat is biased upward to imitate hot air and flames rising.
* **Random flame licks** — occasional additional bursts appear above the source.
* **High sparks** — rare hotter events appear higher in the fire.
* **Base glow** — LED 0 is prevented from becoming completely cold.
* **Noise modulation** — FastLED noise changes the intensity of individual LEDs.
* **Flare effects** — button interactions can temporarily inject significantly more heat.

The color of each LED is calculated from its simulated heat. The palette progresses through several stages:

**dark red → red → orange → yellow → white-hot**

Low heat produces a dark red glow, while the hottest portions transition toward a pale white-hot color.

A normal tap creates a stronger flare. A double tap creates an even stronger flare and also changes the fire's source target and velocity. This gives the Fireplace mode an interactive component rather than making it a completely passive animation.

### Lightsaber Mode

Lightsaber mode generates a colored animated blade across the LED array.

Every time a blade is launched, V1.8 selects a new hue using the entropy system. The code explicitly prevents the newly selected hue from being identical to the previous blade's hue, so two consecutive launches cannot have exactly the same hue.

Each launch also randomly determines:

* Blade saturation
* Animation pattern
* Instability
* Ignition state

The blade starts with zero power and **extends** outward. The power value controls the visible blade length, allowing the blade to grow into position rather than appearing at full length immediately.

The blade has several animation patterns that can change its brightness, flicker, pulsing behavior, and instability. The first LEDs are also blended toward white to create a brighter blade core.

During the first 170 ms of ignition, LED 0 receives an additional white flash to emphasize the blade starting up.

A tap stops an extending blade. Once stopped, another tap launches a new blade. A double tap immediately launches another blade. A hold creates a short **clash effect**, during which portions of the blade are repeatedly brightened toward white.

Additional random white flashes are occasionally placed along the blade to make it look less static.

### Rainbow Mode

Rainbow mode continuously generates a rainbow across all ten LEDs using FastLED's `fill_rainbow()` function.

The rainbow has its own speed value, and the global speed setting also affects how quickly the hue moves.

A tap cycles through ten rainbow animation speeds.

A double tap reverses the direction of the rainbow.

When Rainbow mode is entered, the starting hue is randomized so it does not always begin at exactly the same point in the color spectrum.

### Fireball Game

V1.8 also contains a complete LED-based game called **Fireball**.

The game uses LED 0 as the target. The player waits for a fireball traveling from the opposite end of the LED array to reach that target and presses the button at the correct moment.

The game starts with:

* **3 lives**
* **115 ms fireball movement interval**
* **10 ms speed increase per successful catch**
* **30 ms minimum movement interval**

The fireball begins at LED 9 and travels toward LED 0.

The visual fireball consists of:

* A white leading LED
* A hot/orange section behind it
* A red trailing section

This creates a moving projectile rather than simply moving a single colored LED.

When the player taps while the fireball is exactly on LED 0, the catch is successful. The score increases by one and the fireball becomes faster. The game then produces a short expanding blast animation before displaying the score.

The score is displayed using the ten LEDs as a **binary display**. Each LED represents one binary bit, with the cactus-green LEDs showing the active bits.

If the player misses, one life is removed. The game briefly flashes red and then immediately launches another fireball if lives remain.

After all three lives are lost, the game enters the Game Over state. The LEDs display a pulsing red background while the binary score remains visible in cactus green.

A tap during Game Over starts a new game. A long hold while playing exits the game and returns to the Games menu, with the Games cursor remaining on Fireball.

### Settings Menu

The Settings menu contains six positions:

1. **Back**
2. **Brightness**
3. **Speed**
4. **Info**
5. **BOOTSEL**
6. **Shutdown**

The same one-button navigation system is used here: tap moves forward, double tap moves backward, and hold selects the current option.

### Brightness Control

Brightness provides ten predefined levels:

`6, 12, 20, 30, 42, 56, 72, 88, 106, 125`

The current level is represented across the ten LEDs. LEDs up to the selected position are hot pink, while the remaining LEDs are cactus green.

A tap moves to the next brightness level. A double tap moves to the previous level.

The selected value is immediately applied to FastLED.

The firmware also enforces a hard minimum brightness of **6**, meaning the brightness cannot be reduced below that value through the brightness setting.

### Speed Control

Speed also provides ten predefined values:

`40, 55, 70, 85, 100, 120, 145, 170, 200, 240`

The current speed level is displayed using the same ten-LED bar-style interface used for brightness.

A tap moves forward through the available speeds, while a double tap moves backward.

The global speed value is then used by the individual animations to change their timing or movement rate.

### Information Screen

The Info section contains three pages.

The first page displays all ten LEDs in cactus green with LED 0 highlighted hot pink.

The second page displays a changing number of illuminated LEDs based on the current time, creating a simple animated information display.

The third page displays the ten LEDs in cactus green.

A tap advances through the three pages and a double tap moves backward. A hold returns to the Settings menu.

### Entropy and Randomness

V1.8 contains a dedicated `EntropyService` rather than relying entirely on a fixed pseudo-random sequence.

The entropy service reads the four analog inputs and the additional digital entropy input, combines those readings with the microsecond timer, and repeatedly mixes the resulting value using XOR and bit-shift operations.

The service can then provide:

* 32-bit random values
* 8-bit random values
* Random values within a specified range
* Random true/false decisions

The entropy system is initialized by performing **300 mixing operations** before normal operation begins.

During normal operation, the entropy service is updated continuously.

This randomness is used throughout the firmware, including the Fireplace movement and flame behavior, Lightsaber colors and patterns, Rainbow starting position, game effects, and other animation variations.

### Menu Timeout

V1.8 includes an automatic menu timeout of **16 seconds**.

Whenever the user interacts with a menu, the firmware updates the menu activity timer. If the user stops interacting with the menu for more than 16 seconds, the firmware automatically leaves the menu and returns to the default blue Status mode.

Returning to idle resets the main mode to Status and resets the main cursor to the first position.

### Very-Long-Hold Function

A very long button hold is treated as a global escape command.

When the button has been held for approximately 2.8 seconds, the firmware briefly fills all ten LEDs with hot pink and then forces the interface back into the default Status mode.

This provides a general way to escape from the current interface state.

### Shutdown Mode

Shutdown is implemented as a software sleep state.

When Shutdown is selected, the firmware:

1. Changes the UI state to `UI_SLEEP`.
2. Clears the LEDs.
3. Sets FastLED brightness to zero.
4. Shows the blank LED state.
5. Waits for the button to be released.

Once the button has been released, holding it again for approximately **1.5 seconds** wakes the interface.

On wake-up, the firmware restores the configured brightness, returns to Status mode, resets the main cursor, and starts the blue bouncing-ball animation again.

While asleep, the LEDs remain off and the normal rendering loop is bypassed.

### USB BOOTSEL Mode

The Settings menu also provides a BOOTSEL option.

When selected, V1.8 turns the LEDs off, sets FastLED brightness to zero, waits briefly, and calls:

`reset_usb_boot(0, 0)`

This restarts the Raspberry Pi controller into USB boot mode so firmware can be loaded through the RP2354A's bootloader interface.

### Startup Sequence

At startup, V1.8 initializes FastLED for the ten WS2812B LEDs, disables dithering, applies the configured brightness, and clears the LEDs.

The entropy and button services are then initialized.

The firmware performs a short startup sweep across all ten LEDs. Each LED is assigned a different HSV hue as the sweep progresses, with the LEDs being displayed individually with an 18 ms delay.

After the sweep, the firmware waits briefly, clears the LEDs, and enters the default Status mode.

This means the board has a defined visual startup sequence before entering normal operation.

### Main Program Loop

The V1.8 main loop operates continuously.

On every iteration it:

1. Checks whether the system is in Shutdown mode.
2. Updates the entropy service.
3. Checks the menu timeout.
4. Polls the button for a gesture.
5. Passes the gesture to the appropriate UI or application.
6. Updates the LED animation when the 16 ms frame interval has elapsed.
7. Renders the current UI state.
8. Sends the resulting LED data to the WS2812B LEDs with `FastLED.show()`.

The normal renderer therefore operates at a target interval of approximately **16 ms per frame**, or about **62.5 frames per second**, while individual animations control their own movement rates internally.

Shutdown mode is handled separately so that normal animation rendering does not continue while the system is asleep.

### V1.8 Overall

In V1.8, the DEV-Brain is more than an LED controller. It is a complete single-button user interface running on the Raspberry Pi RP2354A.

The firmware combines:

* Ten individually controlled WS2812B RGB LEDs
* A single multi-gesture button interface
* Persistent menu cursors
* Four main lighting modes
* A simulated Fireplace effect
* An interactive Lightsaber effect
* A configurable Rainbow effect
* The Fireball LED game
* Adjustable brightness
* Adjustable animation speed
* An information interface
* Automatic menu timeout
* Software shutdown and wake-up
* USB BOOTSEL entry
* Hardware-derived entropy for randomized effects
* A centralized UI state machine
* A 16 ms animation/rendering cycle

The result is a self-contained control system for the Lamoka1-DEV-Brain where the same ten LEDs function both as the primary visual output and as the user interface itself. V1.8 uses those LEDs not only for animations, but also for menu selection, settings indicators, game graphics, binary score display, status information, and system-state feedback.
