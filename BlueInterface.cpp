#include "BlueInterface.h"

/*
TODO:
    蓝牙开启
    蓝牙关闭
    获取蓝牙状态
    蓝牙扫描
    获取蓝牙扫描结果
    蓝牙配对
    取消配对设备
    获取已配对设备
    连接蓝牙设备
    断开蓝牙设备连接
    获取已连接设备
    修改设备名称
*/
BlueInterface::BlueInterface() : bluetoothEnabled_(false), isScanning_(false)
{
    loadDeviceConfig();
}

BlueInterface::~BlueInterface()
{
}

bool BlueInterface::validateBluetoothState()
{
#ifndef _WIN32
    if (!bluetoothEnabled_)
    {
        std::cout << "Bluetooth is not enabled." << std::endl;
        return false;
    }
#endif
    return true;
}
bool BlueInterface::validateDeviceAddress(const std::string &deviceAddress)
{
    if (deviceAddress.empty())
    {
        std::cout << "Invalid device MAC address." << std::endl;
        return false;
    }
    return true;
}
bool BlueInterface::validateDevice(const BluetoothDevice &device)
{
    return validateDeviceAddress(device.address);
}

void BlueInterface::loadDeviceConfig()
{
#ifndef _WIN32
    std::ifstream configFile("/etc/bluetooth_devices.conf");
    if (!configFile.is_open())
    {
        return;
    }

    std::string line;
    while (std::getline(configFile, line))
    {
        size_t pos = line.find('|');
        if (pos != std::string::npos)
        {
            std::string address = line.substr(0, pos);
            bool autoConnect = line.substr(pos + 1) == "1";
            autoConnectDevices_[address] = autoConnect;
        }
    }
    configFile.close();
#else
    return;
#endif // _WIN32
}

void BlueInterface::saveDeviceConfig()
{
#ifndef _WIN32
    std::ofstream configFile("/etc/bluetooth_devices.conf");
    if (!configFile.is_open())
    {
        std::cout << "Cannot create Bluetooth device profile" << std::endl;
        return;
    }

    for (const auto &device : autoConnectDevices_)
    {
        configFile << device.first << "|" << (device.second ? "1" : "0") << std::endl;
    }
    configFile.close();
#else
    return;
#endif // _WIN32
}

std::string BlueInterface::executeCommand(const std::string &command)
{
#ifndef _WIN32
    // 通过临时文件的方式解决阻塞问题
    std::srand(std::time(nullptr));
    std::string tempFile = "/tmp/cmd_output_" + std::to_string(std::rand()) + ".txt";

    std::string fullCommand = "timeout 10s " + command + " > " + tempFile + " 2>&1";
    system(fullCommand.c_str());

    // 读取临时文件内容
    std::string output = "";
    FILE *file = fopen(tempFile.c_str(), "r");
    if (file)
    {
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), file) != nullptr)
        {
            output += buffer;
        }
        fclose(file);

        // 删除临时文件
        std::string cleanupCommand = "rm -f " + tempFile;
        system(cleanupCommand.c_str());
    }
    else
    {
        output = "Error: Failed to read temporary file";
    }

    return output;
#else
    return "";
#endif // _WIN32
}

bool BlueInterface::executeCommandWithResult(const std::string &command)
{
#ifndef _WIN32
    int result = system(command.c_str());
    return (result == 0);
#else
    return true;
#endif // _WIN32
}

bool BlueInterface::enableBluetooth()
{
#ifndef _WIN32
    // 1. 启动bluetoothd服务
    std::string startDaemonCommand = "bluetoothd -n &";
    if (!executeCommandWithResult(startDaemonCommand))
    {
        std::cout << "Failed to start bluetoothd daemon." << std::endl;
        return false;
    }

    sleep(2); // 等待2s,确保bluetoothd服务启动完成

    // 2. 启用蓝牙接口
    std::string upCommand = "hciconfig hci0 up";
    if (!executeCommandWithResult(upCommand))
    {
        std::cout << "Failed to enable Bluetooth." << std::endl;
        return false;
    }

    // 3. 开启蓝牙电源
    std::string powerOnCommand = "timeout 5 bluetoothctl -- power on";
    if (executeCommandWithResult(powerOnCommand))
    {
        bluetoothEnabled_ = true;
        std::cout << "Bluetooth enabled." << std::endl;
        return true;
    }
    else
    {
        // 如果timeout失败，则直接发送power on命令
        std::string alternativeCommand = "printf 'power on\nquit' | bluetoothctl";
        if (executeCommandWithResult(alternativeCommand))
        {
            bluetoothEnabled_ = true;
            std::cout << "Bluetooth enabled." << std::endl;
            return true;
        }
    }

    std::cout << "Failed to enable Bluetooth." << std::endl;
    return false;
#else
    bluetoothEnabled_ = true;
    return true;
#endif // _WIN32
}

