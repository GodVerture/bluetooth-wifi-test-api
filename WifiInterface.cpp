#include "WifiInterface.h"

WifiInterface::WifiInterface(const std::string &staInterface, const std::string &apInterface)
    : staInterface_(staInterface), apInterface_(apInterface),
      connectionStatus_(ConnectionStatus::DISCONNECTED), isAPRunning_(false),
      wpaSupplicantPid_(-1), hostapdPid_(-1)
{
    // 初始化默认AP配置
    apConfig_.ssid = "ONWA_AP";
    apConfig_.password = "12345678";
    apConfig_.channel = 6;
    apConfig_.security = SecurityMode::WPA2_PSK;
    apConfig_.maxClients = 5;

    // 初始化默认静态IP配置
    staticIPConfig_.ipAddress = "";
    staticIPConfig_.subnetMask = "255.255.255.0";
    staticIPConfig_.gateway = "";

    // 检测实际工作模式并初始化
    currentMode_ = detectActualMode();

    loadNetworkConfig();
    loadAPConfig();
}

WifiInterface::~WifiInterface()
{
#ifndef _WIN32
    if (connectionStatus_ == ConnectionStatus::CONNECTED)
    {
        disconnect();
    }

    if (isAPRunning_)
    {
        stopAP();
    }

    if (wpaSupplicantPid_ > 0)
    {
        stopWpaSupplicant();
    }

    disableSTAInterface();
    disableAPInterface();

#endif // _WIN32
}
std::string WifiInterface::executeCommand(const std::string &command)
{
#ifndef _WIN32
    char buffer[128];
    std::string result = "";
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe)
    {
        return "Error: popen failed";
    }
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        result += buffer;
    }
    pclose(pipe);
    return result;
#else
    return "";
#endif // _WIN32
}

bool WifiInterface::executeCommandWithResult(const std::string &command)
{
#ifndef _WIN32
    int result = system(command.c_str());
    return (result == 0);
#else
    return true;
#endif // _WIN32
}

std::string WifiInterface::decodeHexString(const std::string &hexString)
{
    std::string result = "";
    size_t pos = 0;
    // printf("hexString: %s\n", hexString.c_str());
#ifndef _WIN32
    while (pos < hexString.length())
    {
        if (hexString.substr(pos, 2) == "\\x" && pos + 3 < hexString.length())
        {
            std::string hexByte = hexString.substr(pos + 2, 2);
            try
            {
                int byteValue = std::stoi(hexByte, nullptr, 16);
                result += static_cast<char>(byteValue);
                pos += 4; // 跳过 "\\x" 和 2个十六进制字符
            }
            catch (...)
            {
                // 转换失败,保留原字符
                result += hexString[pos];
                pos++;
            }
        }
        else
        {
            result += hexString[pos];
            pos++;
        }
    }
    // printf("decodeHexString: %s\n", result.c_str());
    return result;
#else
    return result;
#endif // _WIN32
}

bool WifiInterface::enableInterface(const std::string &iface)
{
#ifndef _WIN32
    std::string command = "ifconfig " + iface + " up";
    return executeCommandWithResult(command);
#else
    return true;
#endif // _WIN32
}
bool WifiInterface::disableInterface(const std::string &iface)
{
#ifndef _WIN32
    std::string command = "ifconfig " + iface + " down";
    return executeCommandWithResult(command);
#else
    return true;
#endif // _WIN32
}

bool WifiInterface::enableSTAInterface()
{
#ifndef _WIN32
    // 清理接口地址信息，避免IP冲突
    std::string cleanupCommand = "ip addr flush dev " + staInterface_;
    executeCommandWithResult(cleanupCommand);
    return enableInterface(staInterface_);
#else
    return true;
#endif // _WIN32
}
bool WifiInterface::disableSTAInterface()
{
#ifndef _WIN32
    return disableInterface(staInterface_);
#else
    return true;
#endif // _WIN32
}

bool WifiInterface::enableAPInterface()
{
#ifndef _WIN32
    stopHostapd();

    disableInterface(apInterface_);
    sleep(1); // 确保接口完全关闭

    std::string setModeCommand = "iw dev " + apInterface_ + " set type __ap";
    if (!executeCommandWithResult(setModeCommand))
    {
        std::cout << "Warning: Failed to set interface type to AP, continuing anyway..." << std::endl;
    }

    if (!enableInterface(apInterface_))
    {
        std::cout << "Error: Failed to enable AP interface " << apInterface_ << std::endl;
        return false;
    }
    sleep(2); // 等待接口完全启动

    std::string cleanupCommand = "ip addr del 192.168.7.1/24 dev " + apInterface_ + " 2>/dev/null";
    executeCommandWithResult(cleanupCommand);

    std::string statusCommand = "ifconfig " + apInterface_;
    std::string statusResult = executeCommand(statusCommand);
    if (statusResult.find("UP") == std::string::npos)
    {
        std::cout << "Error: AP interface " << apInterface_ << " is not UP after enabling" << std::endl;
        std::cout << "Interface status: " << statusResult << std::endl;
        return false;
    }
    std::cout << "AP interface " << apInterface_ << " enabled successfully" << std::endl;
    return true;
#else
    return true;
#endif // _WIN32
}
bool WifiInterface::disableAPInterface()
{
#ifndef _WIN32
    return disableInterface(apInterface_);
#else
    return true;
#endif // _WIN32
}

/////////////////////// WIFI_MODE  ////////////////////
/*
Todo:
    1. 设置WiFi工作模式[AP/STA/AP+STA]
    2. 获取当前工作模式
*/
bool WifiInterface::setOperationMode(WifiMode mode)
{
#ifndef _WIN32
    if (isAPRunning_)
    {
        std::cout << "Stop AP service..." << std::endl;
        stopAP();
    }
    if (wpaSupplicantPid_ > 0)
    {
        std::cout << "Stop wpa_supplicant service..." << std::endl;
        stopWpaSupplicant();
    }

    currentMode_ = mode;

    bool success = false;
    switch (mode)
    {
    case WifiMode::WIFI_MODE_STA:
        // 启用STA接口，禁用AP接口
        enableSTAInterface();
        disableAPInterface();
        success = startWpaSupplicant();
        if (success)
        {
            sleep(2);
            if (autoConnectToSavedNetworks())
            {
                std::cout << "Auto connect to saved networks success." << std::endl;
            }
            else
            {
                std::cout << "Auto connect to saved networks failed." << std::endl;
            }
        }
        break;
    case WifiMode::WIFI_MODE_AP:
        // 启用AP接口，禁用STA接口
        enableAPInterface();
        disableSTAInterface();
        success = startAP();
        break;
    case WifiMode::WIFI_MODE_AP_STA:
        // 同时启用AP和STA接口
        enableAPInterface();
        enableSTAInterface();
        success = startAP() && startWpaSupplicant();
        if (success)
        {
            sleep(2);
            if (autoConnectToSavedNetworks())
            {
                std::cout << "Auto connect to saved networks success." << std::endl;
            }
            else
            {
                std::cout << "Auto connect to saved networks failed." << std::endl;
            }
        }
        break;
    case WifiMode::WIFI_MODE_ALL_OFF:
        // 关闭WiFi所有服务
        disableAPInterface();
        disableSTAInterface();
        success = true;
        break;
    default:
        return false;
    }
    return success;
#else
    return true;
#endif // _WIN32
}

WifiMode WifiInterface::getCurrentMode()
{
    return currentMode_;
}

