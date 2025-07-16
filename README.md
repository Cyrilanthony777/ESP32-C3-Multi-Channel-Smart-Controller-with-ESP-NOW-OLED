# ESP32-C3 Multi-Channel Smart Controller with ESP-NOW & OLED

This project presents an ESP32-C3 based smart controller capable of managing multiple power outputs (5V USB lines and high-voltage AC/DC lines) and automating timed sequences. It utilizes ESP-NOW for robust wireless communication, allowing for remote control and status monitoring, and an OLED display for local visual feedback.

The "USB" outputs are designed to enable/disable LM2596 5V fixed buck converters, providing switched 5V power, while the "HV" outputs are connected to optocouplers driving MOSFETs for controlling higher voltage loads (e.g., mains AC or higher DC voltage devices).

## Features

* **ESP-NOW Communication:** Securely receive commands and broadcast status updates to other ESP-NOW enabled devices.
* **Authentication:** Employs a pre-shared authentication token for secure command reception, preventing unauthorized access.
* **Multi-Channel Power Control:**
    * **3 x 5V Switched Outputs (USB1-USB3):** Control the enable pins of LM2596 5V fixed buck converters to switch 5V power on/off for USB-powered devices.
    * **3 x High-Voltage Outputs (HV1-HV3):** Control optocouplers connected to MOSFETs for switching higher voltage loads (e.g., pumps, lights, fans).
* **Automated Timed Sequences:** Configure independent ON/OFF cycles for a pump, light, and air system based on user-defined durations in seconds. Ideal for applications like hydroponics, aquaponics, or grow tents.
* **OLED Display (128x64 SSD1306):** Provides real-time status indication, showing whether the automated sequences are "Running" or "Stopped".
* **Manual Start/Stop Inputs:** Dedicated input pins allow for physical buttons/switches to manually initiate or halt the automated sequences.

## Hardware Requirements

* **ESP32-C3 Development Board:** An ESP32-C3 board compatible with the Arduino ESP32 core (e.g., ESP32-C3 DevKitM-1).
* **OLED Display:** 128x64 I2C OLED display (e.g., Adafruit SSD1306 breakout).
* **LM2596 5V Fixed Buck Converters:** 3 units. Ensure they have an `ENABLE` pin.
* **MOSFET Modules with Optocouplers:** 3 units. These typically include the optocoupler and suitable driving circuitry for the MOSFET.
* **Push Buttons/Switches:** Two momentary push buttons or toggle switches for `IP1` (Start) and `IP2` (Stop) functionality.
* **Jumper Wires:** For all connections.
* **Breadboard (Optional):** For initial prototyping.
* **Power Supply:** Appropriate power supplies for the ESP32-C3, the LM2596 converters' input, and the high-voltage loads.

## Software Requirements

* **Arduino IDE:** Version 1.8.x or newer is recommended.
* **ESP32 Boards Manager:** Install `esp32` platform version **2.0.3** or a compatible version via the Arduino IDE Boards Manager.
    * Navigate to `File > Preferences`, and add `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json` to "Additional Boards Manager URLs".
    * Then, go to `Tools > Board > Boards Manager`, search for "esp32", and install version `2.0.3`.
* **Adafruit GFX Library:** Install via Arduino Library Manager (`Sketch > Include Library > Manage Libraries...`).
* **Adafruit SSD1306 Library:** Install via Arduino Library Manager.

## Pin Configuration

| Component                 | ESP32-C3 Pin | Notes                                                                      |
| :------------------------ | :----------- | :------------------------------------------------------------------------- |
| OLED SDA                  | Default I2C  | Check your specific ESP32-C3 board's pinout for default I2C pins.          |
| OLED SCL                  | Default I2C  |                                                                            |
| **5V Switched Outputs** |              | Connected to the `ENABLE` pin of LM2596 buck converters (active-low).      |
| USB1                      | `GPIO8`      |                                                                            |
| USB2                      | `GPIO9`      |                                                                            |
| USB3                      | `GPIO10`     |                                                                            |
| **High-Voltage Outputs** |              | Connected to the input of optocouplers driving MOSFETs.                    |
| HV1                       | `GPIO4`      |                                                                            |
| HV2                       | `GPIO3`      |                                                                            |
| HV3                       | `GPIO5`      |                                                                            |
| **Input Pins** |              | Connect buttons/switches to GND to trigger (internal pull-up assumed).     |
| IP1 (Start Automation)    | `GPIO2`      | Pulling this pin LOW starts the automated sequence.                        |
| IP2 (Stop Automation)     | `GPIO1`      | Pulling this pin LOW stops the automated sequence.                         |