bool BlueInterface::disableBluetooth()
{
#ifndef _WIN32
    if (isScanning_)
    {
        stopScanning();
    }

    // 关闭蓝牙电源
    std::string powerOffCommand = "timeout 5 bluetoothctl -- power off";
    if (!executeCommandWithResult(powerOffCommand))
    {
        std::string alternativeCommand = "printf 'power off\nquit' | bluetoothctl";
        executeCommandWithResult(alternativeCommand);
    }

    // 关闭蓝牙接口
    std::string downCommand = "hciconfig hci0 down";
    executeCommandWithResult(downCommand);

    // 停止Bluetoothd服务
    std::string stopDaemonCommand = "killall bluetoothd";
    executeCommandWithResult(stopDaemonCommand);
    sleep(2); // 等待2s,确保bluetoothd服务停止完成
    bluetoothEnabled_ = false;
    std::cout << "Bluetooth disabled." << std::endl;
    return true;
#else
    bluetoothEnabled_ = false;
    return true;
#endif // _WIN32
}

bool BlueInterface::isBluetoothEnabled()
{
#ifndef _WIN32
    // 检测bluetoothd服务是否运行
    std::string daemonCommand = "ps aux | grep -q '[b]luetoothd' && echo 'running' || echo 'stopped'";
    std::string daemonStatus = executeCommand(daemonCommand);

    if (daemonStatus.find("running") != std::string::npos)
    {
        // 检测蓝牙接口状态
        std::string hciCommand = "hciconfig hci0 | grep -q 'UP RUNNING' && echo 'enabled' || echo 'disabled'";
        std::string hciResult = executeCommand(hciCommand);
        bluetoothEnabled_ = (hciResult.find("enabled") != std::string::npos);
    }
    else
    {
        bluetoothEnabled_ = false;
    }

    return bluetoothEnabled_;
#else
    return bluetoothEnabled_;
#endif
}

bool BlueInterface::startScanning(int duration)
{
#ifndef _WIN32
    if (!validateBluetoothState())
    {
        return false;
    }

    if (isScanning_)
    {
        std::cout << "Scanning is already running." << std::endl;
        return false;
    }

    clearScanResults();

    // 关闭配对请求的验证，解决后台终端需要输入yes确认的问题
    std::string agentOffCommand = "bluetoothctl -- agent off";
    std::string agentOffOutput = executeCommand(agentOffCommand);
    if (agentOffOutput.find("Agent unregistered") != std::string::npos)
    {
        std::cout << "Agent unregistered successfully." << std::endl;
    }
    else
    {
        std::cout << "Warning:No agent was registered, continuing scan." << std::endl;
    }

    // 使能设备可配对
    std::string pairableOnCommand = "bluetoothctl -- pairable on";
    std::string pairableOnOutput = executeCommand(pairableOnCommand);
    if (pairableOnOutput.find("Changing pairable on succeeded") != std::string::npos)
    {
        std::cout << "Pairable mode enabled successfully." << std::endl;
    }
    else
    {
        std::cout << "Failed to enable pairable mode." << std::endl;
        return false;
    }

    // 使能设备可发现
    std::string discoverableOnCommand = "bluetoothctl -- discoverable on";
    std::string discoverableOnOutput = executeCommand(discoverableOnCommand);
    if (discoverableOnOutput.find("Changing discoverable on succeeded") != std::string::npos)
    {
        std::cout << "Discoverable mode enabled successfully." << std::endl;
    }
    else
    {
        std::cout << "Failed to enable discoverable mode." << std::endl;
        return false;
    }

    std::string scanCommand = "timeout " + std::to_string(duration) + " bluetoothctl -- scan on 2>/dev/null";
    std::cout << "Start scanning for Bluetooth devices. Duration: " << duration << " seconds." << std::endl;

    std::string scanOutput = executeCommand(scanCommand);

    // 解析扫描过程中发现的设备
    if (!parseScanResults(scanOutput))
    {
        std::cout << "Failed to parse scan results." << std::endl;
        return false;
    }

    // 扫描完成后获取设备列表
    std::string devicesCommand = "bluetoothctl -- devices";
    std::string devicesOutput = executeCommand(devicesCommand);
    parseScanResults(devicesOutput);

    isScanning_ = false;

    // 获取已配对设备数量用于统计
    auto pairedDevices = getPairedDevices();
    std::cout << "Scan completed, found " << scanResults_.size() << " unpaired devices. (" << pairedDevices.size() << " paired devices filtered out)" << std::endl;
    return true;
#else
    isScanning_ = true;
    return true;
#endif // _WIN32
}