WifiMode WifiInterface::detectActualMode()
{
#ifndef _WIN32
    std::string wlan0Check = "ifconfig wlan0 2>/dev/null | grep -q 'UP' && echo '1'";
    std::string wlan1Check = "ifconfig wlan1 2>/dev/null | grep -q 'UP' && echo '1'";

    std::string wlan0Result = executeCommand(wlan0Check);
    std::string wlan1Result = executeCommand(wlan1Check);

    bool wlan0Up = !wlan0Result.empty() && wlan0Result.find('1') != std::string::npos;
    bool wlan1Up = !wlan1Result.empty() && wlan1Result.find('1') != std::string::npos;

    if (wlan0Up && wlan1Up)
    {
        return WifiMode::WIFI_MODE_AP_STA;
    }
    else if (wlan0Up)
    {
        return WifiMode::WIFI_MODE_STA;
    }
    else if (wlan1Up)
    {
        return WifiMode::WIFI_MODE_AP;
    }
    else
    {
        return WifiMode::WIFI_MODE_ALL_OFF;
    }
#else
    return WifiMode::WIFI_MODE_ALL_OFF;
#endif // _WIN32
}

//////////////////// WIFI_STA_MODE ////////////////////
/*
Todo:
    1. 扫描可用的WiFi网络
    2. 获取扫描结果列表
    3. 连接指定的WiFi网络
    4. 断开当前连接
    5. 获取当前连接的WiFi网络信息
    6. 获取已保存的网络列表
    7. 删除已保存的网络
    8. 设置自动连接[运行设备在此网络附近时自动连接]
*/
bool WifiInterface::scanNetworks()
{
#ifndef _WIN32
    if (!enableSTAInterface())
    {
        std::cout << "Error: Failed to enable interface " << staInterface_ << std::endl;
        return false;
    }

    std::string command = "iw dev " + staInterface_ + " scan | grep -E \"^BSS|SSID:|signal:|freq:|WPA|RSN|WEP\"";
    std::string scanOutput = executeCommand(command);

    // 解析扫描结果
    return parseScanResults(scanOutput);
#else
    return true;
#endif // _WIN32
}

bool WifiInterface::parseScanResults(const std::string &scanOutput)
{
#ifndef _WIN32
    scanResults_.clear();

    std::istringstream stream(scanOutput);
    std::string line;
    NetworkInfo currentNetwork;
    bool hasNetwork = false;

    // 由于iw scan出来的设备有重复的，因此需要去重，只保留信号最强的网络
    std::map<std::string, NetworkInfo> uniqueNetworks;

    while (std::getline(stream, line))
    {
        // 去除行首尾的空格
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);

        if (line.find("BSS") == 0) // 以BSS开头的行
        {
            // 新的网络开始，保存前一个网络
            if (hasNetwork && !currentNetwork.ssid.empty())
            {
                // 去重:只保留信号最强的网络
                auto it = uniqueNetworks.find(currentNetwork.ssid);
                if (it == uniqueNetworks.end() ||
                    currentNetwork.signalStrength > it->second.signalStrength)
                {
                    uniqueNetworks[currentNetwork.ssid] = currentNetwork;
                }
            }
            currentNetwork = NetworkInfo();
            hasNetwork = false;

            // 提取BSSID
            std::regex bssRegex("BSS\\s+([0-9a-f:]+)");
            std::smatch match;
            if (std::regex_search(line, match, bssRegex))
            {
                currentNetwork.bssid = match[1];
            }
        }
        else if (line.find("freq:") != std::string::npos)
        {
            // 提取频率
            std::regex freqRegex("freq:\\s*([0-9]+)");
            std::smatch match;
            if (std::regex_search(line, match, freqRegex))
            {
                currentNetwork.frequency = std::stoi(match[1]);
                // 根据频率计算信道
                if (currentNetwork.frequency >= 2412 && currentNetwork.frequency <= 2484)
                {
                    currentNetwork.channel = (currentNetwork.frequency - 2412) / 5 + 1;
                }
                else if (currentNetwork.frequency >= 5180 && currentNetwork.frequency <= 5825)
                {
                    currentNetwork.channel = (currentNetwork.frequency - 5180) / 5 + 36;
                }
            }
        }
        else if (line.find("signal:") != std::string::npos)
        {
            // 提取信号强度
            std::regex signalRegex("signal:\\s*(-?[0-9]+\\.[0-9]+)");
            std::smatch match;
            if (std::regex_search(line, match, signalRegex))
            {
                // 将浮点数转换为整数(取整)
                float signalFloat = std::stof(match[1]);
                currentNetwork.signalStrength = static_cast<int>(signalFloat);
            }
            else
            {
                // 如果没有小数点，则匹配整数
                std::regex signalRegexInt("signal:\\s*(-?[0-9]+)");
                std::smatch matchInt;
                if (std::regex_search(line, matchInt, signalRegexInt))
                {
                    currentNetwork.signalStrength = std::stoi(matchInt[1]);
                }
            }
        }
        else if (line.find("SSID:") != std::string::npos)
        {
            // 提取SSID
            size_t pos = line.find("SSID:") + 5;
            currentNetwork.ssid = line.substr(pos);

            currentNetwork.ssid.erase(0, currentNetwork.ssid.find_first_not_of(" \t"));
            currentNetwork.ssid.erase(currentNetwork.ssid.find_last_not_of(" \t") + 1);
            // 处理中文编码问题
            if (currentNetwork.ssid.find("\\x") != std::string::npos)
            {
                currentNetwork.ssid = decodeHexString(currentNetwork.ssid);
            }
            hasNetwork = true;

            // 默认设置为开放网络模式
            currentNetwork.security = SecurityMode::OPEN;
        }
        else if (line.find("WPA") != std::string::npos || line.find("RSN") != std::string::npos)
        {
            if (line.find("WPA2") != std::string::npos || line.find("RSN") != std::string::npos)
            {
                currentNetwork.security = SecurityMode::WPA2_PSK;
            }
            else if (line.find("WPA") != std::string::npos)
            {
                currentNetwork.security = SecurityMode::WPA_PSK;
            }
        }
        else if (line.find("WEP") != std::string::npos)
        {
            currentNetwork.security = SecurityMode::WEP;
        }
    }

    // 保存最后一个网络
    if (hasNetwork && !currentNetwork.ssid.empty())
    {
        auto it = uniqueNetworks.find(currentNetwork.ssid);
        if (it == uniqueNetworks.end() ||
            currentNetwork.signalStrength > it->second.signalStrength)
        {
            uniqueNetworks[currentNetwork.ssid] = currentNetwork;
        }
    }

    // 将去重后的网络添加到结果列表
    for (const auto &pair : uniqueNetworks)
    {
        bool isSaved = false;
        for (const auto &savedNetwork : savedNetworks_)
        {
            if (savedNetwork.ssid == pair.second.ssid)
            {
                isSaved = true;
                break;
            }
        }
        if (!isSaved)
        {
            scanResults_.push_back(pair.second);
        }
    }

    return !scanResults_.empty();
#else
    return true;
#endif // _WIN32
}

bool WifiInterface::stopWpaSupplicant()
{
#ifndef _WIN32
    std::string killCommand = "killall wpa_supplicant";
    executeCommandWithResult(killCommand);

    std::string forceKillCommand = "killall -9 wpa_supplicant";
    executeCommandWithResult(forceKillCommand);

    // 清理残留的控制接口文件
    std::string cleanupCommand = "rm -f /var/run/wpa_supplicant/" + staInterface_;
    executeCommandWithResult(cleanupCommand);

    sleep(1); // 等待进程完全停止

    // 再次检查是否还有wpa_supplicant进程在运行
    std::string checkCommand = "ps | grep wpa_supplicant | grep -v grep";
    std::string checkResult = executeCommand(checkCommand);

    if (checkResult.empty())
    {
        wpaSupplicantPid_ = -1; // 标记为未运行
        return true;
    }

    return false;
#else
    return true;
#endif // _WIN32
}

