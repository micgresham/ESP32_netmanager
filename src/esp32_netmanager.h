#include <esp_wifi.h>
#include <WiFi.h>
#include <SPI.h>
#include <Ethernet.h>
#include <DNSServer.h>

// Network configuration class
class NetworkConfig {
  public: static
  const int MAX_WIFI_CREDENTIALS = 2; // Allow up to 2 sets of credentials
  struct WiFiCredential {
    char ssid[32];
    char password[64];
    wifi_auth_mode_t authMode;
  };

  WiFiCredential credentials[MAX_WIFI_CREDENTIALS];
  bool isDhcp;
  IPAddress ip;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns;

  NetworkConfig(): isDhcp(true) {
    ip = IPAddress(0, 0, 0, 0);
    gateway = IPAddress(0, 0, 0, 0);
    subnet = IPAddress(255, 255, 255, 0);
    dns = IPAddress(8, 8, 8, 8);
    for (int i = 0; i < MAX_WIFI_CREDENTIALS; ++i) {
      credentials[i].ssid[0] = '\0';
      credentials[i].password[0] = '\0';
      credentials[i].authMode = WIFI_AUTH_WPA2_PSK;
    }
  }
};

// Soft AP Configuration
class SoftAPConfig {
  public: char ssid[32];
  char password[64];
  uint8_t channel;
  wifi_auth_mode_t authMode;
  uint8_t maxConnections;
  bool hidden;

  SoftAPConfig(): channel(1),
  authMode(WIFI_AUTH_OPEN),
  maxConnections(4),
  hidden(false) {
    ssid[0] = 'ppC_noInternet';
    password[0] = '\0';
  }
};

// WiFi Network Information Structure
struct WiFiNetwork {
  char ssid[33];
  int32_t rssi;
  wifi_auth_mode_t authMode;
  bool isHidden;
};

struct ScanResult {
  WiFiNetwork * networks; // Array of WiFi networks
  int count; // Number of valid networks found
};

// Main Network Manager Class
class NetworkManager {
  public: enum NetworkMode {
    MODE_ETHERNET,
    MODE_WIFI,
    MODE_ETHERNET_WIFI_BACKUP,
    MODE_WIFI_AP
  };

  enum NetworkState {
    STATE_DISCONNECTED,
    STATE_SCANNING,
    STATE_CONNECTING,
    STATE_WAITING_FOR_IP,
    STATE_CONNECTED,
    STATE_CONNECTION_LOST,
    STATE_WRONG_PASSWORD,
    STATE_NO_AP_FOUND,
    STATE_ERROR
  };

  struct ScanResult {
    WiFiNetwork * networks;
    int count;

    ScanResult(): networks(nullptr), count(0) {}
      ~ScanResult() {
        if (networks != nullptr) {
          delete[] networks;
        }
      }
  };

  NetworkManager(): currentMode(MODE_ETHERNET),
  currentState(STATE_DISCONNECTED),
  isBackupActive(false),
  isSoftAPActive(false),
  lastWifiAttempt(0),
  isScanning(false),
  scanMinRSSI(-100),
  onConnectedCallback(nullptr),
  onDisconnectedCallback(nullptr),
  onErrorCallback(nullptr),
  onDHCPTimeoutCallback(nullptr),
  onClientConnectedCallback(nullptr),
  onClientDisconnectedCallback(nullptr),
  onIPAssignedCallback(nullptr) {}

  bool hasValidWiFiConfig() {
    for (int i = 0; i < wifiConfig.MAX_WIFI_CREDENTIALS; i++) {
      if (strlen(wifiConfig.credentials[i].ssid) > 0 &&
        (wifiConfig.credentials[i].authMode == WIFI_AUTH_OPEN || strlen(wifiConfig.credentials[i].password) > 0)) {
        return true;
      }
    }
    return false;
  }

  void fallbackToSoftAP() {
    Serial.println("Falling back to SoftAP mode");
    currentMode = MODE_WIFI_AP;
    setupSoftAP();
  }

  bool setEthMacAddress(const byte * mac) {
    if (mac == nullptr) {
      Serial.println("Error: Null MAC address provided.");
      return false;
    }

    for (int i = 0; i < 6; i++) {
      EthMacAddress[i] = mac[i];
    }

    Serial.print("Updated Ethernet MAC Address: ");
    for (int i = 0; i < 6; i++) {
      Serial.printf("%02X", EthMacAddress[i]);
      if (i < 5) Serial.print(":");
    }
    Serial.println();

    return true;
  }

