# KWD Tracer Configuration Guide

This document explains all configuration settings used in the KWD Tracer split keyboard.

## Configuration Files Overview

- **`config/kwdtracer.conf`** - Shared configuration applied to both sides
- **`boards/shields/kwdtracer/kwdtracer_left.conf`** - Left side (Central) specific configuration
- **`boards/shields/kwdtracer/kwdtracer_right.conf`** - Right side (Peripheral) specific configuration

## Shared Configuration (`config/kwdtracer.conf`)

### Debounce Settings
```
CONFIG_ZMK_KSCAN_DEBOUNCE_PRESS_MS=10
CONFIG_ZMK_KSCAN_DEBOUNCE_RELEASE_MS=10
```
**Status**: Currently commented out (using default values)  
**Purpose**: Increases debounce time for more reliable key detection. Enable if experiencing key chatter or double-presses.

### Split Keyboard BLE Settings (Mandatory)
```
CONFIG_ZMK_SPLIT_BLE=y
```
**Purpose**: Enables Bluetooth-based split keyboard communication. This is required for wireless split functionality.

### Battery Level Monitoring (Optional)
```
CONFIG_ZMK_BATTERY_REPORTING=y
CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING=y
CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_PROXY=y
```
**Purpose**: 
- Enables battery level reporting to host devices
- Allows central to fetch peripheral's battery level
- Proxies peripheral battery level to host through central

### Connection Reliability Improvements (Optional)
```
CONFIG_ZMK_BLE_EXPERIMENTAL_CONN=y
CONFIG_ZMK_BLE_EXPERIMENTAL_SEC=y
```
**Purpose**: Enables experimental BLE features for improved connection stability and security.

### Bluetooth Authentication Settings
```
CONFIG_ZMK_BLE_CLEAR_BONDS_ON_START=n
```
**Purpose**: Prevents automatic clearing of Bluetooth bonds on startup. Set to `y` if you need to clear pairings on every boot.

```
CONFIG_BT_CTLR_TX_PWR_PLUS_8=y
```
**Purpose**: Increases Bluetooth transmission power to +8dBm for stronger, more reliable connections between keyboard halves and with host devices.

### External Power Control
```
CONFIG_ZMK_EXT_POWER=y
```
**Purpose**: Enables external power control (for LED on pin 20). Required for controlling underglow LEDs or other external peripherals.

### Pointing Device Support
```
CONFIG_ZMK_POINTING=y
CONFIG_ZMK_POINTING_SMOOTH_SCROLLING=y
```
**Purpose**: Enables pointing device support (trackball, trackpad) with smooth scrolling feature.

### USB Logging
```
CONFIG_ZMK_USB_LOGGING=y
```
**Status**: Currently commented out  
**Purpose**: Enables USB logging for debugging. Disable for production to save power and improve performance.

## Left Side Configuration (`kwdtracer_left.conf`)

### External Power Control
```
CONFIG_ZMK_EXT_POWER=y
```
**Purpose**: Enables external power control on the central (left) side.

### Split Central Role
```
CONFIG_ZMK_SPLIT_ROLE_CENTRAL=y
```
**Purpose**: Explicitly sets this side as the central (main) controller. The central handles all HID communication with host devices and runs the keymap logic.

### BLE Settings
```
CONFIG_ZMK_BLE_EXPERIMENTAL_CONN=y
```
**Purpose**: Enables experimental BLE connection improvements for more stable communication with peripherals and host devices.

### Sleep/Wake Functionality
```
CONFIG_ZMK_SLEEP=y
```
**Purpose**: Enables sleep mode for power conservation. The keyboard can wake from deep sleep on key press.

## Right Side Configuration (`kwdtracer_right.conf`)

### External Power Control
```
CONFIG_ZMK_EXT_POWER=y
```
**Purpose**: Enables the right side to control its own external power independently. This allows the peripheral to manage LEDs and other peripherals without relying on the central.

### Split Peripheral Role
```
CONFIG_ZMK_SPLIT_ROLE_PERIPHERAL=y
```
**Purpose**: Explicitly sets this side as a peripheral. Peripherals send key events to the central but don't directly communicate with host devices.

### Independent Input Processing
```
CONFIG_ZMK_SPLIT_PERIPHERAL_HID_INDICATORS=y
```
**Purpose**: Allows the peripheral to process and respond to inputs independently. This is critical for handling system commands (reset, bootloader, ext_power) when disconnected from the central.

**Key Benefit**: Without this setting, the peripheral becomes unresponsive when disconnected and requires manual power cycling.

### Sleep/Wake Functionality
```
CONFIG_ZMK_SLEEP=y
```
**Purpose**: Enables sleep mode and wake-on-keypress functionality for the peripheral.

### BLE Connection Settings
```
CONFIG_ZMK_BLE_EXPERIMENTAL_CONN=y
```
**Purpose**: Improves BLE connection stability and helps maintain responsiveness during connection issues or when temporarily disconnected.

## Independent Operation

With the current configuration, both keyboard halves can operate independently:

### Left Side (Central)
- ✅ Fully functional when right side is disconnected
- ✅ Can reset itself via keymap
- ✅ Can enter bootloader mode via keymap
- ✅ Controls its own external power
- ✅ Communicates with host devices

### Right Side (Peripheral)
- ✅ Can reset itself via keymap (even when disconnected)
- ✅ Can enter bootloader mode via keymap (even when disconnected)
- ✅ Controls its own external power (even when disconnected)
- ✅ Wakes from sleep on key press
- ✅ No manual power cycling required when disconnected

## Troubleshooting

### If peripheral becomes unresponsive
1. Access system layer (Layer 6) on the peripheral
2. Press the reset key
3. The peripheral should reset without manual intervention

### If split halves won't pair
1. Flash settings_reset firmware to both sides
2. Re-flash normal firmware to both sides
3. Reset both sides simultaneously
4. They should auto-pair within a few seconds

### If connection is weak
- Ensure `CONFIG_BT_CTLR_TX_PWR_PLUS_8=y` is set
- Check for metal enclosures or obstacles
- Verify both sides are properly powered

### If external power doesn't work
- Verify GPIO 20 is properly wired
- Check the init-delay-ms in overlay files (currently 50ms)
- Ensure `CONFIG_ZMK_EXT_POWER=y` is set in relevant conf files

## Configuration Priority

Configuration settings are applied in this order (later overrides earlier):
1. Board defaults
2. Shield defaults
3. `config/kwdtracer.conf` (shared)
4. `boards/shields/kwdtracer/kwdtracer_left.conf` or `kwdtracer_right.conf` (side-specific)

This means side-specific settings will override shared settings if there's a conflict.

## Related Documentation

- [SPLIT_INDEPENDENCE.md](SPLIT_INDEPENDENCE.md) - Detailed explanation of split independence features
- [ZMK Split Keyboards](https://zmk.dev/docs/features/split-keyboards)
- [ZMK Configuration](https://zmk.dev/docs/config)
- [ZMK External Power](https://zmk.dev/docs/features/underglow)

---

**Last Updated**: October 25, 2025

