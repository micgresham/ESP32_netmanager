#include <esp32_netmanager.h>


void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  Serial.println("\nESP32 Network Manager Test Program");

}

// Callback functions
void onNetworkConnected() {
  Serial.println("Network connected!");
}

void onNetworkDisconnected() {
  Serial.println("Network disconnected!");
}

void onNetworkError(const char * error) {
  Serial.print("Network error: ");
  Serial.println(error);
}

void onDHCPTimeout() {
  Serial.println("DHCP timeout occurred");
}

void onClientConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == SYSTEM_EVENT_AP_STACONNECTED) {
    // Station connected to SoftAP
    Serial.printf("New client connected to AP - MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
      info.wifi_sta_connected.bssid[0], info.wifi_sta_connected.bssid[1], info.wifi_sta_connected.bssid[2],
      info.wifi_sta_connected.bssid[3], info.wifi_sta_connected.bssid[4], info.wifi_sta_connected.bssid[5]);
  }
}

void onClientDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == SYSTEM_EVENT_AP_STADISCONNECTED) {
    // Station disconnected from SoftAP
    Serial.printf("Client disconnected from AP - MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
      info.wifi_sta_disconnected.bssid[0], info.wifi_sta_disconnected.bssid[1], info.wifi_sta_disconnected.bssid[2],
      info.wifi_sta_disconnected.bssid[3], info.wifi_sta_disconnected.bssid[4], info.wifi_sta_disconnected.bssid[5]);
  }
}

void printNetworkInfo(NetworkManager & network) {
  Serial.print("IP Address: ");
  Serial.println(network.getIP());
  Serial.print("Network State: ");
  switch (network.getState()) {
  case NetworkManager::STATE_DISCONNECTED:
    Serial.println("Disconnected");
    break;
  case NetworkManager::STATE_SCANNING:
    Serial.println("Scanning");
    break;
  case NetworkManager::STATE_CONNECTING:
    Serial.println("Connecting");
    break;
  case NetworkManager::STATE_WAITING_FOR_IP:
    Serial.println("Waiting for IP");
    break;
  case NetworkManager::STATE_CONNECTED:
    Serial.println("Connected");
    break;
  case NetworkManager::STATE_CONNECTION_LOST:
    Serial.println("Connection Lost");
    break;
  case NetworkManager::STATE_WRONG_PASSWORD:
    Serial.println("Wrong Password");
    break;
  case NetworkManager::STATE_NO_AP_FOUND:
    Serial.println("No AP Found");
    break;
  case NetworkManager::STATE_ERROR:
    Serial.println("Error");
    break;
  }
}

void testWiFiScan(NetworkManager & network) {
  Serial.println("\nPerforming WiFi scan...");
  auto result = network.scanNetworks(-70); // Only show networks with signal stronger than -70 dBm

  Serial.printf("Found %d networks:\n", result.count);
  for (int i = 0; i < result.count; i++) {
    Serial.printf("%d: %s, Signal: %d dBm, Security: ",
      i + 1,
      result.networks[i].ssid,
      result.networks[i].rssi
    );

    switch (result.networks[i].authMode) {
    case WIFI_AUTH_OPEN:
      Serial.println("Open");
      break;
    case WIFI_AUTH_WEP:
      Serial.println("WEP");
      break;
    case WIFI_AUTH_WPA_PSK:
      Serial.println("WPA PSK");
      break;
    case WIFI_AUTH_WPA_WPA2_PSK:
      Serial.println("WPA/WPA PSK");
      break;
    case WIFI_AUTH_WPA2_PSK:
      Serial.println("WPA2 PSK");
      break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
      Serial.println("WPA2 ENTERPRISE");
      break;
    case WIFI_AUTH_WPA3_PSK:
      Serial.println("WPA3 PSK");
      break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
      Serial.println("WPA2/WPA3 PSK");
      break;
    default:
      Serial.println("Unknown");
    }
  }
}

void loop() {
  static NetworkManager network;
  static bool initialized = false;
  static unsigned long lastStatusPrint = 0;
  static unsigned long lastScan = 0;
  static const unsigned long STATUS_INTERVAL = 5000; // 5 seconds
  static const unsigned long SCAN_INTERVAL = 30000; // 30 seconds

  if (!initialized) {
    // Configure Primary WiFi settings
    NetworkConfig wifiConfig;
    strcpy(wifiConfig.credentials[0].ssid, "test1");
    strcpy(wifiConfig.credentials[0].password, "dsahkahsdkasdhas");
    wifiConfig.isDhcp = true;
    wifiConfig.credentials[0].authMode = WIFI_AUTH_WPA2_PSK;

    // Configure Backup WiFi settings
    NetworkConfig backupWiFiConfig;
    strcpy(wifiConfig.credentials[1].ssid, "test2");
    strcpy(wifiConfig.credentials[1].password, "sakdaksjdhaskhdsakdhkasjhd");
    wifiConfig.credentials[1].authMode = WIFI_AUTH_WPA2_PSK;

    // Configure ethernet settings if needed
    NetworkConfig ethConfig;
    ethConfig.isDhcp = true;

    byte newMac[6] = {
      0x00,
      0x1A,
      0x2B,
      0x3C,
      0x4D,
      0x5E
    };
    if (network.setEthMacAddress(newMac)) {
      Serial.println("MAC address updated successfully.");
    } else {
      Serial.println("Failed to update MAC address.");
    }

    // Set configurations
    network.setWiFiConfig(wifiConfig); 
    network.setEthernetConfig(ethConfig);

    // Set callbacks
    network.setCallbacks(
      onNetworkConnected,
      onNetworkDisconnected,
      onNetworkError,
      onDHCPTimeout,
      onClientConnected,
      onClientDisconnected
    );

    // Start network
    // Modes available:
    //    MODE_ETHERNET                 - Ethernet only
    //    MODE_WIFI                     - WiFi only
    //    MODE_ETHERNET_WIFI_BACKUP     - Ethernet with WiFi backup
    //    MODE_WIFI_AP                  - Soft AP mode
    network.begin(NetworkManager::MODE_WIFI);
    initialized = true;
  }

  // Regular updates
  network.update();

  // Print status periodically
  if (millis() - lastStatusPrint >= STATUS_INTERVAL) {
    printNetworkInfo(network);
    lastStatusPrint = millis();
  }

  // Perform periodic network scans
  if (millis() - lastScan >= SCAN_INTERVAL) {
    testWiFiScan(network);
    lastScan = millis();
  }
}