bool BlueInterface::stopScanning()
{
#ifndef _WIN32
    if (!isScanning_)
    {
        std::cout << "Scanning is not running." << std::endl;
        return false;
    }

    std::string stopCommand = "bluetoothctl -- scan off";
    if (!executeCommandWithResult(stopCommand))
    {
        std::cout << "Failed to stop scanning." << std::endl;
        return false;
    }

    sleep(1);

    // 获取最终的设备列表
    std::string devicesCommand = "bluetoothctl -- devices";
    std::string devicesOutput = executeCommand(devicesCommand);
    if (!parseScanResults(devicesOutput))
    {
        std::cout << "Failed to parse scan results." << std::endl;
        return false;
    }

    isScanning_ = false;

    // 获取已配对设备数量用于统计
    auto pairedDevices = getPairedDevices();
    std::cout << "Scan stopped, found " << scanResults_.size() << " unpaired devices. (" << pairedDevices.size() << " paired devices filtered out)" << std::endl;
    return true;
#else
    isScanning_ = false;
    return true;
#endif // _WIN32
}

bool BlueInterface::parseScanResults(const std::string &scanOutput)
{
#ifndef _WIN32
    std::istringstream iss(scanOutput);
    std::string line;

    // 获取已配对设备列表，用于过滤
    auto pairedDevices = getPairedDevices();
    std::set<std::string> pairedDeviceAddresses;
    for (const auto &pairedDevice : pairedDevices)
    {
        pairedDeviceAddresses.insert(pairedDevice.address);
    }

    while (std::getline(iss, line))
    {
        if (!line.empty() && line.back() == '\n')
        {
            line.pop_back();
        }
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        // 处理两种格式：
        // 1. [NEW] Device MAC地址 设备名称(实时扫描输出)
        // 2. Device MAC地址 设备名称 (bluetoothctl -- devices输出)

        BluetoothDevice device;
        bool validDevice = false;

        // 检查是否是[NEW] Device格式
        if (line.find("[NEW] Device") != std::string::npos)
        {
            size_t devicePos = line.find("Device");
            if (devicePos != std::string::npos)
            {
                std::string deviceLine = line.substr(devicePos);
                validDevice = parseDeviceLine(deviceLine, device);
            }
        }
        // 检查是否是Device格式
        else if (line.find("Device") == 0)
        {
            validDevice = parseDeviceLine(line, device);
        }

        if (validDevice)
        {
            // 检查设备是否已配对，如果已配对则跳过
            if (pairedDeviceAddresses.find(device.address) != pairedDeviceAddresses.end())
            {
                std::cout << "Skipping paired device: " << device.name << " (" << device.address << ")" << std::endl;
                continue; // 跳过已配对的设备
            }

            // 检查是否已经存在相同MAC地址的设备
            bool exists = false;
            for (auto &existingDevice : scanResults_)
            {
                if (existingDevice.address == device.address)
                {
                    exists = true;
                    // 更新设备信息
                    if (!device.name.empty() && device.name != "unknown")
                    {
                        existingDevice.name = device.name;
                    }
                    break;
                }
            }

            if (!exists)
            {
                // 设置默认值
                device.isPaired = false;
                device.isConnected = false;
                device.autoConnect = false;

                scanResults_.push_back(device);
            }
        }
    }

    return true;
#else
    return true;
#endif // _WIN32
}

