#ifndef WIFI_INTERFACE_H
#define WIFI_INTERFACE_H

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <ctime>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <regex>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif // _WIN32

enum class WifiMode
{
    WIFI_MODE_STA = 0,    // 仅STA模式
    WIFI_MODE_AP = 1,     // 仅AP模式
    WIFI_MODE_AP_STA = 2, // AP+STA双模式
    WIFI_MODE_ALL_OFF = 3 // 所有模式关闭
};

// WiFi加密模式
enum class SecurityMode
{
    OPEN = 0,        // 开放网络
    WEP = 1,         // WEP加密
    WPA_PSK = 2,     // WPA-PSK
    WPA2_PSK = 3,    // WPA2-PSK
    WPA_WPA2_PSK = 4 // WPA/WPA2混合
};

struct NetworkInfo
{
    std::string ssid;
    int signalStrength; // 信号强度(dBm)
    SecurityMode security;
    int channel;
    bool isHidden;
    std::string bssid;    // MAC地址
    int frequency;        // 频率(MHz)
    bool autoConnect;     // 是否自动连接
    std::string password; // 保存的密码
};

struct ClientInfo
{
    std::string macAddress;
    std::string ipAddress;
    int signalStrength;
    std::string hostname;
    long connectedTime; // 连接时间(秒)
};

struct StaticIPConfig
{
    std::string ipAddress;  // IP地址 (如: "192.168.1.100")
    std::string subnetMask; // 子网掩码 (如: "255.255.255.0")
    std::string gateway;    // 网关地址 (如: "192.168.1.1")

    StaticIPConfig() : subnetMask("255.255.255.0") {}
};

struct APConfig
{
    std::string ssid;
    std::string password;
    int channel;
    SecurityMode security;
    int maxClients; // 最大客户端数量(1-5)
};

enum class ConnectionStatus
{
    DISCONNECTED = 0,
    CONNECTING = 1,
    CONNECTED = 2,
    DISCONNECTING = 3,
    CONNECTION_FAILED = 4
};

class WifiInterface
{
public:
    WifiInterface(const std::string &staInterface = "wlan0", const std::string &apInterface = "wlan1");
    virtual ~WifiInterface();

    std::string getSTAInterface() const { return staInterface_; }
    std::string getAPInterface() const { return apInterface_; }
    //////////////////// WIFI_MODE  ////////////////////

    /**
     * 设置WiFi工作模式[AP/STA/AP+STA]
     * @param mode 工作模式
     * @return 成功返回true，失败返回false
     */
    bool setOperationMode(WifiMode mode);

    /**
     * 获取当前工作模式
     * @return 当前工作模式
     */
    WifiMode getCurrentMode();

    ////////////////// WIFI_STA_MODE ////////////////////

    /**
     * 扫描可用的WiFi网络
     * @return 成功返回true，失败返回false
     */
    bool scanNetworks();

    /**
     * 获取扫描结果列表
     * @return 网络信息列表
     */
    std::vector<NetworkInfo> getScanResults();

    /**
     * 连接指定的WiFi网络
     * @param ssid 网络名称
     * @param password 密码
     * @return 成功返回true，失败返回false
     */
    bool connectToNetwork(const std::string &ssid, const std::string &password = "");

    /**
     * 断开当前连接
     * @return 成功返回true，失败返回false
     */
    bool disconnect();

    /**
     * 获取当前连接的WiFi网络信息
     * @return 网络信息，如果未连接返回空结构体
     */
    NetworkInfo getCurrentNetwork();

    /**
     * 获取已保存的网络列表
     * @return 已保存的网络列表
     */
    std::vector<NetworkInfo> getSavedNetworks();

    /**
     * 删除已保存的网络
     * @param ssid 要删除的网络名称
     * @return 成功返回true，失败返回false
     */
    bool forgetNetwork(const std::string &ssid);

    /**
     * 自动连接已保存的网络
     * @return 成功返回true，失败返回false
     */
    bool autoConnectToSavedNetworks();

    /**
     * 设置自动连接[运行设备在此网络附近时自动连接]
     * @param ssid 网络名称
     * @param enable 是否启用自动连接
     * @return 成功返回true，失败返回false
     */
    bool setAutoConnect(const std::string &ssid, bool enable);

    /**
     * 设置静态IP配置
     * @param staticConfig 静态IP配置
     * @return 成功返回true，失败返回false
     */
    bool setStaticIPConfig(const StaticIPConfig &staticConfig);

    /**
     * 获取当前静态IP配置
     * @return 静态IP配置
     */
    StaticIPConfig getStaticIPConfig();

