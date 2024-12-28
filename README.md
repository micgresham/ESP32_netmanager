# Network Configuration and Management

## Overview

This document provides detailed information about the classes and structures used for network configuration and management. It covers the `NetworkConfig`, `SoftAPConfig`, `WiFiNetwork`, `ScanResult`, and `NetworkManager` classes.

---

## NetworkConfig Class

### Overview
The `NetworkConfig` class is responsible for managing network configuration settings, including WiFi credentials and IP configurations.

### Syntax

```cpp
class NetworkConfig
```

### Members

#### Public Constants
- **`MAX_WIFI_CREDENTIALS`**  
  Maximum number of WiFi credentials that can be stored.  
  *Type:* `int`  

#### Public Types
- **`WiFiCredential`**  
  Structure to store WiFi credentials.  
  *Members:*
  - `char ssid[32]`: WiFi SSID
  - `char password[64]`: WiFi Password
  - `wifi_auth_mode_t authMode`: Authentication mode

#### Public Methods
- **`NetworkConfig()`**  
  Initializes a new instance of the `NetworkConfig` class with default values.

### Example

```cpp
NetworkConfig config;
```

---

## SoftAPConfig Class

### Overview
The `SoftAPConfig` class holds the configuration settings for a Soft Access Point (AP).

### Syntax

```cpp
class SoftAPConfig
```

### Members

#### Public Data Members
- **`char ssid[32]`**  
  SSID of the Soft AP.  
  *Type:* `char[32]`  

- **`char password[64]`**  
  Password of the Soft AP.  
  *Type:* `char[64]`  

- **`uint8_t channel`**  
  WiFi channel.  
  *Type:* `uint8_t`  

- **`wifi_auth_mode_t authMode`**  
  Authentication mode.  
  *Type:* `wifi_auth_mode_t`  

- **`uint8_t maxConnections`**  
  Maximum number of simultaneous connections.  
  *Type:* `uint8_t`  

- **`bool hidden`**  
  Whether the SSID is hidden.  
  *Type:* `bool`  

#### Public Methods
- **`SoftAPConfig()`**  
  Initializes a new instance of the `SoftAPConfig` class with default values.

### Example

```cpp
SoftAPConfig apConfig;
```

---

## WiFiNetwork Structure

### Overview
The `WiFiNetwork` structure holds information about a WiFi network.

### Syntax

```cpp
struct WiFiNetwork
```

### Members

#### Data Members
- **`char ssid[33]`**  
  SSID of the WiFi network.  
  *Type:* `char[33]`  

- **`int32_t rssi`**  
  Signal strength of the WiFi network.  
  *Type:* `int32_t`  

- **`wifi_auth_mode_t authMode`**  
  Authentication mode of the WiFi network.  
  *Type:* `wifi_auth_mode_t`  

- **`bool isHidden`**  
  Whether the WiFi network is hidden.  
  *Type:* `bool`  

### Example

```cpp
WiFiNetwork network;
```

---

## ScanResult Structure

### Overview
The `ScanResult` structure holds the results of a WiFi network scan.

### Syntax

```cpp
struct ScanResult
```

### Members

#### Data Members
- **`WiFiNetwork* networks`**  
  Array of WiFi networks found during the scan.  
  *Type:* `WiFiNetwork*`  

- **`int count`**  
  Number of valid networks found.  
  *Type:* `int`  

#### Methods
- **`ScanResult()`**  
  Initializes a new instance of the `ScanResult` structure with default values.

- **`~ScanResult()`**  
  Cleans up the dynamically allocated memory.

### Example

```cpp
ScanResult result;
```

---

## NetworkManager Class

### Overview
The `NetworkManager` class is responsible for managing network connections. It supports Ethernet, WiFi, and Soft AP modes.

### Syntax

```cpp
class NetworkManager
```

### Members

#### Public Types
- **`NetworkMode`**  
  Enumeration for network modes.
  - `MODE_ETHERNET`
  - `MODE_WIFI`
  - `MODE_ETHERNET_WIFI_BACKUP`
  - `MODE_WIFI_AP`