bool BlueInterface::parseDeviceLine(const std::string &line, BluetoothDevice &device)
{
    std::istringstream lineStream(line);
    std::string token;

    // 跳过 "Device"
    lineStream >> token;

    // 获取MAC地址
    if (lineStream >> token)
    {
        // 验证MAC地址格式
        if (token.length() == 17 &&
            token[2] == ':' && token[5] == ':' && token[8] == ':' &&
            token[11] == ':' && token[14] == ':')
        {
            device.address = token;

            // 获取设备名称
            std::string name;
            std::getline(lineStream, name);

            size_t start = name.find_first_not_of(" \t");
            size_t end = name.find_last_not_of(" \t");
            if (start != std::string::npos && end != std::string::npos)
            {
                device.name = name.substr(start, end - start + 1);
            }
            else
            {
                device.name = "unknown";
            }

            return true;
        }
    }
    return false;
}

std::vector<BluetoothDevice> BlueInterface::getScanResults()
{
    return scanResults_;
}

void BlueInterface::clearScanResults()
{
    scanResults_.clear();
}

bool BlueInterface::pairDevice(const BluetoothDevice &device)
{
#ifndef _WIN32
    if (!validateBluetoothState() || !validateDevice(device))
    {
        return false;
    }

    std::string pairCommand = "timeout 10 bluetoothctl -- pair " + device.address;
    std::string pairOutput = executeCommand(pairCommand);

    if (pairOutput.find("Pairing successful") != std::string::npos)
    {
        std::cout << "Pairing successful for device " << device.address << "." << std::endl;
        // 更新设备配对状态
        for (auto &dev : scanResults_)
        {
            if (dev.address == device.address)
            {
                dev.isPaired = true;
                break;
            }
        }
        std::cout << "Device " << device.address << " is now paired." << std::endl;
        return true;
    }
    else
    {
        std::cout << "Pairing timed out. Try pairing again." << std::endl;
        return false;
    }
#else
    return true;
#endif // _WIN32
}

bool BlueInterface::unpairDevice(const BluetoothDevice &device)
{
#ifndef _WIN32
    if (!validateDevice(device))
    {
        return false;
    }

    std::cout << "Unpairing device " << device.address << "..." << std::endl;

    std::string unpairCommand = "bluetoothctl -- remove " + device.address;
    if (executeCommandWithResult(unpairCommand))
    {
        // 更新设备配对状态
        for (auto &dev : scanResults_)
        {
            if (dev.address == device.address)
            {
                dev.isPaired = false;
                dev.isConnected = false;
                dev.autoConnect = false;
                break;
            }
        }

        // 断开设备连接
        std::string disconnectCommand = "bluetoothctl -- disconnect " + device.address;
        executeCommand(disconnectCommand);

        // 失能信任状态
        std::string trustCommand = "bluetoothctl -- untrust " + device.address;
        executeCommand(trustCommand);

        // 从自动连接配置文件中移除该设备
        auto it = autoConnectDevices_.find(device.address);
        if (it != autoConnectDevices_.end())
        {
            autoConnectDevices_.erase(it);
            saveDeviceConfig();
            std::cout << "Device " << device.address << " has been removed from auto-connect configuration." << std::endl;
        }

        std::cout << "Device " << device.address << " is now unpaired." << std::endl;
        return true;
    }
    return false;
#else
    return true;
#endif // _WIN32
}