bool WifiInterface::startWpaSupplicant()
{
#ifndef _WIN32
    stopWpaSupplicant();

    std::string cleanupCommand = "rm -f /var/run/wpa_supplicant/" + staInterface_;
    executeCommandWithResult(cleanupCommand);

    std::string mkdirCommand = "mkdir -p /var/run/wpa_supplicant";
    executeCommandWithResult(mkdirCommand);

    std::string command = "wpa_supplicant -B -Dnl80211 -c /etc/wpa_supplicant.conf -i " + staInterface_;
    if (executeCommandWithResult(command))
    {
        wpaSupplicantPid_ = 1; // 标记为运行中
        return true;
    }
    return false;
#else
    return true;
#endif // _WIN32
}

std::vector<NetworkInfo> WifiInterface::getScanResults()
{
    return scanResults_;
}

bool WifiInterface::connectToNetwork(const std::string &ssid, const std::string &password)
{
#ifndef _WIN32
    clearStaticIPConfig();

    connectionStatus_ = ConnectionStatus::CONNECTING;

    if (!configureWpaSupplicant(ssid, password))
    {
        connectionStatus_ = ConnectionStatus::CONNECTION_FAILED;
        std::cout << "Error: Failed to configure wpa_supplicant.conf" << std::endl;
        return false;
    }

    // 重启wpa_supplicant以应用新配置
    if (wpaSupplicantPid_ > 0)
    {
        stopWpaSupplicant();
    }

    if (!startWpaSupplicant())
    {
        connectionStatus_ = ConnectionStatus::CONNECTION_FAILED;
        std::cout << "Error: Failed to start wpa_supplicant" << std::endl;
        return false;
    }

    // 等待连接建立
    std::cout << "Waiting for WiFi connection to be established..." << std::endl;
    for (int i = 0; i < 10; i++) // 最多等待10秒
    {
        sleep(1);

        // 检查wpa_supplicant连接状态
        std::string wpaStatusCommand = "wpa_cli -i " + staInterface_ + " status";
        std::string wpaStatus = executeCommand(wpaStatusCommand);

        if (wpaStatus.find("wpa_state=COMPLETED") != std::string::npos)
        {
            // wpa_supplicant认证成功
            break;
        }

        if (wpaStatus.find("wpa_state=DISCONNECTED") != std::string::npos)
        {
            connectionStatus_ = ConnectionStatus::CONNECTION_FAILED;
            std::cout << "WiFi authentication failed, please check whether the password is correct" << std::endl;
            return false;
        }
    }

    // 检查WiFi链路状态
    std::string linkCommand = "iw dev " + staInterface_ + " link";
    std::string linkStatus = executeCommand(linkCommand);

    if (linkStatus.find("Connected") == std::string::npos)
    {
        connectionStatus_ = ConnectionStatus::CONNECTION_FAILED;
        std::cout << "WiFi link connection failed" << std::endl;
        return false;
    }

    // 获取DHCP分配的IP地址
    std::cout << "Get IP address..." << std::endl;
    std::string dhcpCommand = "udhcpc -b -i " + staInterface_ + " -R -t 5 -n";
    if (!executeCommandWithResult(dhcpCommand))
    {
        connectionStatus_ = ConnectionStatus::CONNECTION_FAILED;
        std::cout << "DHCP failed to obtain IP address" << std::endl;
        return false;
    }

    sleep(3); // 等待IP地址分配

    // 验证IP地址是否成功分配
    std::string ipAddress = getIPAddress();
    if (ipAddress.empty())
    {
        connectionStatus_ = ConnectionStatus::CONNECTION_FAILED;
        std::cout << "IP address allocation failed!!!" << std::endl;
        return false;
    }

    // 更新连接状态和网络信息
    connectionStatus_ = ConnectionStatus::CONNECTED;

    // 更新当前网络信息
    for (const auto &network : scanResults_)
    {
        if (network.ssid == ssid)
        {
            currentNetwork_ = network;
            break;
        }
    }

    // 添加到已连接网络列表
    auto it = std::find_if(savedNetworks_.begin(), savedNetworks_.end(),
                           [&ssid](const NetworkInfo &network)
                           {
                               return network.ssid == ssid;
                           });
    if (it == savedNetworks_.end())
    {
        savedNetworks_.push_back(currentNetwork_);
    }

    std::cout << "Connection successful! IP address:" << ipAddress << std::endl;

    // 连接成功后，保存密码
    if (connectionStatus_ == ConnectionStatus::CONNECTED)
    {
        std::string passwordToSave = password;
        if (password.empty())
        {
            auto passwordIt = savedPasswords_.find(ssid);
            if (passwordIt != savedPasswords_.end())
            {
                passwordToSave = passwordIt->second;
            }
        }

        savedPasswords_[ssid] = passwordToSave;
        autoConnectNetworks_[ssid] = true;
        saveNetworkConfig();
    }

    return connectionStatus_ == ConnectionStatus::CONNECTED;
#else
    return true;
#endif // _WIN32
}

bool WifiInterface::setStaticIPConfig(const StaticIPConfig &staticConfig)
{
#ifndef _WIN32
    if (staticConfig.ipAddress.empty() || staticConfig.gateway.empty())
    {
        std::cout << "Error: IP address and gateway cannot be empty" << std::endl;
        return false;
    }

    auto isValidIP = [](const std::string &ip) -> bool
    {
        int dotCount = 0;
        for (char c : ip)
        {
            if (c == '.')
                dotCount++;
            else if (!isdigit(c))
                return false;
        }
        return dotCount == 3;
    };

    if (!isValidIP(staticConfig.ipAddress))
    {
        std::cout << "Error: Invalid IP address format: " << staticConfig.ipAddress << std::endl;
        return false;
    }

    if (!isValidIP(staticConfig.gateway))
    {
        std::cout << "Error: Invalid gateway format: " << staticConfig.gateway << std::endl;
        return false;
    }

    // 设置静态IP配置
    staticIPConfig_ = staticConfig;
    useStaticIP_ = true;

    std::cout << "Applying static IP configuration to interface " << staInterface_ << "..." << std::endl;

    // 1. 清除现有IP配置
    std::string clearIPCommand = "ip addr flush dev " + staInterface_;
    if (!executeCommandWithResult(clearIPCommand))
    {
        std::cout << "Warning: Failed to clear existing IP configuration" << std::endl;
    }

    // 2. 设置静态IP地址和子网掩码
    std::string setIPCommand = "ip addr add " + staticConfig.ipAddress + "/" +
                               (staticConfig.subnetMask == "255.255.255.0" ? "24" : staticConfig.subnetMask == "255.255.0.0" ? "16"
                                                                                                                             : "24") +
                               " dev " + staInterface_;
    if (!executeCommandWithResult(setIPCommand))
    {
        std::cout << "Error: Failed to set static IP address" << std::endl;
        return false;
    }

    // 3. 启用接口
    std::string upCommand = "ip link set " + staInterface_ + " up";
    if (!executeCommandWithResult(upCommand))
    {
        std::cout << "Error: Failed to enable interface" << std::endl;
        return false;
    }

    // 4. 设置默认网关
    std::string routeCommand = "ip route add default via " + staticConfig.gateway + " dev " + staInterface_;
    if (!executeCommandWithResult(routeCommand))
    {
        std::cout << "Warning: Failed to set default gateway, you may need to set it manually" << std::endl;
    }

    std::cout << "Static IP configuration set and applied successfully:" << std::endl;
    std::cout << "  IP Address: " << staticIPConfig_.ipAddress << std::endl;
    std::cout << "  Subnet Mask: " << staticIPConfig_.subnetMask << std::endl;
    std::cout << "  Gateway: " << staticIPConfig_.gateway << std::endl;
    std::cout << "  Interface: " << staInterface_ << std::endl;

    return true;
#else
    return true;
#endif // _WIN32
}