- **`NetworkState`**  
  Enumeration for network states.
  - `STATE_DISCONNECTED`
  - `STATE_SCANNING`
  - `STATE_CONNECTING`
  - `STATE_WAITING_FOR_IP`
  - `STATE_CONNECTED`
  - `STATE_CONNECTION_LOST`
  - `STATE_WRONG_PASSWORD`
  - `STATE_NO_AP_FOUND`
  - `STATE_ERROR`

#### Public Methods
- **`NetworkManager()`**  
  Initializes a new instance of the `NetworkManager` class with default values.

- **`bool hasValidWiFiConfig()`**  
  Checks if valid WiFi configuration is available.
  *Returns:* `bool`

- **`void fallbackToSoftAP()`**  
  Falls back to Soft AP mode if no network is available.

- **`bool setEthMacAddress(const byte* mac)`**  
  Sets the Ethernet MAC address.
  *Parameters:*  
  `const byte* mac` - MAC address to set.  
  *Returns:* `bool`

- **`void begin(NetworkMode mode = MODE_ETHERNET)`**  
  Begins network operation in the specified mode.
  *Parameters:*  
  `NetworkMode mode` - Network mode to begin with.

- **`void setEthernetConfig(const NetworkConfig& config)`**  
  Sets Ethernet configuration.
  *Parameters:*  
  `const NetworkConfig& config` - Ethernet configuration to set.

- **`void setWiFiConfig(const NetworkConfig& config)`**  
  Sets WiFi configuration.
  *Parameters:*  
  `const NetworkConfig& config` - WiFi configuration to set.

- **`void setSoftAPConfig(const SoftAPConfig& config)`**  
  Sets Soft AP configuration.
  *Parameters:*  
  `const SoftAPConfig& config` - Soft AP configuration to set.

- **`void setCallbacks(...)`**  
  Sets callback functions for various network events.
  *Parameters:*  
  `(void(*onConnected)(void), void(*onDisconnected)(void), void(*onError)(const char* error), void(*onDHCPTimeout)(void) = nullptr, void(*onClientConnected)(WiFiEvent_t, WiFiEventInfo_t) = nullptr, void(*onClientDisconnected)(WiFiEvent_t, WiFiEventInfo_t) = nullptr, void(*onIPAssigned)(void) = nullptr)`

- **`NetworkState getState()`**  
  Gets the current network state.
  *Returns:* `NetworkState`

- **`bool isConnected()`**  
  Checks if the network is connected.
  *Returns:* `bool`

- **`IPAddress getIP()`**  
  Gets the current IP address.
  *Returns:* `IPAddress`

- **`ScanResult scanNetworks(int32_t minRSSI = -100)`**  
  Performs a synchronous network scan.
  *Parameters:*  
  `int32_t minRSSI` - Minimum RSSI to include in results.
  *Returns:* `ScanResult`

- **`void startAsyncScan(int32_t minRSSI = -100)`**  
  Starts an asynchronous network scan.
  *Parameters:*  
  `int32_t minRSSI` - Minimum RSSI to include in results.

- **`bool getAsyncScanResult(ScanResult& result)`**  
  Gets the results of an asynchronous network scan.
  *Parameters:*  
  `ScanResult& result` - Structure to store scan results.
  *Returns:* `bool`

- **`bool isCurrentlyScanning()`**  
  Checks if a network scan is currently in progress.
  *Returns:* `bool`

- **`void update()`**  
  Updates the network manager state.

---

### Example

```cpp
#include <Arduino.h>
#include "NetworkManager.h"

NetworkManager network;

void setup() {
    Serial.begin(115200);

    NetworkConfig wifiConfig;
    strcpy(wifiConfig.credentials[0].ssid, "test1");
    strcpy(wifiConfig.credentials[0].password, "password1");
    wifiConfig.isDhcp = true;

    network.setWiFiConfig(wifiConfig);
    network.begin(NetworkManager::MODE_WIFI);

    network.setCallbacks(onNetworkConnected, onNetworkDisconnected, onNetworkError);
}

void loop() {
    network.update();
}

void onNetworkConnected() {
    Serial.println("Network connected!");
}

void onNetworkDisconnected() {
    Serial.println("Network disconnected!");
}

void onNetworkError(const char* error) {
    Serial.print("Network error: ");
    Serial.println(error);
}