std::vector<BluetoothDevice> BlueInterface::getPairedDevices()
{
    std::vector<BluetoothDevice> pairedDevices;
#ifndef _WIN32
    std::string pairedCommand = "bluetoothctl -- paired-devices";
    std::string pairedOutput = executeCommand(pairedCommand);

    std::istringstream iss(pairedOutput);
    std::string line;
    while (std::getline(iss, line))
    {
        if (line.find("Device") == 0)
        {
            std::istringstream lineStream(line);
            std::string token;
            BluetoothDevice device;

            // 跳过 "Device"
            lineStream >> token;

            // 获取MAC地址
            if (lineStream >> token)
            {
                device.address = token;
            }

            // 获取设备名称
            std::string name;
            std::getline(lineStream, name);
            size_t start = name.find_first_not_of(" \t");
            size_t end = name.find_last_not_of(" \t");
            if (start != std::string::npos && end != std::string::npos)
            {
                device.name = name.substr(start, end - start + 1);
            }
            else
            {
                device.name = "unknown";
            }

            device.isPaired = true;
            device.isConnected = false;
            device.autoConnect = false;
            pairedDevices.push_back(device);
        }
    }
#endif // _WIN32
    return pairedDevices;
}

bool BlueInterface::connectToDevice(const BluetoothDevice &device)
{
#ifndef _WIN32
    if (!validateBluetoothState() || !validateDevice(device))
    {
        return false;
    }

    std::cout << "Connecting to device " << device.address << "..." << std::endl;

    // 检查设备是否已配对
    bool isPaired = false;
    for (const auto &dev : scanResults_)
    {
        if (dev.address == device.address && dev.isPaired)
        {
            isPaired = true;
            break;
        }
    }

    // 如果扫描结果中没有配对信息，检查已配对设备列表
    if (!isPaired)
    {
        auto pairedDevices = getPairedDevices();
        for (const auto &pairedDevice : pairedDevices)
        {
            if (pairedDevice.address == device.address)
            {
                isPaired = true;
                // 更新扫描结果
                bool foundInScanResults = false;
                for (auto &scannedDevice : scanResults_)
                {
                    if (scannedDevice.address == device.address)
                    {
                        scannedDevice.isPaired = true;
                        scannedDevice.name = device.name;
                        foundInScanResults = true;
                        break;
                    }
                }
                if (!foundInScanResults)
                {
                    BluetoothDevice newDevice = device;
                    newDevice.isPaired = true;
                    newDevice.isConnected = false;
                    newDevice.autoConnect = false;
                    scanResults_.push_back(newDevice);
                }
                break;
            }
        }
    }

    if (!isPaired)
    {
        std::cout << "Device " << device.address << " is not paired. Please pair first." << std::endl;
        return false;
    }

    // 使能受信任状态(自动重连功能)
    std::string trustCommand = "bluetoothctl -- trust " + device.address;
    std::string trustOutput = executeCommand(trustCommand);
    if (trustOutput.find("Changing") != std::string::npos || trustOutput.find("succeeded") != std::string::npos)
    {
        std::cout << "Device " << device.address << " is now trusted." << std::endl;
    }
    else
    {
        std::cout << "Warning: Failed to set trust status. The connection continues..." << std::endl;
    }

    std::string connectCommand = "timeout 10 bluetoothctl -- connect " + device.address;
    std::string connectOutput = executeCommand(connectCommand);

    if (connectOutput.find("Connection successful") != std::string::npos)
    {
        // 更新设备连接状态
        for (auto &dev : scanResults_)
        {
            if (dev.address == device.address)
            {
                dev.isConnected = true;
                // dev.autoConnect = true;
                break;
            }
        }

        // 保存自动连接设置
        // autoConnectDevices_[device.address] = true;
        // saveDeviceConfig();
        sleep(1); // 等待1秒确保连接完成
        std::cout << "Connection successful to device " << device.address << std::endl;
        return true;
    }
    else
    {
        std::cout << "Failed to connect to device " << device.address << std::endl;
        return false;
    }
#else
    return true;
#endif
}