StaticIPConfig WifiInterface::getStaticIPConfig()
{
    return staticIPConfig_;
}

bool WifiInterface::clearStaticIPConfig()
{
#ifndef _WIN32
    useStaticIP_ = false;

    std::cout << "Clearing static IP configuration from interface " << staInterface_ << "..." << std::endl;

    // 1. 清除静态IP地址
    if (!staticIPConfig_.ipAddress.empty())
    {
        std::string clearIPCommand = "ip addr del " + staticIPConfig_.ipAddress + "/" +
                                     (staticIPConfig_.subnetMask == "255.255.255.0" ? "24" : staticIPConfig_.subnetMask == "255.255.0.0" ? "16"
                                                                                                                                         : "24") +
                                     " dev " + staInterface_;
        executeCommandWithResult(clearIPCommand);
    }

    // 2. 清除默认路由
    if (!staticIPConfig_.gateway.empty())
    {
        std::string clearRouteCommand = "ip route del default via " + staticIPConfig_.gateway + " dev " + staInterface_;
        executeCommandWithResult(clearRouteCommand);
    }

    // 3. 重置为默认值
    staticIPConfig_ = StaticIPConfig();

    // 4. 启用接口(使用DHCP)
    std::string upCommand = "ip link set " + staInterface_ + " up";
    executeCommandWithResult(upCommand);

    std::cout << "Static IP configuration cleared, interface " << staInterface_ << " will use DHCP" << std::endl;
    return true;
#else
    return true;
#endif // _WIN32
}

bool WifiInterface::configureWpaSupplicant(const std::string &ssid, const std::string &password)
{
#ifndef _WIN32
    std::ofstream configFile("/etc/wpa_supplicant.conf");
    if (!configFile.is_open())
    {
        std::cout << "Error: Failed to open wpa_supplicant.conf" << std::endl;
        return false;
    }

    configFile << "ctrl_interface=/var/run/wpa_supplicant\n";
    configFile << "update_config=1\n";
    configFile << "country=US\n";

    configFile << "network={\n";
    configFile << "    ssid=\"" << ssid << "\"\n";

    // 如果密码为空，检查是否有已保存的密码
    std::string actualPassword = password;
    if (password.empty())
    {
        auto passwordIt = savedPasswords_.find(ssid);
        if (passwordIt != savedPasswords_.end())
        {
            actualPassword = passwordIt->second;
            std::cout << "----->actualPassword: " << actualPassword << std::endl;
        }
    }

    if (!actualPassword.empty())
    {
        configFile << "    psk=\"" << actualPassword << "\"\n";
    }
    else
    {
        configFile << "    key_mgmt=NONE\n";
    }
    configFile << "}\n";
    configFile.close();
    return true;
#else
    return true;
#endif // _WIN32
}

bool WifiInterface::disconnect()
{
#ifndef _WIN32
    ConnectionStatus actualStatus = getConnectionStatus();
    if (actualStatus == ConnectionStatus::CONNECTED)
    {
        stopWpaSupplicant();

        // 杀死udhcpc进程
        std::string killCommand = "killall udhcpc 2>/dev/null";
        executeCommandWithResult(killCommand);

        // 清除IP地址
        std::string clearIPCommand = "ip addr flush dev " + staInterface_;
        executeCommandWithResult(clearIPCommand);

        // 禁用网络接口
        std::string downCommand = "ip link set " + staInterface_ + " down";
        executeCommandWithResult(downCommand);

        // 重新启用网络接口(清除所有配置)
        std::string upCommand = "ip link set " + staInterface_ + " up";
        executeCommandWithResult(upCommand);

        connectionStatus_ = ConnectionStatus::DISCONNECTED;
        currentNetwork_ = NetworkInfo(); // 清空当前网络信息

        std::cout << "WiFi connection has been lost and network interface has been reset" << std::endl;
        return true;
    }
    return false;
#else
    return false;
#endif // _WIN32
}

NetworkInfo WifiInterface::getCurrentNetwork()
{
    return currentNetwork_;
}

std::vector<NetworkInfo> WifiInterface::getSavedNetworks()
{
#ifndef _WIN32
    std::vector<NetworkInfo> networks;

    for (const auto &network : savedNetworks_)
    {
        NetworkInfo info = network;
        auto autoConnectIt = autoConnectNetworks_.find(network.ssid);
        if (autoConnectIt != autoConnectNetworks_.end())
        {
            info.autoConnect = autoConnectIt->second;
        }
        networks.push_back(info);
    }

    return networks;
#else
    return std::vector<NetworkInfo>();
#endif // _WIN32
}
bool WifiInterface::forgetNetwork(const std::string &ssid)
{
#ifndef _WIN32
    auto it = std::find_if(savedNetworks_.begin(), savedNetworks_.end(),
                           [&ssid](const NetworkInfo &network)
                           {
                               return network.ssid == ssid;
                           });
    if (it != savedNetworks_.end())
    {
        savedNetworks_.erase(it);
        savedPasswords_.erase(ssid);
        autoConnectNetworks_.erase(ssid);
        saveNetworkConfig();
        return true;
    }
    return false;
#else
    return false;
#endif // _WIN32
}

void WifiInterface::saveNetworkConfig()
{
#ifndef _WIN32
    std::ofstream configFile("/etc/wifi_networks.conf");
    if (!configFile.is_open())
    {
        std::cout << "Unable to create network profile" << std::endl;
        return;
    }

    for (const auto &network : savedNetworks_)
    {
        auto passwordIt = savedPasswords_.find(network.ssid);
        auto autoConnectIt = autoConnectNetworks_.find(network.ssid);

        if (passwordIt != savedPasswords_.end() && autoConnectIt != autoConnectNetworks_.end())
        {
            configFile << network.ssid << "|"
                       << passwordIt->second << "|"
                       << (autoConnectIt->second ? "1" : "0") << std::endl;
        }
    }
    configFile.close();
#else
    return;
#endif // _WIN32
}

void WifiInterface::loadNetworkConfig()
{
#ifndef _WIN32
    std::ifstream configFile("/etc/wifi_networks.conf");
    if (!configFile.is_open())
    {
        return;
    }

    std::string line;
    while (std::getline(configFile, line))
    {
        size_t pos1 = line.find('|');
        size_t pos2 = line.find('|', pos1 + 1);

        if (pos1 != std::string::npos && pos2 != std::string::npos)
        {
            std::string ssid = line.substr(0, pos1);
            std::string password = line.substr(pos1 + 1, pos2 - pos1 - 1);
            bool autoConnect = line.substr(pos2 + 1) == "1";

            // 保存密码和自动连接设置
            savedPasswords_[ssid] = password;
            autoConnectNetworks_[ssid] = autoConnect;

            // 创建NetworkInfo并添加到savedNetworks_列表
            NetworkInfo networkInfo;
            networkInfo.ssid = ssid;
            networkInfo.autoConnect = autoConnect;

            // 检查是否已存在该网络，避免重复添加
            auto it = std::find_if(savedNetworks_.begin(), savedNetworks_.end(),
                                   [&ssid](const NetworkInfo &network)
                                   {
                                       return network.ssid == ssid;
                                   });
            if (it == savedNetworks_.end())
            {
                savedNetworks_.push_back(networkInfo);
            }
        }
    }
    configFile.close();
#else
    return;
#endif // _WIN32
}

bool WifiInterface::setAutoConnect(const std::string &ssid, bool autoConnect)
{
#ifndef _WIN32
    autoConnectNetworks_[ssid] = autoConnect;
    saveNetworkConfig();
    return true;
#else
    return false;
#endif // _WIN32
}