    /**
     * 清除静态IP配置，恢复DHCP自动获取
     * @return 成功返回true，失败返回false
     */
    bool clearStaticIPConfig();

    /**
     * 获取连接状态
     * @return 当前连接状态
     */
    ConnectionStatus getConnectionStatus();

    /**
     * 获取IP地址
     * @return IP地址字符串
     */
    std::string getIPAddress();

    /*
     * 获取子网掩码
     * @return 子网掩码字符串
     */
    std::string getSubnetMask();

    /**
     * 获取网关地址
     * @return 网关地址字符串
     */
    std::string getGateway();

    /**
     * 获取MAC地址
     * @return MAC地址字符串
     */
    std::string getMACAddress();

    /**
     * 获取信号强度
     * @return 信号强度(dBm)
     */
    int getSignalStrength();

    ////////////////// WIFI_AP_MODE ////////////////////

    /**
     * 设置热点配置[热点名称、热点密码]
     * @param config AP配置信息
     * @return 成功返回true，失败返回false
     */
    bool setAPConfig(const APConfig &config);

    /**
     * 获取热点配置信息
     * @return AP配置信息
     */
    APConfig getAPConfig();

    /**
     * 启动AP模式
     * @return 成功返回true，失败返回false
     */
    bool startAP();

    /**
     * 停止AP模式
     * @return 成功返回true，失败返回false
     */
    bool stopAP();

    /**
     * 设置连接数量[1-5台]
     * @param maxClients 最大客户端数量
     * @return 成功返回true，失败返回false
     */
    bool setMaxClients(int maxClients);

    /**
     * 获取当前客户端连接数量
     * @return 客户端数量
     */
    int getClientCount();

    /**
     * 获取已连接客户端列表
     * @return 客户端信息列表
     */
    std::vector<ClientInfo> getConnectedClients();

    /**
     * 断开指定客户端连接
     * @param macAddress 客户端MAC地址
     * @return 成功返回true，失败返回false
     */
    bool disconnectClient(const std::string &macAddress);

    /**
     * 获取AP的IP地址
     * @return AP的IP地址
     */
    std::string getAPIPAddress();

    /**
     * 获取AP状态
     * @return AP是否运行中
     */
    bool isAPRunning();

    /**
     * 检测实际的工作模式
     * @return 检测到的实际工作模式
     */
    WifiMode detectActualMode();

private:
    std::string staInterface_;
    std::string apInterface_;
    WifiMode currentMode_;
    ConnectionStatus connectionStatus_;
    APConfig apConfig_;
    NetworkInfo currentNetwork_;
    std::vector<NetworkInfo> savedNetworks_;
    std::vector<NetworkInfo> scanResults_;
    std::vector<ClientInfo> connectedClients_;
    bool isAPRunning_;
    pid_t wpaSupplicantPid_;
    pid_t hostapdPid_;
    std::map<std::string, std::string> savedPasswords_; // 保存wifi密码
    std::map<std::string, bool> autoConnectNetworks_;   // 自动连接设置

    StaticIPConfig staticIPConfig_;
    bool useStaticIP_;

    std::string executeCommand(const std::string &command);
    bool executeCommandWithResult(const std::string &command);
    /*
     * 解码十六进制字符串,解决中文编码问题
     * @param hexString 十六进制字符串
     * @return 解码后的字符串
     */
    std::string decodeHexString(const std::string &hexString);

    bool enableInterface(const std::string &iface);
    bool disableInterface(const std::string &iface);
    bool enableSTAInterface();
    bool enableAPInterface();
    bool disableSTAInterface();
    bool disableAPInterface();
    /*
     * 解析扫描结果并过滤已保存网络
     * @param scanOutput 扫描输出
     * @return 过滤后的网络列表（不包括已保存的）
     */
    bool parseScanResults(const std::string &scanOutput);
    /**
     * 解析扫描结果但不进行已保存网络过滤
     * @param scanOutput 扫描输出
     * @return 所有网络列表（包括已保存的）
     */
    std::vector<NetworkInfo> parseScanResultsWithoutFiltering(const std::string &scanOutput);
    bool startWpaSupplicant();
    bool stopWpaSupplicant();
    bool startHostapd();
    bool startHostapdSafe();
    bool stopHostapd();
    bool startDHCPServer();
    bool configureWpaSupplicant(const std::string &ssid, const std::string &password);
    bool configureHostapd(const APConfig &config);
    bool getInterfaceStatus();

    bool validateAPConfig(const APConfig &config);
    bool validateMaxClients(int maxClients);
    void saveNetworkConfig();
    void loadNetworkConfig();
    bool saveAPConfig();
    bool loadAPConfig();
};

#endif // WIFI_INTERFACE_H