bool BlueInterface::disconnectDevice(const BluetoothDevice &device)
{
#ifndef _WIN32
    if (!validateBluetoothState() || !validateDevice(device))
    {
        return false;
    }

    std::cout << "Disconnecting device " << device.address << "..." << std::endl;
    std::string disconnectCommand = "bluetoothctl -- disconnect " + device.address;
    std::string disconnectOutput = executeCommand(disconnectCommand);
    if (disconnectOutput.find("Successful disconnected") != std::string::npos)
    {
        // 更新设备连接状态
        for (auto &dev : scanResults_)
        {
            if (dev.address == device.address)
            {
                dev.isConnected = false;
                break;
            }
        }

        // 断开连接后，如果已配对的设备不在保存列表中，添加到保存列表
        auto pairedDevices = getPairedDevices();
        bool isPaired = false;
        for (const auto &pairedDevice : pairedDevices)
        {
            if (pairedDevice.address == device.address)
            {
                isPaired = true;
                break;
            }
        }
        if (isPaired)
        {
            // 检查设备是否已经在保存列表中
            auto it = autoConnectDevices_.find(device.address);
            if (it == autoConnectDevices_.end())
            {
                // 设备不在保存列表中，添加到保存列表[默认不自动连接]
                autoConnectDevices_[device.address] = false;
                saveDeviceConfig();
                std::cout << "Device " << device.address << " has been added to saved devices list." << std::endl;
            }
        }
        return true;
    }
    return false;
#else
    return true;
#endif
}

std::vector<BluetoothDevice> BlueInterface::getConnectedDevices()
{
    std::vector<BluetoothDevice> connectedDevices;
#ifndef _WIN32
    // 通过系统命令实时获取已连接设备
    std::string connectedCommand = "bluetoothctl -- info | grep -E 'Device|Name|Alias|Connected: yes'";
    std::string connectedOutput = executeCommand(connectedCommand);

    std::istringstream iss(connectedOutput);
    std::string line;
    BluetoothDevice currentDevice;
    std::string currentAddress;

    while (std::getline(iss, line))
    {
        if (line.find("Device") == 0)
        {
            // 解析设备地址
            std::istringstream lineStream(line);
            std::string token;
            lineStream >> token; // 跳过 "Device"
            if (lineStream >> token)
            {
                currentAddress = token;
                currentDevice.address = currentAddress;
                currentDevice.name = "unknown"; // 默认名称
            }
        }
        else if (line.find("Name:") != std::string::npos)
        {
            // 获取设备名称
            size_t pos = line.find("Name:");
            if (pos != std::string::npos)
            {
                std::string name = line.substr(pos + 5); // 跳过 "Name:"
                size_t start = name.find_first_not_of(" \t");
                size_t end = name.find_last_not_of(" \t");
                if (start != std::string::npos && end != std::string::npos)
                {
                    currentDevice.name = name.substr(start, end - start + 1);
                }
            }
        }
        else if (line.find("Alias:") != std::string::npos)
        {
            // 获取设备别名
            size_t pos = line.find("Alias:");
            if (pos != std::string::npos)
            {
                std::string alias = line.substr(pos + 6); // 跳过 "Alias:"
                size_t start = alias.find_first_not_of(" \t");
                size_t end = alias.find_last_not_of(" \t");
                if (start != std::string::npos && end != std::string::npos)
                {
                    std::string aliasName = alias.substr(start, end - start + 1);
                    // 如果别名不是默认值，使用别名
                    if (aliasName != "unknown" && aliasName != currentDevice.name)
                    {
                        currentDevice.name = aliasName;
                    }
                }
            }
        }
        else if (line.find("Connected: yes") != std::string::npos && !currentAddress.empty())
        {
            currentDevice.isConnected = true;
            currentDevice.isPaired = true;
            currentDevice.autoConnect = false;

            // 如果名称仍然是unknown，尝试从已配对设备中获取名称
            if (currentDevice.name == "unknown")
            {
                auto pairedDevices = getPairedDevices();
                for (const auto &pairedDevice : pairedDevices)
                {
                    if (pairedDevice.address == currentAddress)
                    {
                        currentDevice.name = pairedDevice.name;
                        break;
                    }
                }
            }

            connectedDevices.push_back(currentDevice);
            currentAddress.clear(); // 重置当前地址
        }
    }
#endif
    return connectedDevices;
}

bool BlueInterface::isDeviceConnected(const std::string &deviceAddress)
{
#ifndef _WIN32
    // 通过系统命令实时检测连接状态
    std::string connectedCommand = "bluetoothctl -- info " + deviceAddress + " | grep 'Connected: yes'";
    std::string connectedOutput = executeCommand(connectedCommand);

    if (!connectedOutput.empty())
    {
        return true;
    }

    // 如果系统命令检测不到，再从扫描结果中查找
    for (const auto &dev : scanResults_)
    {
        if (dev.address == deviceAddress)
        {
            return dev.isConnected;
        }
    }

    return false;
#endif
    return false;
}