**Important Note on USB Output Logic:** The `USB1`, `USB2`, `USB3` pins are configured as `OUTPUT_OPEN_DRAIN`. This means they will actively pull the line LOW, and when set to HIGH, they will essentially "float" (require an external pull-up resistor or for the LM2596 enable pin to have an internal pull-up). Most LM2596 enable pins are active-low, meaning a LOW signal enables the converter, and a HIGH/floating signal disables it. The code's `digitalWrite(USBx, LOW)` will enable the converter, and `digitalWrite(USBx, HIGH)` will disable it.

## Getting Started

1.  **Install Arduino IDE and ESP32 Board Support:** Follow the instructions in the "Software Requirements" section.
2.  **Install Libraries:** Install `Adafruit GFX` and `Adafruit SSD1306` libraries through the Arduino Library Manager.
3.  **Hardware Connections:**
    * **OLED:** Connect SDA and SCL to the default I2C pins on your ESP32-C3 board.
    * **LM2596 Converters:** Connect `GPIO8`, `GPIO9`, `GPIO10` to the `ENABLE` pins of your respective LM2596 5V fixed buck converters.
    * **MOSFET Optocouplers:** Connect `GPIO4`, `GPIO3`, `GPIO5` to the input pins of your MOSFET optocoupler modules.
    * **Input Buttons:** Connect one side of each button to `GPIO2` (IP1) and `GPIO1` (IP2) respectively. Connect the other side of each button to `GND`. The code assumes these pins are pulled HIGH internally when the button is not pressed, and LOW when pressed.
4.  **Security Precaution:** **CRITICAL:** Change the `AUTH_TOKEN` in the code to a strong, unique value. This token is essential for securing your ESP-NOW commands.

    ```cpp
    const char* AUTH_TOKEN = "Your_Very_Strong_Secret_Token_Here"; // CHANGE THIS!
    ```

5.  **Upload the Code:**
    * Open the provided `.ino` file in the Arduino IDE.
    * Select your ESP32-C3 board (`Tools > Board > ESP32 Arduino > ESP32C3 Dev Module` or similar).
    * Select the correct COM Port (`Tools > Port`).
    * Click the "Upload" button.
6.  **Monitor Serial Output:** Open the Serial Monitor (Baud Rate: 9600) to observe initialization messages, received commands, and broadcasted status updates.

## Usage

### ESP-NOW Communication

This device acts as an ESP-NOW peer, capable of receiving commands from an authenticated master and broadcasting its current state to other peers.

* **Incoming Commands (`OnDataRecv`):**
    Commands are received as `incoming_message` structures, containing an `AUTH_TOKEN` and a `command` byte array.

    **Command Structure (Hexadecimal Bytes):**
    The `command` field is a `char[16]`. The current implementation interprets the first few bytes for specific actions:

    * **`0xAA 0xCC 0xDA <PIN_INDEX> <ACTION>` (Direct Pin Control - only when `running` is false):**
        * `<PIN_INDEX>`:
            * `0x00`: USB1 (GPIO8)
            * `0x01`: USB2 (GPIO9)
            * `0x02`: USB3 (GPIO10)
            * `0x03`: HV1 (GPIO4)
            * `0x04`: HV2 (GPIO3)
            * `0x05`: HV3 (GPIO5)
        * `<ACTION>`:
            * `0xAF`: Turn ON (USB outputs go LOW to enable, HV outputs go HIGH to activate)
            * `0xA0`: Turn OFF (USB outputs go HIGH to disable, HV outputs go LOW to deactivate)

    * **`0xAA 0xCC 0xDB` (Start Automated Sequence):** Sets the `running` flag to `true`, initiating the timed operations.
    * **`0xAA 0xCC 0xDC` (Stop Automated Sequence):** **Note:** In the provided code, this command also sets `running = true`. This appears to be a logical error and should ideally set `running = false` to stop the sequence. You might want to correct this in your code to `running = false;` if you intend for this command to stop the automation.
    * **`0xAA 0xCC 0xCC` (Reset All & Stop Automation):** This command stops all automated sequences, sets all USB outputs to `HIGH` (disabled) and HV outputs to `LOW` (disabled), and resets all internal timing flags and variables.