  void begin(NetworkMode mode = MODE_ETHERNET) {
    currentMode = mode;
    currentState = STATE_SCANNING;

    switch (currentMode) {
    case MODE_ETHERNET:
      setupEthernet();
      break;
    case MODE_WIFI:
      setupWiFi();
      break;
    case MODE_ETHERNET_WIFI_BACKUP:
      setupEthernet();
      setupWiFiBackup();
      break;
    case MODE_WIFI_AP:
      setupSoftAP();
      break;
    }
  }

  void setEthernetConfig(const NetworkConfig & config) {
    ethConfig = config;
  }

  void setWiFiConfig(const NetworkConfig & config) {
    wifiConfig = config;
  }

  void setSoftAPConfig(const SoftAPConfig & config) {
    apConfig = config;
  }

  void setCallbacks(void( * onConnected)(void),
    void( * onDisconnected)(void),
    void( * onError)(const char * error),
    void( * onDHCPTimeout)(void) = nullptr,
    void( * onClientConnected)(WiFiEvent_t, WiFiEventInfo_t) = nullptr,
    void( * onClientDisconnected)(WiFiEvent_t, WiFiEventInfo_t) = nullptr,
    void( * onIPAssigned)(void) = nullptr) {
    onConnectedCallback = onConnected;
    onDisconnectedCallback = onDisconnected;
    onErrorCallback = onError;
    onDHCPTimeoutCallback = onDHCPTimeout;
    onClientConnectedCallback = onClientConnected;
    onClientDisconnectedCallback = onClientDisconnected;
    onIPAssignedCallback = onIPAssigned;
  }

  NetworkState getState() {
    return currentState;
  }

  bool isConnected() {
    return currentState == STATE_CONNECTED;
  }

  IPAddress getIP() {
    switch (currentMode) {
    case MODE_WIFI:
      return WiFi.localIP();
    case MODE_ETHERNET_WIFI_BACKUP:
      return WiFi.localIP();
    case MODE_ETHERNET:
      return Ethernet.localIP();
    case MODE_WIFI_AP:
      return WiFi.softAPIP();
    default:
      return IPAddress(0, 0, 0, 0);
    }
  }

  // Synchronous network scan
  ScanResult scanNetworks(int32_t minRSSI = -100) {
    WiFi.mode(WIFI_STA); // Set WiFi mode to Station (STA)
    int foundNetworks = WiFi.scanNetworks(); // Start network scan

    int validNetworks = 0;
    for (int i = 0; i < foundNetworks; i++) {
      if (WiFi.RSSI(i) >= minRSSI) {
        validNetworks++; // Count valid networks based on RSSI
      }
    }

    // Prepare the result structure
    ScanResult result;
    result.networks = new WiFiNetwork[validNetworks];
    result.count = validNetworks;

    int index = 0;
    for (int i = 0; i < foundNetworks && index < validNetworks; i++) {
      if (WiFi.RSSI(i) >= minRSSI) {
        // Copy network information to result
        strncpy(result.networks[index].ssid, WiFi.SSID(i).c_str(), 32);
        result.networks[index].ssid[32] = '\0'; // Ensure null termination
        result.networks[index].rssi = WiFi.RSSI(i);
        result.networks[index].authMode = WiFi.encryptionType(i);

        // Detect hidden networks by checking if the SSID is empty
        result.networks[index].isHidden = WiFi.SSID(i).length() == 0;

        index++;
      }
    }

    WiFi.scanDelete(); // Clean up scan data
    return result;
  }

  // Asynchronous scan start
  void startAsyncScan(int32_t minRSSI = -100) {
    scanMinRSSI = minRSSI;
    if (!isScanning) {
      isScanning = true;
      WiFi.mode(WIFI_STA);
      WiFi.scanNetworks(true);
    }
  }