bool BlueInterface::setAutoConnect(const std::string &deviceAddress, bool autoConnect)
{
#ifndef _WIN32
    autoConnectDevices_[deviceAddress] = autoConnect;
    saveDeviceConfig();

    if (autoConnect)
    {
        std::string trustCommand = "bluetoothctl -- trust " + deviceAddress;
        std::string trustOutput = executeCommand(trustCommand);
        if (trustOutput.find("Changing") != std::string::npos || trustOutput.find("succeeded") != std::string::npos)
        {
            std::cout << "Device " << deviceAddress << " is now trusted." << std::endl;
        }
    }
    return true;
#else
    return false;
#endif // _WIN32
}

bool BlueInterface::autoConnectToPairedDevices()
{
#ifndef _WIN32
    std::cout << "Auto connecting to paired devices..." << std::endl;
    if (!validateBluetoothState())
    {
        return false;
    }

    // 获取已配对设备
    auto pairedDevices = getPairedDevices();
    if (pairedDevices.empty())
    {
        std::cout << "No paired devices found." << std::endl;
        return false;
    }

    std::cout << "Found " << pairedDevices.size() << " paired devices." << std::endl;

    // 更新扫描结果中的配对状态
    for (const auto &pairedDevice : pairedDevices)
    {
        bool foundInScanResults = false;
        for (auto &scannedDevice : scanResults_)
        {
            if (scannedDevice.address == pairedDevice.address)
            {
                scannedDevice.isPaired = true;
                scannedDevice.name = pairedDevice.name;
                foundInScanResults = true;
                break;
            }
        }

        // 如果配对设备不在扫描结果中，添加到扫描结果
        if (!foundInScanResults)
        {
            BluetoothDevice newDevice = pairedDevice;
            newDevice.isPaired = true;
            newDevice.isConnected = false;
            newDevice.autoConnect = false;
            scanResults_.push_back(newDevice);
        }
    }

    bool connected = false;
    int attemptCount = 0;
    int successCount = 0;

    for (const auto &device : pairedDevices)
    {
        auto it = autoConnectDevices_.find(device.address);
        if (it != autoConnectDevices_.end() && it->second)
        {
            attemptCount++;
            if (!isDeviceConnected(device.address))
            {
                std::cout << "Try to connect the device automatically: " << device.name << "(" << device.address << ")" << std::endl;

                if (connectToDevice(device))
                {
                    std::cout << "Auto Connect Success" << std::endl;
                    connected = true;
                    successCount++;

                    // 更新自动连接状态
                    autoConnectDevices_[device.address] = true;
                }
                else
                {
                    std::cout << "Auto Connect Failed" << std::endl;
                }
            }
            else
            {
                std::cout << "Device " << device.name << "(" << device.address << ") is already connected." << std::endl;
                connected = true;
                successCount++;
            }
        }
    }

    if (attemptCount == 0)
    {
        std::cout << "No devices have auto-connect enabled." << std::endl;
    }
    else if (successCount > 0)
    {
        std::cout << "Auto connection completed: " << successCount << "/" << attemptCount << " devices connected successfully." << std::endl;
    }
    else
    {
        std::cout << "Auto connection failed: 0/" << attemptCount << " devices connected." << std::endl;
    }

    return connected;
#else
    return false;
#endif // _WIN32
}

bool BlueInterface::setDeviceName(const std::string &deviceAddress, const std::string &deviceName)
{
#ifndef _WIN32
    std::string setNameCommand = "bluetoothctl -- set-alias \"" + deviceName + "\"";
    return executeCommandWithResult(setNameCommand);
#else
    return true;
#endif
}

std::string BlueInterface::getDeviceName(const std::string &deviceAddress)
{
    for (const auto &dev : scanResults_)
    {
        if (dev.address == deviceAddress)
        {
            return dev.name;
        }
    }
    return "unknown";
}