bool WifiInterface::autoConnectToSavedNetworks()
{
#ifndef _WIN32
    std::cout << "Trying to automatically connect to a saved network..." << std::endl;

    auto savedNetworks = getSavedNetworks();
    if (savedNetworks.empty())
    {
        std::cout << "No saved network configuration" << std::endl;
        return false;
    }

    std::vector<NetworkInfo> availableNetworks;
    for (const auto &savedNetwork : savedNetworks)
    {
        if (savedNetwork.autoConnect)
        {
            availableNetworks.push_back(savedNetwork);
        }
    }

    if (availableNetworks.empty())
    {
        std::cout << "No network with auto-connect enabled" << std::endl;
        return false;
    }

    for (const auto &network : availableNetworks)
    {
        std::cout << "Try to connect to the network: " << network.ssid << std::endl;

        // 从保存的密码中获取密码
        auto passwordIt = savedPasswords_.find(network.ssid);
        if (passwordIt == savedPasswords_.end())
        {
            std::cout << "Network not found " << network.ssid << " Password, skip" << std::endl;
            continue;
        }

        if (connectToNetwork(network.ssid, passwordIt->second))
        {
            std::cout << "Automatic connection successful! Connect to the network: " << network.ssid << std::endl;
            return true;
        }
        else
        {
            std::cout << "Connect to the Internet " << network.ssid << " fail" << std::endl;
        }
    }

    std::cout << "Auto-connect failed. No available network could be connected." << std::endl;
    return false;
#else
    return false;
#endif // _WIN32
}

ConnectionStatus WifiInterface::getConnectionStatus()
{
#ifndef _WIN32
    // 检查STA接口的连接状态
    std::string command = "iw dev " + staInterface_ + " link";
    std::string linkStatus = executeCommand(command);

    if (linkStatus.empty())
    {
        return connectionStatus_;
    }

    if (linkStatus.find("Connected") != std::string::npos)
    {
        if (connectionStatus_ != ConnectionStatus::CONNECTED)
        {
            connectionStatus_ = ConnectionStatus::CONNECTED;
        }
        return ConnectionStatus::CONNECTED;
    }
    else if (linkStatus.find("Not connected") != std::string::npos)
    {
        if (connectionStatus_ != ConnectionStatus::DISCONNECTED)
        {
            connectionStatus_ = ConnectionStatus::DISCONNECTED;
        }
        return ConnectionStatus::DISCONNECTED;
    }
    else
    {
        std::cout << "Cannot determine the WiFi link status. Use internal status." << std::endl;
        return connectionStatus_;
    }
#else
    return ConnectionStatus::DISCONNECTED;
#endif // _WIN32
}

std::string WifiInterface::getIPAddress()
{
#ifndef _WIN32
    std::string command = "ip addr show " + staInterface_ + " | grep 'inet ' | awk '{print $2}' | cut -d/ -f1";
    std::string ip = executeCommand(command);

    if (!ip.empty() && ip.back() == '\n')
    {
        ip.pop_back();
    }
    return ip;
#else
    return "192.168.0.1";
#endif // _WIN32
}
std::string WifiInterface::getMACAddress()
{
#ifndef _WIN32
    std::string command = "cat /sys/class/net/" + staInterface_ + "/address";
    std::string mac = executeCommand(command);

    if (!mac.empty() && mac.back() == '\n')
    {
        mac.pop_back();
    }
    return mac;
#else
    return "";
#endif // _WIN32
}

int WifiInterface::getSignalStrength()
{
#ifndef _WIN32
    std::string command = "iw dev " + staInterface_ + " link | grep 'signal:' | awk '{print $2}'";
    std::string signalStr = executeCommand(command);

    if (!signalStr.empty())
    {
        try
        {
            return std::stoi(signalStr);
        }
        catch (...)
        {
            return -100; // 转换失败，返回一个无效值
        }
    }
    return -100; // 未找到信号强度信息，返回一个无效值
#else
    return 0;
#endif // _WIN32
}

//////////////////// WIFI_AP_MODE ////////////////////
/*
Todo:
    1. 设置热点配置[热点名称、热点密码]
    2. 获取热点配置信息
    3. 设置连接数量[1-5台]
    4. 获取当前客户端连接数量
    5. 获取已连接客户端列表
*/
bool WifiInterface::setAPConfig(const APConfig &config)
{
#ifndef _WIN32
    if (!validateAPConfig(config))
    {
        return false;
    }
    apConfig_ = config;

    if (!saveAPConfig())
    {
        std::cout << "Error: Failed to save AP config" << std::endl;
        return false;
    }

    // 如果AP模式正在运行，需要重启AP服务使新配置生效
    if (isAPRunning_)
    {
        // 1. 配置hostapd
        if (!configureHostapd(apConfig_))
        {
            std::cout << "Error: Failed to configure hostapd" << std::endl;
            return false;
        }
        // 2.停止AP服务
        if (!stopAP())
        {
            std::cout << "Error: Failed to stop AP service" << std::endl;
        }
        // 3. 确保服务停止
        sleep(1);
        // 4. 启动AP服务
        if (!startAP())
        {
            std::cout << "Error: Failed to start AP service" << std::endl;
            return false;
        }
        std::cout << "AP configuration has been updated and restarted successfully." << std::endl;
    }
    else
    {
        // AP模式未运行，只生成配置文件
        if (!configureHostapd(apConfig_))
        {
            std::cout << "Error: Failed to configure hostapd" << std::endl;
            return false;
        }
        std::cout << "The AP configuration is saved. The new configuration will be used next time you start AP mode." << std::endl;
    }
    return true;
#else
    return false;
#endif // _WIN32
}

APConfig WifiInterface::getAPConfig()
{
    return apConfig_;
}