  bool getAsyncScanResult(ScanResult & result) {
    if (!isScanning) return false;

    int16_t scanComplete = WiFi.scanComplete();
    if (scanComplete == WIFI_SCAN_RUNNING) return false; // Scan is still running

    isScanning = false;

    if (scanComplete == WIFI_SCAN_FAILED) {
      result.networks = nullptr; // No networks found
      result.count = 0;
      return true;
    }

    // Count valid networks based on RSSI threshold
    int validNetworks = 0;
    for (int i = 0; i < scanComplete; i++) {
      if (WiFi.RSSI(i) >= scanMinRSSI) {
        validNetworks++;
      }
    }

    // Allocate memory for valid networks
    result.networks = new WiFiNetwork[validNetworks];
    result.count = validNetworks;

    // Populate the result with valid networks
    int index = 0;
    for (int i = 0; i < scanComplete && index < validNetworks; i++) {
      if (WiFi.RSSI(i) >= scanMinRSSI) {
        // Copy SSID and other network data
        strncpy(result.networks[index].ssid, WiFi.SSID(i).c_str(), 32);
        result.networks[index].ssid[32] = '\0'; // Ensure null-termination
        result.networks[index].rssi = WiFi.RSSI(i);
        result.networks[index].authMode = WiFi.encryptionType(i);

        // Check if SSID is empty and assume the network is hidden
        result.networks[index].isHidden = WiFi.SSID(i).length() == 0;

        index++;
      }
    }

    // Clean up scan data
    WiFi.scanDelete();
    return true;
  }

  bool isCurrentlyScanning() {
    return isScanning;
  }

  void startWiFiScan() {
    if (!isScanning) {
      WiFi.scanNetworks(true); // Start async scan
      isScanning = true;
      Serial.println("WiFi scan started...");
    }
  }

  void update() {

    switch (currentMode) {
    case MODE_ETHERNET:
      updateEthernet();
      break;
    case MODE_WIFI:
      updateWiFi();
      break;
    case MODE_ETHERNET_WIFI_BACKUP:
      updateEthernetWithBackup();
      break;
    case MODE_WIFI_AP:
      updateSoftAP();
      break;
    }
  }

  private: NetworkMode currentMode;
  NetworkState currentState;
  NetworkConfig ethConfig;
  NetworkConfig wifiConfig;
  SoftAPConfig apConfig;
  bool isBackupActive;
  bool isSoftAPActive;
  static
  const int ETH_CS_PIN = 16;
  unsigned long lastWifiAttempt;
  bool isScanning;
  int32_t scanMinRSSI;
  static
  const unsigned long WIFI_RETRY_DELAY = 30000;
  DNSServer dnsServer;

  byte EthMacAddress[6] = {
    0xDE,
    0xAD,
    0xBE,
    0xEF,
    0xFE,
    0xED
  };

  void( * onConnectedCallback)(void);
  void( * onDisconnectedCallback)(void);
  void( * onErrorCallback)(const char * error);
  void( * onDHCPTimeoutCallback)(void);
  void( * onClientConnectedCallback)(WiFiEvent_t, WiFiEventInfo_t);
  void( * onClientDisconnectedCallback)(WiFiEvent_t, WiFiEventInfo_t);

  void( * onIPAssignedCallback)(void);