bool BlueInterface::setAdapterName(const std::string &adapterName)
{
#ifndef _WIN32
    if (adapterName.empty())
    {
        std::cout << "Invalid adapter name." << std::endl;
        return false;
    }

    std::cout << "Setting adapter name to " << adapterName << " ..." << std::endl;

    std::string setNameCommand = "timeout 5 bluetoothctl -- system-alias \"" + adapterName + "\"";
    std::string result = executeCommand(setNameCommand);

    if ((result.find("Changing") != std::string::npos &&
         result.find("succeeded") != std::string::npos) ||
        result.find("Alias set to") != std::string::npos ||
        result.find("Controller") != std::string::npos)
    {
        adapterName_ = adapterName;
        std::cout << "Bluetooth adapter name set successfully." << std::endl;
        // 等待1秒，确保系统有足够时间更新
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        std::string currentName = getAdapterName();

        return true;
    }
    else
    {
        std::cout << "Failed to set Bluetooth adapter name. Output: " << result << std::endl;
        return false;
    }
#else
    adapterName_ = adapterName;
    return true;
#endif
}

bool BlueInterface::resetAdapterName()
{
#ifndef _WIN32
    std::cout << "Resetting Bluetooth adapter name to default..." << std::endl;

    std::string resetCommand = "timeout 5 bluetoothctl -- reset-alias";
    std::string result = executeCommand(resetCommand);

    if (result.find("Alias removed") != std::string::npos ||
        result.find("Changing") != std::string::npos ||
        result.find("Controller") != std::string::npos)
    {
        std::string currentName = getAdapterName();
        adapterName_ = currentName;

        std::cout << "Bluetooth adapter name reset to: " << adapterName_ << std::endl;
        return true;
    }
    else
    {
        std::cout << "Failed to reset Bluetooth adapter name. Output: " << result << std::endl;
        return false;
    }
#else
    adapterName_ = "Default Bluetooth";
    return true;
#endif
}

std::string BlueInterface::getAdapterName()
{
#ifndef _WIN32
    std::string showCommand = "timeout 3 bluetoothctl -- show 2>/dev/null | grep 'Alias:' | sed 's/^[[:space:]]*Alias:[[:space:]]*//'";
    std::string aliasOutput = executeCommand(showCommand);

    if (!aliasOutput.empty())
    {
        if (aliasOutput.back() == '\n')
        {
            aliasOutput.pop_back();
        }

        size_t start = aliasOutput.find_first_not_of(" \t\r\n");
        size_t end = aliasOutput.find_last_not_of(" \t\r\n");
        if (start != std::string::npos && end != std::string::npos)
        {
            adapterName_ = aliasOutput.substr(start, end - start + 1);
            return adapterName_;
        }
    }

    // 当蓝牙关闭时，返回"Unknown Bluetooth Adapter"
    adapterName_ = "Unknown Bluetooth Adapter";
    return adapterName_;
#else
    return adapterName_.empty() ? "Windows Bluetooth Adapter" : adapterName_;
#endif
}

bool BlueInterface::getAutoConnectStatus(const std::string &deviceAddress)
{
    auto it = autoConnectDevices_.find(deviceAddress);
    return it != autoConnectDevices_.end() && it->second;
}

std::vector<BluetoothDevice> BlueInterface::getSavedDevices()
{
    std::vector<BluetoothDevice> savedDevices;
#ifndef _WIN32
    // 已保存设备 = 已配对但未连接的设备
    auto pairedDevices = getPairedDevices();

    for (const auto &pairedDevice : pairedDevices)
    {
        // 检查设备是否已连接
        bool isConnected = isDeviceConnected(pairedDevice.address);

        if (!isConnected)
        {
            BluetoothDevice savedDevice = pairedDevice;
            savedDevice.isConnected = false;

            // 检查自动连接设置
            auto it = autoConnectDevices_.find(pairedDevice.address);
            if (it != autoConnectDevices_.end())
            {
                savedDevice.autoConnect = it->second;
            }
            else
            {
                savedDevice.autoConnect = false;
            }

            savedDevices.push_back(savedDevice);
        }
    }
#endif // _WIN32
    return savedDevices;
}