bool WifiInterface::startAP()
{
#ifndef _WIN32
    if (isAPRunning_)
    {
        std::cout << "The AP service is already running, stop it first..." << std::endl;
        stopAP();
    }

    std::cout << "Starting AP mode with enhanced safety measures..." << std::endl;

    // 保存当前网络状态
    std::string saveRoute = "route -n > /tmp/route_backup.txt 2>/dev/null";
    executeCommandWithResult(saveRoute);

    // 先检查SSH连接使用的接口
    std::string sshInterface = "route -n | grep '^0.0.0.0' | awk '{print $8}' | head -1";
    std::string sshIfResult = executeCommand(sshInterface);
    if (!sshIfResult.empty())
    {
        std::cout << "SSH connection uses interface: " << sshIfResult << std::endl;
    }

    // 检查AP接口状态，如果接口未启用则启用它
    std::string statusCommand = "ifconfig " + apInterface_;
    std::string statusResult = executeCommand(statusCommand);
    if (statusResult.find("UP") == std::string::npos)
    {
        std::cout << "AP interface " << apInterface_ << " is not UP, enabling it..." << std::endl;
        if (!enableAPInterface())
        {
            std::cout << "Error: Unable to enable AP interface" << apInterface_ << std::endl;
            return false;
        }
    }
    else
    {
        std::cout << "AP interface " << apInterface_ << " is already enabled" << std::endl;
    }

    std::string cleanupCommand = "killall -9 hostapd dnsmasq 2 >/dev/null";
    executeCommandWithResult(cleanupCommand);

    std::string rmConfigCommand = "rm -f /etc/hostapd.conf /etc/dnsmasq.conf";
    executeCommandWithResult(rmConfigCommand);

    if (!configureHostapd(apConfig_))
    {
        std::cout << "Error: Failed to configure hostapd" << std::endl;
        return false;
    }
    // 先设置AP接口的IP地址
    std::string ipCommand = "ip addr add 192.168.7.1/24 dev " + apInterface_;
    if (!executeCommandWithResult(ipCommand))
    {
        std::cout << "Error: Failed to set IP address for AP interface" << std::endl;
        return false;
    }
    std::cout << "IP address 192.168.7.1/24 set for AP interface" << std::endl;

    // 启用IP转发
    std::string ipForwardCommand = "echo 1 > /proc/sys/net/ipv4/ip_forward";
    executeCommandWithResult(ipForwardCommand);

    std::string natCommand = "iptables -t nat -A POSTROUTING -o " + apInterface_ + " -j MASQUERADE";
    executeCommandWithResult(natCommand);

    std::string forwardRule1 = "iptables -A FORWARD -i " + apInterface_ + " -o eth0 -j ACCEPT";
    std::string forwardRule2 = "iptables -A FORWARD -i eth0 -o " + apInterface_ + " -m state --state RELATED,ESTABLISHED -j ACCEPT";
    executeCommandWithResult(forwardRule1);
    executeCommandWithResult(forwardRule2);

    if (!startDHCPServer())
    {
        std::cout << "Warning: Failed to start DHCP server, clients will need manual IP configuration" << std::endl;
    }

    if (!startHostapdSafe())
    {
        std::cout << "Error: Failed to start hostapd" << std::endl;
        stopAP();
        return false;
    }

    sleep(3); // 等待hostapd完全启动

    // 检查hostapd是否正常运行
    std::string checkCommand = "ps | grep hostapd | grep -v grep";
    std::string checkResult = executeCommand(checkCommand);
    if (checkResult.empty())
    {
        std::cout << "Error: hostapd process not found after startup" << std::endl;
        stopAP();
        return false;
    }

    // 检查网络连接状态
    std::string pingCheck = "ping -c 1 -W 2 8.8.8.8 >/dev/null 2>&1 && echo 'Internet connection: OK' || echo 'Internet connection: Failed'";
    std::string pingResult = executeCommand(pingCheck);
    std::cout << pingResult << std::endl;

    isAPRunning_ = true;
    std::cout << "AP mode is enabled, interface:" << apInterface_ << std::endl;
    std::cout << "SSID: " << apConfig_.ssid << ", Channel: " << apConfig_.channel << std::endl;

    return true;
#else
    return true;
#endif // _WIN32
}

bool WifiInterface::startDHCPServer()
{
#ifndef _WIN32
    // 创建dnsmasq配置文件
    std::ofstream dhcpConfig("/etc/dnsmasq.conf");
    if (!dhcpConfig.is_open())
    {
        std::cout << "Error: Cannot create dnsmasq configuration" << std::endl;
        return false;
    }

    dhcpConfig << "interface=" << apInterface_ << "\n";
    dhcpConfig << "dhcp-range=192.168.7.10,192.168.7.100,255.255.255.0,24h\n";
    dhcpConfig << "dhcp-option=3,192.168.7.1\n"; // 网关
    dhcpConfig << "dhcp-option=6,8.8.8.8\n";     // DNS服务器
    dhcpConfig << "server=8.8.8.8\n";
    dhcpConfig << "log-dhcp\n";
    dhcpConfig.close();

    // 启动dnsmasq
    std::string command = "dnsmasq -C /etc/dnsmasq.conf";
    if (executeCommandWithResult(command))
    {
        sleep(2);

        // 检查dnsmasq是否运行
        std::string checkCommand = "ps | grep dnsmasq | grep -v grep";
        std::string checkResult = executeCommand(checkCommand);
        if (!checkResult.empty())
        {
            std::cout << "DHCP server started successfully" << std::endl;
            return true;
        }
    }

    std::cout << "Error: Failed to start DHCP server" << std::endl;
    return false;
#else
    return false;
#endif // _WIN32
}

bool WifiInterface::configureHostapd(const APConfig &config)
{
#ifndef _WIN32
    if (!validateAPConfig(config))
    {
        return false;
    }

    std::ofstream configFile("/etc/hostapd.conf");
    if (!configFile.is_open())
    {
        std::cout << "Error: Cannot open /etc/hostapd.conf for writing" << std::endl;
        return false;
    }

    configFile << "# Hostapd configuration file for " << apInterface_ << "\n";
    configFile << "interface=" << apInterface_ << "\n";
    configFile << "driver=nl80211\n";
    configFile << "ssid=" << config.ssid << "\n";

    std::string hw_mode;
    if (config.channel >= 1 && config.channel <= 14)
    {
        // 2.4GHz频段
        hw_mode = "g"; // 802.11g (2.4GHz)
    }
    else if (config.channel >= 36 && config.channel <= 165)
    {
        // 5GHz频段
        hw_mode = "a"; // 802.11a (5GHz)
    }
    else
    {
        // 无效信道，使用默认值
        std::cout << "Warning: Invalid channel " << config.channel << ", using default channel 6" << std::endl;
        hw_mode = "g";
        configFile << "hw_mode=" << hw_mode << "\n";
        configFile << "channel=6\n";
    }

    configFile << "hw_mode=" << hw_mode << "\n";
    configFile << "channel=" << config.channel << "\n";

    configFile << "wmm_enabled=1\n";
    configFile << "macaddr_acl=0\n";
    configFile << "auth_algs=1\n";
    configFile << "ignore_broadcast_ssid=0\n";
    configFile << "max_num_sta=" << config.maxClients << "\n";

    // 根据频段设置HT/VHT模式
    if (hw_mode == "g")
    {
        // 2.4GHz频段 - 启用HT40模式
        configFile << "ieee80211n=1\n";
        configFile << "ht_capab=[HT40+][HT40-][SHORT-GI-20][SHORT-GI-40][DSSS_CCK-40]\n";
    }
    else if (hw_mode == "a")
    {
        // 5GHz频段 - 启用VHT模式
        configFile << "ieee80211n=1\n";
        configFile << "ieee80211ac=1\n";
        configFile << "vht_capab=[MAX-AMPDU-3895][RXLDPC][SHORT-GI-80][TX-STBC-2BY1][RX-STBC-1][MAX-A-MPDU-LEN-EXP3]\n";
        configFile << "vht_oper_chwidth=1\n";
        configFile << "vht_oper_centr_freq_seg0_idx=" << (config.channel + 2) << "\n";
    }

    switch (config.security)
    {
    case SecurityMode::OPEN:
        configFile << "wpa=0\n";
        break;
    case SecurityMode::WPA2_PSK:
        configFile << "wpa=2\n";
        configFile << "wpa_key_mgmt=WPA-PSK\n";
        configFile << "wpa_passphrase=" << config.password << "\n";
        configFile << "rsn_pairwise=CCMP\n";
        configFile << "wpa_group_rekey=3600\n";
        break;
    default:
        configFile << "wpa=2\n";
        configFile << "wpa_key_mgmt=WPA-PSK\n";
        configFile << "wpa_passphrase=" << config.password << "\n";
        configFile << "rsn_pairwise=CCMP\n";
        configFile << "wpa_group_rekey=3600\n";
        break;
    }

    configFile.close();
    return true;
#else
    return true;
#endif // _WIN32
}