* **Broadcasting Status (`broadcastData`):**
    The device periodically broadcasts a `broadcast_message` containing the current states of all USB and HV outputs, as well as the configured `pumpOn`, `pumpOff`, `lightOn`, `lightOff`, `airOn`, `airOff` values, its MAC address, and a timestamp. This allows other ESP-NOW devices to monitor its operational status.

### Automated Timed Sequences

The controller can execute pre-configured timed sequences for a pump, light, and air system.

* **Configuration:** The ON and OFF durations for `pump`, `light`, and `air` are defined in `seconds` at the top of the sketch. Adjust these values according to your application's requirements.

    ```cpp
    unsigned long pumpOn = 45, pumpOff = 180;   // Pump: 45 seconds ON, 180 seconds OFF
    unsigned long lightOn = 43200, lightOff = 43200; // Light: 12 hours ON, 12 hours OFF (43200 seconds = 12 hours)
    unsigned long airOn = 60, airOff = 500;     // Air: 60 seconds ON, 500 seconds OFF
    ```

* **Starting/Stopping Automation:**
    * **Manual Control:**
        * Pressing the button connected to `IP1` will set `running = true`, initiating the automated sequences.
        * Pressing the button connected to `IP2` will set `running = false`, stopping the automated sequences and resetting outputs to their off state.
    * **ESP-NOW Control:**
        * Sending the `0xAA 0xCC 0xDB` command starts the automated sequence.
        * Sending the `0xAA 0xCC 0xCC` command stops the automated sequence and performs a full reset.

* **Initial Sequence on Start:** When the `running` flag transitions to `true` and the system is not yet `started` (i.e., first time running after a stop or power-up), the following actions occur to initiate the cycle:
    1.  `USB3` (Light) is set `LOW` (enabled).
    2.  `USB2` (Air) is set `LOW` (enabled).
    3.  `HV1` (Pump) is set `HIGH` (enabled).
    4.  All internal timers (`t_pump`, `t_light`, `t_air`) are set for their initial `ON` duration.

## Troubleshooting

* **OLED Display Issues:** If the display doesn't show anything, double-check your I2C wiring (SDA, SCL, VCC, GND) and ensure the `SCREEN_ADDRESS` (0x3C) is correct for your specific SSD1306 module. Some modules use 0x3D.
* **ESP-NOW Initialization Error:** Ensure your ESP32 board is correctly selected in the Arduino IDE and that the ESP32 core version 2.0.3 is installed. Check `Tools > Board > Get Board Info`.
* **Relay/Output Not Switching:**
    * Verify your wiring to the LM2596 enable pins and optocoupler inputs.
    * Confirm the active state of your specific LM2596 enable pin (most are active-low, which matches the `OUTPUT_OPEN_DRAIN` setup).
    * Ensure your MOSFET optocouplers are correctly wired and the MOSFETs themselves are functioning.
    * Check if `running` is `true` for automated sequences, or if you're sending direct commands while `running` is `false`. Direct control (`0xDA`) only works when automation is inactive.
* **No ESP-NOW Reception/Transmission:** Ensure both the sender and receiver ESP-NOW devices are using the same `AUTH_TOKEN`. Check the serial output for "Invalid token" messages.
* **Automated Sequence Malfunction:** Verify your `pumpOn`, `pumpOff`, etc., values are in seconds as expected. Double-check the logic within the `callback_1s()` function. Pay close attention to the `0xDC` command behavior, as noted in the usage section.

## Contributing

Contributions, issues, and feature requests are welcome! Feel free to open an issue or submit a pull request.

## License

This project is open-source and available under the [MIT License](LICENSE).