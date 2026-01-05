# Wireless Network and Bluetooth Interface Management Tool

This is a cross-platform WiFi and Bluetooth interface management tool written in C++, providing comprehensive network device management functionality.

## Project Overview

This project implements the following core features:

### WiFi Features
- **Network Scanning**: Scan available WiFi networks
- **Network Connection**: Connect to specified WiFi networks
- **Network Management**: Manage saved network configurations
- **AP Mode**: Support for creating WiFi hotspots
- **Static IP Configuration**: Support for setting static IP addresses
- **Auto-Connect**: Support for automatic network connection

### Bluetooth Features
- **Device Scanning**: Scan surrounding Bluetooth devices
- **Device Pairing**: Pair with Bluetooth devices
- **Device Connection**: Connect to paired Bluetooth devices
- **Device Management**: Manage paired and connected devices
- **Auto-Connect**: Support for automatic Bluetooth device connection

## Project Structure

The project adopts a modular design, mainly containing the following files:

```bash
Interface/
├── main.cpp          # Main program entry, contains user interface and test code
├── WifiInterface.h   # WiFi interface class header file
├── WifiInterface.cpp # WiFi interface class implementation
├── BlueInterface.h   # Bluetooth interface class header file
├── BlueInterface.cpp # Bluetooth interface class implementation
├── Makefile          # Build configuration file
└── README.md         # Project documentation file
```

## Compilation and Running

### Environment Requirements
- C++11 compatible compiler (g++ 4.8+ or clang 3.3+)
- Linux system

### Compilation Commands
```bash
# Using default compiler
make

# Using cross-compilation toolchain (for embedded devices)
make CROSS_COMPILE=aarch64-none-linux-gnu-
```

### Running the Program
```bash
./Peripheral_interface_test
```

## Usage Instructions

### WiFi Function Testing
In `main.cpp`, uncomment the following macro definition to enable WiFi testing:
```cpp
#define WIFI_TEST
```

WiFi features include:
- Scanning available networks
- Connecting/disconnecting from networks
- Managing saved networks
- Setting static IP
- Creating WiFi hotspots

### Bluetooth Function Testing
In `main.cpp`, uncomment the following macro definition to enable Bluetooth testing:
```cpp
#define BLUE_TEST
```

Bluetooth features include:
- Scanning Bluetooth devices
- Pairing/unpairing devices
- Connecting/disconnecting devices
- Managing paired devices
- Setting auto-connect

## Platform Support

- Only supports Linux platform, Windows platform is not adapted

## Important Notes

1. **Permission Requirements**: Some functions require administrator privileges to execute
2. **Hardware Dependencies**: Current interface uses rtl88xx series Bluetooth/WiFi chips for testing
3. **Platform Differences**: Function implementations may vary across different platforms
4. **Testing Mode**: Recommended to use in testing environments to avoid affecting production environments

## Development Guide

### Adding New Features
1. Add method declarations in the corresponding interface class
2. Add platform-specific implementations in the implementation file
3. Add corresponding test code in the main program

### Debugging
- Use conditional compilation to control function modules
- Test each module separately

## Contact Information

If you have any questions or suggestions, please contact us through the project Issue page.