bool WifiInterface::startHostapdSafe()
{
#ifndef _WIN32
    if (hostapdPid_ > 0)
    {
        stopHostapd();
    }

    std::string cleanupCommand = "killall -9 hostapd 2>/dev/null";
    executeCommandWithResult(cleanupCommand);

    std::string command = "setsid hostapd /etc/hostapd.conf -B > /dev/null 2>&1";
    if (executeCommandWithResult(command))
    {
        sleep(3); // 等待hostapd启动

        // 检查hostapd是否正常运行
        std::string checkCommand = "ps | grep hostapd | grep -v grep";
        std::string checkResult = executeCommand(checkCommand);
        if (!checkResult.empty())
        {
            hostapdPid_ = 1; // 标记为正在运行状态
            std::cout << "hostapd started successfully using safe method" << std::endl;

            std::string ifconfig = "ifconfig " + apInterface_;
            std::string ifconfigResult = executeCommand(ifconfig);
            if (!ifconfigResult.empty())
            {
                std::cout << "AP interface " << apInterface_ << " is ready" << std::endl;
            }

            return true;
        }
    }

    std::cout << "Error: Failed to start hostapd using safe method" << std::endl;

    std::cout << "Trying alternative startup method..." << std::endl;
    std::string altCommand = "hostapd -B /etc/hostapd.conf";
    if (executeCommandWithResult(altCommand))
    {
        sleep(2);
        std::string checkCommand = "ps | grep hostapd | grep -v grep";
        std::string checkResult = executeCommand(checkCommand);
        if (!checkResult.empty())
        {
            hostapdPid_ = 1;
            std::cout << "hostapd started successfully using alternative method" << std::endl;
            return true;
        }
    }

    return false;
#else
    return false;
#endif // _WIN32
}

bool WifiInterface::startHostapd()
{
#ifndef _WIN32
    if (hostapdPid_ > 0)
    {
        stopHostapd();
    }

    std::string cleanupCommand = "killall -9 hostapd 2>/dev/null";
    executeCommandWithResult(cleanupCommand);

    std::string command = "nohup hostapd /etc/hostapd.conf > /var/log/hostapd.log 2>&1 &";
    if (executeCommandWithResult(command))
    {
        sleep(2); // 等待hostapd启动

        // 检查hostapd是否正常运行
        std::string checkCommand = "ps | grep hostapd | grep -v grep";
        std::string checkResult = executeCommand(checkCommand);
        if (!checkResult.empty())
        {
            hostapdPid_ = 1; // 标记为正在运行状态
            std::cout << "hostapd started successfully" << std::endl;

            // 检查hostapd日志
            std::string logCheck = "tail -5 /var/log/hostapd.log";
            std::string logResult = executeCommand(logCheck);
            if (!logResult.empty())
            {
                std::cout << "Hostapd log (last 5 lines):" << std::endl;
                std::cout << logResult << std::endl;
            }

            return true;
        }
    }

    std::cout << "Error: Failed to start hostapd" << std::endl;

    std::string errorLog = "cat /var/log/hostapd.log 2>/dev/null | tail -10";
    std::string errorResult = executeCommand(errorLog);
    if (!errorResult.empty())
    {
        std::cout << "Hostapd error log:" << std::endl;
        std::cout << errorResult << std::endl;
    }

    return false;
#else
    return false;
#endif // _WIN32
}

bool WifiInterface::stopHostapd()
{
#ifndef _WIN32
    // 先检查hostapd进程是否存在
    std::string checkCommand = "ps | grep hostapd | grep -v grep";
    std::string checkResult = executeCommand(checkCommand);

    if (checkResult.empty())
    {
        // hostapd进程不存在，直接返回成功
        hostapdPid_ = -1;
        return true;
    }

    std::cout << "Stopping hostapd process..." << std::endl;

    // 停止hostapd服务
    std::string command = "killall hostapd 2>/dev/null &";
    executeCommandWithResult(command);

    sleep(1); // 等待进程停止

    // 检查进程是否已停止
    checkResult = executeCommand(checkCommand);
    if (checkResult.empty())
    {
        hostapdPid_ = -1;
        std::cout << "hostapd stopped successfully" << std::endl;
        return true;
    }

    // 若无法正常停止，则强制杀死hostapd进程
    std::cout << "Forcing hostapd to stop..." << std::endl;
    std::string forceCommand = "killall -9 hostapd 2>/dev/null &";
    executeCommandWithResult(forceCommand);

    sleep(1); // 等待强制停止完成

    // 再次检查进程是否已停止
    checkResult = executeCommand(checkCommand);
    if (checkResult.empty())
    {
        hostapdPid_ = -1;
        std::cout << "hostapd has been forced to stop" << std::endl;
        return true;
    }

    std::cout << "Warning: Unable to stop hostapd process completely" << std::endl;
    return false;
#else
    return false;
#endif // _WIN32
}

bool WifiInterface::stopAP()
{
#ifndef _WIN32
    if (!isAPRunning_)
    {
        return true;
    }
    isAPRunning_ = false;

    std::cout << "Stopping AP service..." << std::endl;

    // 使用后台进程执行清理命令，避免阻塞
    std::string killDHCPServer = "killall -9 dnsmasq 2>/dev/null";
    executeCommandWithResult(killDHCPServer);

    // 停止hostapd进程
    if (!stopHostapd())
    {
        std::cout << "Warning: Failed to stop hostapd gracefully, continuing cleanup..." << std::endl;
    }

    // 清理IP地址
    std::string ipCleanup = "ip addr del 192.168.8.1/24 dev " + apInterface_ + " 2>/dev/null &";
    executeCommandWithResult(ipCleanup);

    // 清理iptables规则
    std::string cleanupNatRule = "iptables -t nat -D POSTROUTING -o " + apInterface_ + " -j MASQUERADE 2>/dev/null &";
    std::string cleanupForward1 = "iptables -D FORWARD -i " + apInterface_ + " -o eth0 -j ACCEPT 2>/dev/null &";
    std::string cleanupForward2 = "iptables -D FORWARD -i eth0 -o " + apInterface_ + " -m state --state RELATED,ESTABLISHED -j ACCEPT 2>/dev/null &";

    executeCommandWithResult(cleanupNatRule);
    executeCommandWithResult(cleanupForward1);
    executeCommandWithResult(cleanupForward2);

    sleep(2); // 等待清理操作完成

    if (!disableInterface(apInterface_))
    {
        std::cout << "Warning: Failed to disable AP interface gracefully" << std::endl;
    }

    std::cout << "AP mode is disabled" << std::endl;
    return true;
#else
    return true;
#endif // _WIN32
}

bool WifiInterface::setMaxClients(int maxClients)
{
#ifndef _WIN32
    if (!validateMaxClients(maxClients))
    {
        return false;
    }

    apConfig_.maxClients = maxClients;
    return true;
#else
    return false;
#endif // _WIN32
}

int WifiInterface::getClientCount()
{
#ifndef _WIN32
    if (!isAPRunning_)
    {
        return 0;
    }

    // 从hostapd的日志或状态文件中获取客户端连接数量
    std::string command = "iw dev " + apInterface_ + " station dump | grep Station | wc -l";
    std::string countStr = executeCommand(command);

    if (!countStr.empty())
    {
        try
        {
            countStr.erase(0, countStr.find_first_not_of(" \t\n\r"));
            countStr.erase(countStr.find_last_not_of(" \t\n\r") + 1);
            return std::stoi(countStr);
        }
        catch (...)
        {
            return 0; // 转换失败
        }
    }
    return 0; // 未找到客户端数量信息
#else
    return 0;
#endif // _WIN32
}