  // Setup Wi-Fi events
  void setupWiFiEvents() {
    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
      switch (event) {
      case SYSTEM_EVENT_STA_START:
        currentState = STATE_SCANNING;
        break;
      case SYSTEM_EVENT_STA_GOT_IP:
        currentState = STATE_CONNECTED;
        if (onConnectedCallback) onConnectedCallback();
        if (onIPAssignedCallback) onIPAssignedCallback();
        break;
      case SYSTEM_EVENT_STA_DISCONNECTED:
        handleWiFiDisconnection(info.wifi_sta_disconnected.reason); // Disconnection reason
        break;
      case SYSTEM_EVENT_STA_CONNECTED:
        currentState = STATE_WAITING_FOR_IP;
        break;
      default:
        break;
      }
    });
  }

  void handleWiFiDisconnection(uint8_t reason) {
    switch (reason) {
    case WIFI_REASON_AUTH_FAIL:
      currentState = STATE_WRONG_PASSWORD;
      if (onErrorCallback) onErrorCallback("Authentication failed");
      break;
    case WIFI_REASON_NO_AP_FOUND:
      currentState = STATE_NO_AP_FOUND;
      if (onErrorCallback) onErrorCallback("No AP found");
      break;
    case WIFI_REASON_ASSOC_LEAVE:
      currentState = STATE_CONNECTION_LOST;
      if (onDisconnectedCallback) onDisconnectedCallback();
      break;
    default:
      currentState = STATE_DISCONNECTED;
      if (onDisconnectedCallback) onDisconnectedCallback();
    }
  }

  void setupEthernet() {
    SPI.begin();
    Ethernet.init(ETH_CS_PIN);

    if (Ethernet.linkStatus() != LinkON) {
      if (onErrorCallback) onErrorCallback("No Ethernet link detected");
      fallbackToWiFi();
      return;
    }

    if (ethConfig.isDhcp) {
      unsigned long dhcpStart = millis();
      if (Ethernet.begin(EthMacAddress) == 0) { // Pass MAC address to begin()
        if (onErrorCallback) onErrorCallback("DHCP configuration failed");
        fallbackToWiFi();
        return;
      }
    } else {
      Ethernet.begin(EthMacAddress, ethConfig.ip, ethConfig.dns, ethConfig.gateway, ethConfig.subnet); // Pass MAC address and static IP config
    }

    delay(1000);

    if (Ethernet.linkStatus() == LinkON) {
      currentState = STATE_CONNECTED;
      if (onConnectedCallback) onConnectedCallback();
    } else {
      fallbackToWiFi();
    }
  }

  void fallbackToWiFi() {
    if (hasValidWiFiConfig()) {
      Serial.println("Falling back to WiFi mode");
      currentMode = MODE_WIFI;
      setupWiFi();
    } else {
      fallbackToSoftAP();
    }
  }

  // Setup WiFi (try the first credentials, then the second)
  void setupWiFi() {
    if (!hasValidWiFiConfig()) {
      fallbackToSoftAP();
      return;
    }

    WiFi.mode(WIFI_STA);
    setupWiFiEvents();

    // Try to connect with the first WiFi credentials
    if (tryWiFiConnection(0)) return;

    // If the first set of credentials failed, try the second set
    if (wifiConfig.credentials[1].ssid[0] != '\0') {
      if (tryWiFiConnection(1)) return;
    }

    fallbackToSoftAP();
  }

  // Attempt to connect using the specified WiFi credential index
  bool tryWiFiConnection(int index) {
    wifi_config_t conf;
    memset( & conf, 0, sizeof(conf));
    strncpy((char * ) conf.sta.ssid, wifiConfig.credentials[index].ssid, sizeof(conf.sta.ssid));
    strncpy((char * ) conf.sta.password, wifiConfig.credentials[index].password, sizeof(conf.sta.password));

    esp_wifi_set_config(WIFI_IF_STA, & conf);

    if (wifiConfig.isDhcp) {
      WiFi.begin(wifiConfig.credentials[index].ssid, wifiConfig.credentials[index].password);
    } else {
      WiFi.config(wifiConfig.ip, wifiConfig.gateway, wifiConfig.subnet, wifiConfig.dns);
      WiFi.begin(wifiConfig.credentials[index].ssid, wifiConfig.credentials[index].password);
    }

    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 30000) {
      delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
      currentState = STATE_CONNECTED;
      if (onConnectedCallback) onConnectedCallback();
      return true;
    }

    return false; // Return false if connection fails
  }

  void setupWiFiBackup() {
    WiFi.mode(WIFI_STA);
    isBackupActive = false;
  }

  void setupSoftAP() {
    WiFi.mode(WIFI_AP);

    if (apConfig.authMode != WIFI_AUTH_OPEN && strlen(apConfig.password) < 8) {
      if (onErrorCallback) onErrorCallback("AP password must be at least 8 characters");
      return;
    }

    WiFi.softAP(apConfig.ssid, apConfig.password, apConfig.channel,
      apConfig.hidden, apConfig.maxConnections);

    dnsServer.start(53, "*", WiFi.softAPIP());

    isSoftAPActive = true;
    currentState = STATE_CONNECTED;

    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
      switch (event) {
      case SYSTEM_EVENT_AP_STACONNECTED:
        if (onClientConnectedCallback) {
          onClientConnectedCallback(event, info);
        }
        break;
      case SYSTEM_EVENT_AP_STADISCONNECTED:
        if (onClientDisconnectedCallback) {
          onClientDisconnectedCallback(event, info);
        }
        break;
      }
    });
  }

  void updateEthernet() {
    if (currentState == STATE_CONNECTED) {
      if (Ethernet.linkStatus() != LinkON) {
        currentState = STATE_DISCONNECTED;
        if (onDisconnectedCallback) onDisconnectedCallback();
        //                fallbackToWiFi();
      } else if (ethConfig.isDhcp) {
        // Check if we still have a valid IP
        IPAddress currentIP = Ethernet.localIP();
        if (currentIP == IPAddress(0, 0, 0, 0)) {
          if (onErrorCallback) onErrorCallback("Lost DHCP lease");
          //                    fallbackToWiFi();
        }
      }
    }
  }

  void updateWiFi() {
    if (currentState == STATE_DISCONNECTED ||
      currentState == STATE_CONNECTION_LOST ||
      currentState == STATE_NO_AP_FOUND) {

      unsigned long currentTime = millis();
      if (currentTime - lastWifiAttempt >= WIFI_RETRY_DELAY) {
        setupWiFi();
        lastWifiAttempt = currentTime;
      }
    }

    if (currentState == STATE_WAITING_FOR_IP) {
      if (WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
        currentState = STATE_CONNECTED;
        if (onConnectedCallback) onConnectedCallback();
      }
    }
  }

  void updateSoftAP() {
    if (isSoftAPActive) {
      dnsServer.processNextRequest();
    }
  }

  void updateEthernetWithBackup() {
    static bool isUsingWiFi = false; // Tracks if the backup WiFi is currently active
    static unsigned long lastEthernetCheck = 0;
    const unsigned long ethernetCheckInterval = 5000; // Check Ethernet status every 5 seconds
    const unsigned long wifiReconnectTimeout = 10000; // Max WiFi connection time (10 seconds)
    static unsigned long lastWiFiReconnectAttempt = 0;
    static int wifiReconnectAttempts = 0;
    const int maxWiFiReconnectAttempts = 3; // Retry WiFi connection up to 3 times
    static int currentWiFiCredentialIndex = 0; // Tracks which credential is being used

    // Check Ethernet link status
    if (Ethernet.linkStatus() == LinkON) {
      if (isUsingWiFi) {
        Serial.println("Ethernet connection restored. Switching back to Ethernet...");

        // Disconnect WiFi if using it
        WiFi.disconnect();
        isUsingWiFi = false;
        wifiReconnectAttempts = 0; // Reset WiFi retry attempts
        currentWiFiCredentialIndex = 0; // Reset to primary WiFi credentials
      }
      // Handle regular Ethernet operations here
      Serial.println("Using Ethernet connection.");
    } else {
      // Ethernet is disconnected
      if (!isUsingWiFi) {
        Serial.println("Ethernet connection lost. Switching to WiFi...");

        // Attempt to connect to WiFi
        if (millis() - lastWiFiReconnectAttempt >= wifiReconnectTimeout && wifiReconnectAttempts < maxWiFiReconnectAttempts) {
          NetworkConfig::WiFiCredential & credential = wifiConfig.credentials[currentWiFiCredentialIndex];
          WiFi.begin(credential.ssid, credential.password);
          lastWiFiReconnectAttempt = millis();
          wifiReconnectAttempts++;

          // Wait for WiFi connection with a timeout
          Serial.printf("Attempting to connect to WiFi: %s\n", credential.ssid);
          unsigned long startAttemptTime = millis();
          while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < wifiReconnectTimeout) {
            delay(100); // Non-blocking, but wait to check connection status
            Serial.print(".");
          }

          if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi connected successfully!");
            isUsingWiFi = true;
            wifiReconnectAttempts = 0; // Reset retry attempts on successful connection
          } else {
            Serial.println("\nFailed to connect to WiFi.");
            currentWiFiCredentialIndex = (currentWiFiCredentialIndex + 1) % NetworkConfig::MAX_WIFI_CREDENTIALS;
          }
        }
      }

      // Handle regular WiFi operations here
      if (isUsingWiFi) {
        Serial.println("Using WiFi connection.");
      }
    }

    // Optionally, throttle Ethernet checks to reduce overhead
    if (millis() - lastEthernetCheck >= ethernetCheckInterval) {
      lastEthernetCheck = millis();
      if (Ethernet.linkStatus() == LinkON) {
        Serial.println("Ethernet reconnected during WiFi fallback.");
      }
    }
  }

};