std::vector<ClientInfo> WifiInterface::getConnectedClients()
{
#ifndef _WIN32
    std::vector<ClientInfo> clients;

    if (!isAPRunning_)
    {
        return clients;
    }

    // 获取客户端列表
    std::string command = "iw dev " + apInterface_ + " station dump";
    std::string clientInfo = executeCommand(command);

    if (clientInfo.empty())
    {
        std::cout << "Unable to obtain client information, command execution failed!!!" << std::endl;
        return clients;
    }

    // 解析客户端信息
    std::istringstream stream(clientInfo);
    std::string line;
    ClientInfo currentClient;
    bool hasClient = false;

    while (std::getline(stream, line))
    {
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);

        if (line.find("Station") == 0)
        {
            // 新的客户端开始，保存前一个客户端
            if (hasClient)
            {
                clients.push_back(currentClient);
                currentClient = ClientInfo();
                hasClient = false;
            }

            // 提取MAC地址
            std::regex macRegex("Station\\s+([0-9a-f:]+)");
            std::smatch match;
            if (std::regex_search(line, match, macRegex))
            {
                currentClient.macAddress = match[1];
                hasClient = true;
            }
        }
        else if (line.find("signal:") != std::string::npos)
        {
            // 提取信号强度
            std::regex signalRegex("signal:\\s*(-?[0-9]+\\.[0-9]+)");
            std::smatch match;
            if (std::regex_search(line, match, signalRegex))
            {
                // 将浮点数转换为整数(取整)
                float signalFloat = std::stof(match[1]);
                currentClient.signalStrength = static_cast<int>(signalFloat);
            }
            else
            {
                // 如果没有小数点，则匹配整数
                std::regex signalRegexInt("signal:\\s*(-?[0-9]+)");
                std::smatch matchInt;
                if (std::regex_search(line, matchInt, signalRegexInt))
                {
                    currentClient.signalStrength = std::stoi(matchInt[1]);
                }
            }
        }
        else if (line.find("connected time:") != std::string::npos)
        {
            // 提取连接时间
            std::regex timeRegex("connected time:\\s*([0-9]+)");
            std::smatch match;
            if (std::regex_search(line, match, timeRegex))
            {
                currentClient.connectedTime = std::stol(match[1]);
            }
        }
    }
    // 保存最后一个客户端
    if (hasClient)
    {
        clients.push_back(currentClient);
    }
    // 为每个客户端获取IP地址和主机名
    for (auto &client : clients)
    {
        // 通过ARP表获取IP地址
        std::string arpCommand = "arp -a | grep " + client.macAddress + " | awk '{print $2}' | tr -d '()'";
        std::string ip = executeCommand(arpCommand);
        if (!ip.empty() && ip.back() == '\n')
        {
            ip.pop_back();
        }
        client.ipAddress = ip.empty() ? "unknown" : ip;

        // 主机名解析
        if (client.ipAddress != "unknown")
        {
            std::string hostnameCommand = "nslookup " + client.ipAddress + " | grep 'name =' | awk '{print $4}'";
            std::string hostname = executeCommand(hostnameCommand);
            if (!hostname.empty() && hostname.back() == '\n')
            {
                hostname.pop_back();
                hostname.pop_back();
            }
            client.hostname = hostname.empty() ? "unknown" : hostname;
        }
        else
        {
            client.hostname = "unknown";
        }
    }
    return clients;
#else
    return std::vector<ClientInfo>();
#endif // _WIN32
}

bool WifiInterface::disconnectClient(const std::string &macAddress)
{
#ifndef _WIN32
    if (!isAPRunning_)
    {
        return false;
    }

    // 验证MAC地址格式
    std::regex macRegex("^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$");
    if (!std::regex_match(macAddress, macRegex))
    {
        std::cout << "Invalid MAC address format:" << macAddress << std::endl;
        return false;
    }

    std::string command = "hostapd_cli -i " + apInterface_ + " deauthenticate " + macAddress;
    return executeCommandWithResult(command);
#else
    return false;
#endif // _WIN32
}
std::string WifiInterface::getAPIPAddress()
{
#ifndef _WIN32
    if (!isAPRunning_)
    {
        return "";
    }

    // 获取AP接口的实际IP地址
    std::string command = "ip addr show " + apInterface_ + " | grep 'inet ' | awk '{print $2}' | cut -d/ -f1";
    std::string ip = executeCommand(command);

    if (!ip.empty() && ip.back() == '\n')
    {
        ip.pop_back();
    }

    if (!ip.empty())
    {
        return ip;
    }

    // 如果未获取到IP地址，返回"unknown"
    return "unknown";
#else
    return "";
#endif // _WIN32
}

bool WifiInterface::isAPRunning()
{
#ifndef _WIN32
    // 检查hostapd服务是否正在运行
    std::string command = "pidof hostapd";
    std::string result = executeCommand(command);

    // 同时检查AP接口状态
    std::string interfaceCommand = "ip link show " + apInterface_ + " | grep -q 'state UP'";
    bool interfaceUp = executeCommandWithResult(interfaceCommand);

    return !result.empty() && interfaceUp;
#else
    return true;
#endif // _WIN32
}

bool WifiInterface::saveAPConfig()
{
#ifndef _WIN32
    std::ofstream file("/etc/wifi_ap_config.conf");
    if (!file.is_open())
    {
        std::cout << "Failed to open AP config file for writing." << std::endl;
        return false;
    }

    file << "ssid=" << apConfig_.ssid << std::endl;
    file << "password=" << apConfig_.password << std::endl;
    file << "channel=" << apConfig_.channel << std::endl;
    file << "security=" << static_cast<int>(apConfig_.security) << std::endl;
    file << "maxClients=" << apConfig_.maxClients << std::endl;
    file.close();
    return true;
#else
    return false;
#endif // _WIN32
}

bool WifiInterface::loadAPConfig()
{
#ifndef _WIN32
    std::ifstream file("/etc/wifi_ap_config.conf");
    if (!file.is_open())
    {
        std::cout << "Failed to open AP config file." << std::endl;
        return false;
    }
    std::string line;
    while (std::getline(file, line))
    {
        size_t pos = line.find('=');
        if (pos != std::string::npos)
        {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            if (key == "ssid")
            {
                apConfig_.ssid = value;
            }
            else if (key == "password")
            {
                apConfig_.password = value;
            }
            else if (key == "channel")
            {
                try
                {
                    apConfig_.channel = std::stoi(value);
                }
                catch (...)
                {
                    apConfig_.channel = 6; // 默认值
                }
            }
            else if (key == "security")
            {
                try
                {
                    int securityInt = std::stoi(value);
                    apConfig_.security = static_cast<SecurityMode>(securityInt);
                }
                catch (...)
                {
                    apConfig_.security = SecurityMode::WPA2_PSK; // 默认值
                }
            }
            else if (key == "maxClients")
            {
                try
                {
                    apConfig_.maxClients = std::stoi(value);
                }
                catch (...)
                {
                    apConfig_.maxClients = 5; // 默认值
                }
            }
        }
    }

    file.close();
    return true;
#else
    return false;
#endif // _WIN32
}

bool WifiInterface::validateAPConfig(const APConfig &config)
{
#ifndef _WIN32
    if (config.ssid.empty() || config.ssid.length() > 32)
    {
        std::cout << "SSID must be between 1 and 32 characters long." << std::endl;
        return false;
    }
    // SSID特殊字符验证
    std::regex ssidRegex("^[a-zA-Z0-9_\\-\\s]+$");
    if (!std::regex_match(config.ssid, ssidRegex))
    {
        std::cout << "The SSID contains illegal characters. Only letters, numbers, underscores, hyphens, and spaces are allowed." << std::endl;
        return false;
    }
    if (config.security != SecurityMode::OPEN)
    {
        if (config.password.length() < 8 || config.password.length() > 64)
        {
            std::cout << "Password must be between 8 and 64 characters long." << std::endl;
            return false;
        }
    }

    if (config.channel < 1 || config.channel > 13)
    {
        std::cout << "Channel must be between 1 and 13." << std::endl;
        return false;
    }

    if (!validateMaxClients(config.maxClients))
    {
        std::cout << "Max clients must be between 1 and 5." << std::endl;
        return false;
    }
    return true;
#else
    return true;
#endif // _WIN32
}

bool WifiInterface::validateMaxClients(int maxClients)
{
#ifndef _WIN32
    return (maxClients >= 1 && maxClients <= 5);
#else
    return true;
#endif // _WIN32
}