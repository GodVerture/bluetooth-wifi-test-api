#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <iomanip>

#define WIFI_TEST
// #define BLUE_TEST

#ifdef WIFI_TEST
#include "WifiInterface.h"
#endif // WIFI_TEST
#ifdef BLUE_TEST
#include "BlueInterface.h"
#endif // BLUE_TEST

#ifdef BLUE_TEST
void displayBluetoothDevices(const std::vector<BluetoothDevice> &devices)
{
    std::cout << "\n=== 蓝牙设备列表 ===" << std::endl;
    std::cout << std::setw(5) << "序号"
              << std::setw(24) << "设备名称"
              << std::setw(22) << "MAC地址"
              << std::setw(11) << "配对"
              << std::setw(9) << "连接"
              << std::setw(15) << "自动连接" << std::endl;
    std::cout << std::string(70, '-') << std::endl;

    if (devices.empty())
    {
        std::cout << "没有找到蓝牙设备" << std::endl;
        return;
    }

    for (size_t i = 0; i < devices.size(); ++i)
    {
        const auto &device = devices[i];
        std::cout << std::setw(3) << i + 1
                  << std::setw(25) << (device.name.empty() ? "[未知设备]" : device.name.substr(0, 24))
                  << std::setw(20) << device.address
                  << std::setw(8) << (device.isPaired ? "是" : "否")
                  << std::setw(8) << (device.isConnected ? "是" : "否")
                  << std::setw(10) << (device.autoConnect ? "是" : "否") << std::endl;
    }
}

void displaySavedDevices(BlueInterface &blue)
{
    auto savedDevices = blue.getSavedDevices();
    auto pairedDevices = blue.getPairedDevices();

    if (savedDevices.empty() && pairedDevices.empty())
    {
        std::cout << "没有已保存或已配对的设备。" << std::endl;
        return;
    }

    std::cout << "\n=== 已配对设备管理2 ===" << std::endl;

    if (!pairedDevices.empty())
    {
        std::cout << "\n--- 已配对设备 ---" << std::endl;
        for (size_t i = 0; i < pairedDevices.size(); ++i)
        {
            const auto &device = pairedDevices[i];
            bool isConnected = blue.isDeviceConnected(device.address);
            bool autoConnect = blue.getAutoConnectStatus(device.address);

            std::cout << i + 1 << ". " << device.name << " (" << device.address << ")" << std::endl;
            std::cout << "   状态: " << (isConnected ? "已连接" : "未连接")
                      << " | 自动连接: " << (autoConnect ? "启用" : "禁用") << std::endl;
        }
    }

    if (!savedDevices.empty())
    {
        std::cout << "\n--- 已保存配置 ---" << std::endl;
        for (size_t i = 0; i < savedDevices.size(); ++i)
        {
            const auto &device = savedDevices[i];
            std::cout << pairedDevices.size() + i + 1 << ". " << device.name << " (" << device.address << ")" << std::endl;
            std::cout << "   自动连接: " << (device.autoConnect ? "启用" : "禁用") << std::endl;
        }
    }
}

void displayBluetoothStatus(BlueInterface &blue)
{
    std::cout << "\n=== 蓝牙状态 ===" << std::endl;
    bool enabled = blue.isBluetoothEnabled();
    std::cout << "蓝牙状态: " << (enabled ? "已开启" : "已关闭") << std::endl;

    if (enabled)
    {
        auto connectedDevices = blue.getConnectedDevices();
        auto pairedDevices = blue.getPairedDevices();

        std::cout << "已连接设备: " << connectedDevices.size() << " 个" << std::endl;
        std::cout << "已配对设备: " << pairedDevices.size() << " 个" << std::endl;

        if (!connectedDevices.empty())
        {
            std::cout << "当前连接的设备:" << std::endl;
            for (const auto &device : connectedDevices)
            {
                std::cout << "  - " << device.name << " (" << device.address << ")" << std::endl;
            }
        }
    }
}

void managePairedDevices(BlueInterface &blue)
{
    std::string input;
    int choice;

    while (true)
    {
        auto pairedDevices = blue.getPairedDevices();
        auto savedDevices = blue.getSavedDevices();

        if (pairedDevices.empty() && savedDevices.empty())
        {
            std::cout << "没有已配对或已保存的设备。" << std::endl;
            return;
        }

        std::cout << "\n=== 已配对设备管理1 ===" << std::endl;
        displaySavedDevices(blue);

        std::cout << "\n管理选项:" << std::endl;
        std::cout << "1. 连接设备" << std::endl;
        std::cout << "2. 断开设备连接" << std::endl;
        std::cout << "3. 取消配对设备" << std::endl;
        std::cout << "4. 设置自动连接" << std::endl;
        std::cout << "5. 设置设备名称" << std::endl;
        std::cout << "6. 刷新设备列表" << std::endl;
        std::cout << "0. 返回上级菜单" << std::endl;
        std::cout << "请选择操作: ";

        std::getline(std::cin, input);

        try
        {
            choice = std::stoi(input);
        }
        catch (...)
        {
            std::cout << "无效输入，请重新选择" << std::endl;
            continue;
        }

        // 如果选择取消配对或者刷新设备列表，需要重新获取设备列表
        bool needRefresh = (choice == 3 || choice == 6);

        switch (choice)
        {
        case 1: // 连接设备
        {
            if (pairedDevices.empty())
            {
                std::cout << "没有已配对的设备。" << std::endl;
                break;
            }

            std::cout << "请选择要连接的设备序号 (0取消): ";
            std::getline(std::cin, input);

            try
            {
                int deviceChoice = std::stoi(input);
                if (deviceChoice == 0)
                    break;

                if (deviceChoice > 0 && deviceChoice <= static_cast<int>(pairedDevices.size()))
                {
                    const auto &selectedDevice = pairedDevices[deviceChoice - 1];
                    std::cout << "正在连接设备: " << selectedDevice.name << std::endl;
                    if (blue.connectToDevice(selectedDevice))
                    {
                        std::cout << "连接成功!" << std::endl;
                    }
                    else
                    {
                        std::cout << "连接失败" << std::endl;
                    }
                }
                else
                {
                    std::cout << "无效的设备序号" << std::endl;
                }
            }
            catch (...)
            {
                std::cout << "无效输入" << std::endl;
            }
            break;
        }

        case 2: // 断开设备连接
        {
            if (pairedDevices.empty())
            {
                std::cout << "没有已配对的设备。" << std::endl;
                break;
            }

            std::cout << "请选择要断开连接的设备序号 (0取消): ";
            std::getline(std::cin, input);

            try
            {
                int deviceChoice = std::stoi(input);
                if (deviceChoice == 0)
                    break;

                if (deviceChoice > 0 && deviceChoice <= static_cast<int>(pairedDevices.size()))
                {
                    const auto &selectedDevice = pairedDevices[deviceChoice - 1];
                    std::cout << "正在断开设备: " << selectedDevice.name << std::endl;
                    if (blue.disconnectDevice(selectedDevice))
                    {
                        std::cout << "断开成功!" << std::endl;
                    }
                    else
                    {
                        std::cout << "断开失败" << std::endl;
                    }
                }
                else
                {
                    std::cout << "无效的设备序号" << std::endl;
                }
            }
            catch (...)
            {
                std::cout << "无效输入" << std::endl;
            }
            break;
        }

        case 3: // 取消配对设备
        {
            if (pairedDevices.empty())
            {
                std::cout << "没有已配对的设备。" << std::endl;
                break;
            }

            std::cout << "请选择要取消配对的设备序号 (0取消): ";
            std::getline(std::cin, input);

            try
            {
                int deviceChoice = std::stoi(input);
                if (deviceChoice == 0)
                    break;

                if (deviceChoice > 0 && deviceChoice <= static_cast<int>(pairedDevices.size()))
                {
                    const auto &selectedDevice = pairedDevices[deviceChoice - 1];
                    std::cout << "正在取消配对设备: " << selectedDevice.name << std::endl;
                    if (blue.unpairDevice(selectedDevice))
                    {
                        std::cout << "取消配对成功!" << std::endl;
                        needRefresh = true;
                    }
                    else
                    {
                        std::cout << "取消配对失败" << std::endl;
                    }
                }
                else
                {
                    std::cout << "无效的设备序号" << std::endl;
                }
            }
            catch (...)
            {
                std::cout << "无效输入" << std::endl;
            }
            break;
        }

        case 4: // 设置自动连接
        {
            if (pairedDevices.empty())
            {
                std::cout << "没有已配对的设备。" << std::endl;
                break;
            }

            std::cout << "请选择要设置自动连接的设备序号 (0取消): ";
            std::getline(std::cin, input);

            try
            {
                int deviceChoice = std::stoi(input);
                if (deviceChoice == 0)
                    break;

                if (deviceChoice > 0 && deviceChoice <= static_cast<int>(pairedDevices.size()))
                {
                    const auto &selectedDevice = pairedDevices[deviceChoice - 1];
                    bool currentAutoConnect = blue.getAutoConnectStatus(selectedDevice.address);
                    std::cout << "当前设备: " << selectedDevice.name << " (" << selectedDevice.address << ")" << std::endl;
                    std::cout << "当前自动连接状态: " << (currentAutoConnect ? "启用" : "禁用") << std::endl;
                    std::cout << "是否启用自动连接? (y/n): ";
                    std::getline(std::cin, input);

                    bool autoConnect = (input == "y" || input == "Y" || input == "是");
                    if (blue.setAutoConnect(selectedDevice.address, autoConnect))
                    {
                        std::cout << "自动连接设置" << (autoConnect ? "启用" : "禁用") << "成功!" << std::endl;
                    }
                    else
                    {
                        std::cout << "设置失败" << std::endl;
                    }
                }
                else
                {
                    std::cout << "无效的设备序号" << std::endl;
                }
            }
            catch (...)
            {
                std::cout << "无效输入" << std::endl;
            }
            break;
        }

        case 5: // 设置设备名称
        {
            if (pairedDevices.empty())
            {
                std::cout << "没有已配对的设备。" << std::endl;
                break;
            }

            std::cout << "请选择要设置设备名称的设备序号 (0取消): ";
            std::getline(std::cin, input);

            try
            {
                int deviceChoice = std::stoi(input);
                if (deviceChoice == 0)
                    break;

                if (deviceChoice > 0 && deviceChoice <= static_cast<int>(pairedDevices.size()))
                {
                    const auto &selectedDevice = pairedDevices[deviceChoice - 1];
                    std::string deviceName;
                    std::cout << "当前设备名称: " << selectedDevice.name << std::endl;
                    std::cout << "请输入新的设备名称: ";
                    std::getline(std::cin, deviceName);

                    if (!deviceName.empty())
                    {
                        if (blue.setDeviceName(selectedDevice.address, deviceName))
                        {
                            std::cout << "新名称设置成功!" << std::endl;
                        }
                        else
                        {
                            std::cout << "设置失败" << std::endl;
                        }
                    }
                }
                else
                {
                    std::cout << "无效的设备序号" << std::endl;
                }
            }
            catch (...)
            {
                std::cout << "无效输入" << std::endl;
            }
            break;
        }

        case 6: // 刷新设备列表
            std::cout << "刷新设备列表..." << std::endl;
            break;

        case 0: // 返回
            return;

        default:
            std::cout << "无效选择，请重新输入" << std::endl;
            break;
        }

        // 如果需要刷新，跳出当前循环重新显示
        if (needRefresh)
        {
            continue;
        }
    }
}

void bluetoothDeviceManagement(BlueInterface &blue)
{
    std::string input;
    int choice;

    while (true)
    {
        auto devices = blue.getScanResults();
        displayBluetoothDevices(devices);
        std::cout << "\n=== 蓝牙设备管理 ===" << std::endl;
        std::cout << "1. 扫描蓝牙设备" << std::endl;
        std::cout << "2. 配对设备" << std::endl;
        std::cout << "3. 取消配对设备" << std::endl;
        std::cout << "4. 连接设备" << std::endl;
        std::cout << "5. 断开设备连接" << std::endl;
        std::cout << "6. 设置设备名称" << std::endl;
        std::cout << "7. 设置自动连接" << std::endl;
        std::cout << "8. 管理已配对设备" << std::endl;
        std::cout << "9. 自动连接已配对设备" << std::endl;
        std::cout << "0. 返回主菜单" << std::endl;
        std::cout << "请选择操作: ";

        std::getline(std::cin, input);

        try
        {
            choice = std::stoi(input);
        }
        catch (...)
        {
            std::cout << "无效输入，请重新选择" << std::endl;
            continue;
        }

        switch (choice)
        {
        case 1:
            std::cout << "正在扫描蓝牙设备..." << std::endl;
            if (blue.startScanning(10))
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                devices = blue.getScanResults();
                std::cout << "扫描完成" << std::endl;
                displayBluetoothDevices(devices);
            }
            else
            {
                std::cout << "扫描失败，请检查蓝牙是否已开启" << std::endl;
            }
            break;

        case 2:
            if (devices.empty())
            {
                std::cout << "请先扫描设备" << std::endl;
                break;
            }

            std::cout << "请选择要配对的设备序号 (0取消): ";
            std::getline(std::cin, input);

            try
            {
                int deviceChoice = std::stoi(input);
                if (deviceChoice == 0)
                    break;

                if (deviceChoice > 0 && deviceChoice <= static_cast<int>(devices.size()))
                {
                    const auto &selectedDevice = devices[deviceChoice - 1];
                    std::cout << "正在配对设备: " << selectedDevice.name << std::endl;
                    if (blue.pairDevice(selectedDevice))
                    {
                        std::cout << "配对成功!" << std::endl;
                    }
                    else
                    {
                        std::cout << "配对失败" << std::endl;
                    }
                }
                else
                {
                    std::cout << "无效的设备序号" << std::endl;
                }
            }
            catch (...)
            {
                std::cout << "无效输入" << std::endl;
            }
            break;

        case 3:
            if (devices.empty())
            {
                std::cout << "请先扫描设备" << std::endl;
                break;
            }

            std::cout << "请选择要取消配对的设备序号 (0取消): ";
            std::getline(std::cin, input);

            try
            {
                int deviceChoice = std::stoi(input);
                if (deviceChoice == 0)
                    break;

                if (deviceChoice > 0 && deviceChoice <= static_cast<int>(devices.size()))
                {
                    const auto &selectedDevice = devices[deviceChoice - 1];
                    std::cout << "正在取消配对设备: " << selectedDevice.name << std::endl;
                    if (blue.unpairDevice(selectedDevice))
                    {
                        std::cout << "取消配对成功!" << std::endl;
                    }
                    else
                    {
                        std::cout << "取消配对失败" << std::endl;
                    }
                }
                else
                {
                    std::cout << "无效的设备序号" << std::endl;
                }
            }
            catch (...)
            {
                std::cout << "无效输入" << std::endl;
            }
            break;

        case 4:
            if (devices.empty())
            {
                std::cout << "请先扫描设备" << std::endl;
                break;
            }

            std::cout << "请选择要连接的设备序号 (0取消): ";
            std::getline(std::cin, input);

            try
            {
                int deviceChoice = std::stoi(input);
                if (deviceChoice == 0)
                    break;

                if (deviceChoice > 0 && deviceChoice <= static_cast<int>(devices.size()))
                {
                    const auto &selectedDevice = devices[deviceChoice - 1];
                    std::cout << "正在连接设备: " << selectedDevice.name << std::endl;
                    if (blue.connectToDevice(selectedDevice))
                    {
                        std::cout << "连接成功!" << std::endl;
                    }
                    else
                    {
                        std::cout << "连接失败" << std::endl;
                    }
                }
                else
                {
                    std::cout << "无效的设备序号" << std::endl;
                }
            }
            catch (...)
            {
                std::cout << "无效输入" << std::endl;
            }
            break;

        case 5:
            if (devices.empty())
            {
                std::cout << "请先扫描设备" << std::endl;
                break;
            }

            std::cout << "请选择要断开连接的设备序号 (0取消): ";
            std::getline(std::cin, input);

            try
            {
                int deviceChoice = std::stoi(input);
                if (deviceChoice == 0)
                    break;

                if (deviceChoice > 0 && deviceChoice <= static_cast<int>(devices.size()))
                {
                    const auto &selectedDevice = devices[deviceChoice - 1];
                    std::cout << "正在断开设备: " << selectedDevice.name << std::endl;
                    if (blue.disconnectDevice(selectedDevice))
                    {
                        std::cout << "断开成功!" << std::endl;
                    }
                    else
                    {
                        std::cout << "断开失败" << std::endl;
                    }
                }
                else
                {
                    std::cout << "无效的设备序号" << std::endl;
                }
            }
            catch (...)
            {
                std::cout << "无效输入" << std::endl;
            }
            break;

        case 6:
            if (devices.empty())
            {
                std::cout << "请先扫描设备" << std::endl;
                break;
            }

            std::cout << "请选择要设置的设备名称序号 (0取消): ";
            std::getline(std::cin, input);

            try
            {
                int deviceChoice = std::stoi(input);
                if (deviceChoice == 0)
                    break;

                if (deviceChoice > 0 && deviceChoice <= static_cast<int>(devices.size()))
                {
                    const auto &selectedDevice = devices[deviceChoice - 1];
                    std::string deviceName;
                    std::cout << "当前设备名称: " << selectedDevice.name << std::endl;
                    std::cout << "请输入新的设备名称: ";
                    std::getline(std::cin, deviceName);

                    if (!deviceName.empty())
                    {
                        if (blue.setDeviceName(selectedDevice.address, deviceName))
                        {
                            std::cout << "新名称设置成功!" << std::endl;
                        }
                        else
                        {
                            std::cout << "设置失败" << std::endl;
                        }
                    }
                }
                else
                {
                    std::cout << "无效的设备序号" << std::endl;
                }
            }
            catch (...)
            {
                std::cout << "无效输入" << std::endl;
            }
            break;

        case 7:
            if (devices.empty())
            {
                std::cout << "请先扫描设备" << std::endl;
                break;
            }

            std::cout << "请选择要设置自动连接的设备序号 (0取消): ";
            std::getline(std::cin, input);

            try
            {
                int deviceChoice = std::stoi(input);
                if (deviceChoice == 0)
                    break;

                if (deviceChoice > 0 && deviceChoice <= static_cast<int>(devices.size()))
                {
                    const auto &selectedDevice = devices[deviceChoice - 1];
                    std::cout << "当前设备: " << selectedDevice.name << " (" << selectedDevice.address << ")" << std::endl;
                    std::cout << "当前自动连接状态: " << (selectedDevice.autoConnect ? "启用" : "禁用") << std::endl;
                    std::cout << "是否启用自动连接? (y/n): ";
                    std::getline(std::cin, input);

                    bool autoConnect = (input == "y" || input == "Y" || input == "是");
                    if (blue.setAutoConnect(selectedDevice.address, autoConnect))
                    {
                        std::cout << "自动连接设置" << (autoConnect ? "启用" : "禁用") << "成功!" << std::endl;
                    }
                    else
                    {
                        std::cout << "设置失败" << std::endl;
                    }
                }
                else
                {
                    std::cout << "无效的设备序号" << std::endl;
                }
            }
            catch (...)
            {
                std::cout << "无效输入" << std::endl;
            }
            break;

        case 8:
            managePairedDevices(blue);
            break;

        case 9:
            std::cout << "正在自动连接已配对的设备..." << std::endl;
            if (blue.autoConnectToPairedDevices())
            {
                std::cout << "自动连接完成!" << std::endl;
            }
            else
            {
                std::cout << "自动连接失败" << std::endl;
            }
            break;

        case 0:
            return;

        default:
            std::cout << "无效选择，请重新输入" << std::endl;
            break;
        }
    }
}

void bluetoothAdapterSettings(BlueInterface &blue)
{
    std::string input;
    int choice;

    while (true)
    {
        std::cout << "\n=== 蓝牙适配器设置 ===" << std::endl;
        std::cout << "当前适配器名称: " << blue.getAdapterName() << std::endl;
        std::cout << "1. 开启蓝牙" << std::endl;
        std::cout << "2. 关闭蓝牙" << std::endl;
        std::cout << "3. 设置适配器名称" << std::endl;
        std::cout << "4. 重置适配器名称" << std::endl;
        std::cout << "0. 返回主菜单" << std::endl;
        std::cout << "请选择操作: ";

        std::getline(std::cin, input);

        try
        {
            choice = std::stoi(input);
        }
        catch (...)
        {
            std::cout << "无效输入，请重新选择" << std::endl;
            continue;
        }

        switch (choice)
        {
        case 1:
            if (blue.enableBluetooth())
            {
                std::cout << "蓝牙开启成功!" << std::endl;
            }
            else
            {
                std::cout << "蓝牙开启失败" << std::endl;
            }
            break;

        case 2:
            if (blue.disableBluetooth())
            {
                std::cout << "蓝牙关闭成功!" << std::endl;
            }
            else
            {
                std::cout << "蓝牙关闭失败" << std::endl;
            }
            break;

        case 3:
        {
            std::string adapterName;
            std::cout << "请输入蓝牙适配器名称: ";
            std::getline(std::cin, adapterName);

            if (!adapterName.empty())
            {
                if (blue.setAdapterName(adapterName))
                {
                    std::cout << "适配器名称设置成功!" << std::endl;
                    std::cout << "当前适配器名称: " << blue.getAdapterName() << std::endl;
                }
                else
                {
                    std::cout << "设置失败" << std::endl;
                }
            }
            else
            {
                std::cout << "适配器名称不能为空" << std::endl;
            }
            break;
        }

        case 4:
            if (blue.resetAdapterName())
            {
                std::cout << "适配器名称重置成功!" << std::endl;
                std::cout << "当前适配器名称: " << blue.getAdapterName() << std::endl;
            }
            else
            {
                std::cout << "重置失败" << std::endl;
            }
            break;

        case 0:
            return;

        default:
            std::cout << "无效选择，请重新输入" << std::endl;
            break;
        }
    }
}

int main()
{
    BlueInterface blue;
    std::string input;
    int choice;

    std::cout << "=== 蓝牙测试程序 ===" << std::endl;
    bool enabled = blue.isBluetoothEnabled();
    std::cout << "蓝牙状态: " << (enabled ? "已开启" : "已关闭") << std::endl;

    while (true)
    {
        std::cout << "\n=== 主菜单 ===" << std::endl;
        std::cout << "1. 蓝牙设备管理" << std::endl;
        std::cout << "2. 蓝牙适配器设置" << std::endl;
        std::cout << "3. 查看蓝牙状态" << std::endl;
        std::cout << "0. 退出程序" << std::endl;
        std::cout << "请选择操作: ";

        std::getline(std::cin, input);

        try
        {
            choice = std::stoi(input);
        }
        catch (...)
        {
            std::cout << "无效输入，请重新选择" << std::endl;
            continue;
        }

        switch (choice)
        {
        case 1:
            bluetoothDeviceManagement(blue);
            break;
        case 2:
            bluetoothAdapterSettings(blue);
            break;
        case 3:
            displayBluetoothStatus(blue);
            break;
        case 0:
            std::cout << "退出程序" << std::endl;
            return 0;
        default:
            std::cout << "无效输入，请重新选择" << std::endl;
            break;
        }
    }
}

#elif (defined(WIFI_TEST))
void displayNetworks(const std::vector<NetworkInfo> &networks)
{
    std::cout << "\n=== 可用网络列表 ===" << std::endl;
    std::cout << std::setw(3) << "序号"
              << std::setw(30) << "SSID"
              << std::setw(23) << "信号强度"
              << std::setw(11) << "信道"
              << std::setw(11) << "频率"
              << std::setw(16) << "BSSID" << std::endl;
    std::cout << std::string(95, '-') << std::endl;

    for (size_t i = 0; i < networks.size(); ++i)
    {
        const auto &network = networks[i];
        std::cout << std::setw(3) << i + 1
                  << std::setw(35) << (network.ssid.empty() ? "[隐藏网络]" : network.ssid)
                  << std::setw(10) << network.signalStrength << " dBm"
                  << std::setw(8) << network.channel
                  << std::setw(10) << network.frequency << "MHz"
                  << std::setw(20) << network.bssid.substr(0, 17) << std::endl;
    }
}

void displaySavedNetworks(WifiInterface &wifi)
{
    auto savedNetworks = wifi.getSavedNetworks();
    std::cout << "\n=== 已保存的网络列表 (仅显示当前范围内的网络) ===" << std::endl;

    if (savedNetworks.empty())
    {
        std::cout << "当前范围内没有可用的已保存网络" << std::endl;
        return;
    }

    std::cout << std::setw(3) << "序号"
              << std::setw(20) << "SSID"
              << std::setw(15) << "信号强度"
              << std::setw(15) << "自动连接" << std::endl;
    std::cout << std::string(60, '-') << std::endl;

    for (size_t i = 0; i < savedNetworks.size(); ++i)
    {
        const auto &network = savedNetworks[i];
        std::cout << std::setw(3) << i + 1
                  << std::setw(25) << network.ssid.substr(0, 24)
                  << std::setw(10) << network.signalStrength << " dBm"
                  << std::setw(10) << (network.autoConnect ? "是" : "否") << std::endl;
    }
}

void displayConnectionStatus(WifiInterface &wifi)
{
    auto status = wifi.getConnectionStatus();
    std::cout << "\n=== 连接状态 ===" << std::endl;

    NetworkInfo currentNetwork;

    switch (status)
    {
    case ConnectionStatus::DISCONNECTED:
        std::cout << "状态: 未连接" << std::endl;
        break;
    case ConnectionStatus::CONNECTING:
        std::cout << "状态: 连接中..." << std::endl;
        break;
    case ConnectionStatus::CONNECTED:
        std::cout << "状态: 已连接" << std::endl;
        std::cout << "wifi名称: " << wifi.getCurrentNetwork().ssid << std::endl;
        std::cout << "信号强度: " << wifi.getSignalStrength() << " dBm" << std::endl;

        currentNetwork = wifi.getCurrentNetwork();
        if (!currentNetwork.ssid.empty())
        {
            std::cout << "当前网络: " << currentNetwork.ssid << std::endl;
        }
        break;
    case ConnectionStatus::DISCONNECTING:
        std::cout << "状态: 断开中..." << std::endl;
        break;
    case ConnectionStatus::CONNECTION_FAILED:
        std::cout << "状态: 连接失败" << std::endl;
        break;
    default:
        std::cout << "状态: 未知状态" << std::endl;
        break;
    }

    std::string ipAddress = wifi.getIPAddress();
    std::string subnetMask = wifi.getSubnetMask();
    std::string gateway = wifi.getGateway();
    std::string macAddress = wifi.getMACAddress();

    std::cout << "IP地址: " << (ipAddress.empty() ? "未分配" : ipAddress) << std::endl;
    std::cout << "子网掩码: " << (subnetMask.empty() ? "未分配" : subnetMask) << std::endl;
    std::cout << "网关地址: " << (gateway.empty() ? "未分配" : gateway) << std::endl;
    std::cout << "MAC地址: " << (macAddress.empty() ? "未知" : macAddress) << std::endl;
}

void displayStaticIPConfig(WifiInterface &wifi)
{
    auto staticConfig = wifi.getStaticIPConfig();
    std::cout << "\n=== 静态IP配置 ===" << std::endl;

    if (staticConfig.ipAddress.empty())
    {
        std::cout << "当前使用DHCP自动获取IP地址" << std::endl;
    }
    else
    {
        std::cout << "IP地址: " << staticConfig.ipAddress << std::endl;
        std::cout << "子网掩码: " << staticConfig.subnetMask << std::endl;
        std::cout << "网关: " << staticConfig.gateway << std::endl;
    }
}

void setStaticIPConfiguration(WifiInterface &wifi)
{
    std::string input;
    StaticIPConfig staticConfig;

    std::cout << "\n=== 设置静态IP配置 ===" << std::endl;
    std::cout << "注意: 设置静态IP后，下次连接将使用静态IP，而不是DHCP" << std::endl;
    std::cout << "警告: 请勿设置与eth0相同的IP地址，否则可能导致网络连接问题" << std::endl;

    auto currentConfig = wifi.getStaticIPConfig();

    std::cout << "请输入IP地址 (" << (currentConfig.ipAddress.empty() ? "例如: 192.168.1.100" : currentConfig.ipAddress) << "): ";
    std::getline(std::cin, input);
    if (!input.empty())
    {
        staticConfig.ipAddress = input;
    }
    else if (!currentConfig.ipAddress.empty())
    {
        staticConfig.ipAddress = currentConfig.ipAddress;
    }

    std::cout << "请输入子网掩码 (" << currentConfig.subnetMask << "): ";
    std::getline(std::cin, input);
    if (!input.empty())
    {
        staticConfig.subnetMask = input;
    }
    else
    {
        staticConfig.subnetMask = currentConfig.subnetMask;
    }

    std::cout << "请输入网关地址 (" << (currentConfig.gateway.empty() ? "例如: 192.168.1.1" : currentConfig.gateway) << "): ";
    std::getline(std::cin, input);
    if (!input.empty())
    {
        staticConfig.gateway = input;
    }
    else if (!currentConfig.gateway.empty())
    {
        staticConfig.gateway = currentConfig.gateway;
    }

    if (wifi.setStaticIPConfig(staticConfig))
    {
        std::cout << "静态IP配置设置成功!" << std::endl;
        displayStaticIPConfig(wifi);
    }
    else
    {
        std::cout << "静态IP配置设置失败!" << std::endl;
    }
}

void manageSavedNetworks(WifiInterface &wifi)
{
    std::string input;
    int choice;
    while (true)
    {
        std::cout << "\n=== 已保存网络管理 ===" << std::endl;
        displaySavedNetworks(wifi);

        std::cout << "\n1. 设置自动连接" << std::endl;
        std::cout << "2. 删除已保存网络" << std::endl;
        std::cout << "0. 返回上级菜单" << std::endl;
        std::cout << "请选择操作: ";

        std::getline(std::cin, input);
        try
        {
            choice = std::stoi(input);
        }
        catch (...)
        {
            std::cout << "无效输入，请重新选择" << std::endl;
            continue;
        }
        switch (choice)
        {
        case 1:
        {
            auto savedNetworks = wifi.getSavedNetworks();
            if (savedNetworks.empty())
            {
                std::cout << "没有已保存的网络" << std::endl;
                break;
            }

            std::cout << "请选择要设置自动连接的网络序号 (0取消): ";
            std::getline(std::cin, input);

            try
            {
                int networkChoice = std::stoi(input);
                if (networkChoice == 0)
                    break;

                if (networkChoice > 0 && networkChoice <= static_cast<int>(savedNetworks.size()))
                {
                    const auto &selectedNetwork = savedNetworks[networkChoice - 1];
                    std::cout << "当前网络: " << selectedNetwork.ssid << std::endl;
                    std::cout << "当前自动连接设置: " << (selectedNetwork.autoConnect ? "启用" : "禁用") << std::endl;

                    std::cout << "是否启用自动连接? (y/n): ";
                    std::getline(std::cin, input);

                    bool enable = (input == "y" || input == "Y" || input == "是");
                    if (wifi.setAutoConnect(selectedNetwork.ssid, enable))
                    {
                        std::cout << "自动连接设置已更新" << std::endl;
                    }
                    else
                    {
                        std::cout << "设置失败" << std::endl;
                    }
                }
                else
                {
                    std::cout << "无效的网络序号" << std::endl;
                }
            }
            catch (...)
            {
                std::cout << "无效输入" << std::endl;
            }
            break;
        }

        case 2:
        {
            auto savedNetworks = wifi.getSavedNetworks();
            if (savedNetworks.empty())
            {
                std::cout << "没有已保存的网络" << std::endl;
                break;
            }

            std::cout << "请选择要删除的网络序号 (0取消): ";
            std::getline(std::cin, input);

            try
            {
                int networkChoice = std::stoi(input);
                if (networkChoice == 0)
                    break;

                if (networkChoice > 0 && networkChoice <= static_cast<int>(savedNetworks.size()))
                {
                    const auto &selectedNetwork = savedNetworks[networkChoice - 1];
                    std::cout << "确定要删除网络 '" << selectedNetwork.ssid << "' 吗? (y/n): ";
                    std::getline(std::cin, input);
                    if (input == "y" || input == "Y" || input == "是")
                    {
                        if (wifi.forgetNetwork(selectedNetwork.ssid))
                        {
                            std::cout << "网络已删除" << std::endl;
                        }
                        else
                        {
                            std::cout << "删除失败" << std::endl;
                        }
                    }
                }
                else
                {
                    std::cout << "无效的网络序号" << std::endl;
                }
            }
            catch (...)
            {
                std::cout << "无效输入" << std::endl;
            }
            break;
        }

        case 0:
            return;

        default:
            std::cout << "无效选择" << std::endl;
            break;
        }
    }
}

// ==================== STA模式功能菜单 ====================
void staModeMenu(WifiInterface &wifi)
{
    std::string input;
    int choice;
    // 如果当前未连接，尝试自动连接已保存网络
    if (wifi.getConnectionStatus() == ConnectionStatus::DISCONNECTED)
    {
        wifi.autoConnectToSavedNetworks();
    }
    while (true)
    {
        std::cout << "\n=== STA模式功能菜单 ===" << std::endl;
        std::cout << "1. 扫描可用网络" << std::endl;
        std::cout << "2. 连接WiFi网络" << std::endl;
        std::cout << "3. 断开当前连接" << std::endl;
        std::cout << "4. 显示连接状态" << std::endl;
        std::cout << "5. 自动连接已保存网络" << std::endl;
        std::cout << "6. 管理已保存网络" << std::endl;
        std::cout << "7. 静态IP配置管理" << std::endl;
        std::cout << "0. 返回主菜单" << std::endl;
        std::cout << "请选择操作: ";

        std::getline(std::cin, input);

        try
        {
            choice = std::stoi(input);
        }
        catch (...)
        {
            std::cout << "无效输入，请重新选择" << std::endl;
            continue;
        }

        switch (choice)
        {
        case 1:
            std::cout << "正在扫描网络..." << std::endl;
            if (wifi.scanNetworks())
            {
                auto networks = wifi.getScanResults();
                displayNetworks(networks);
            }
            else
            {
                std::cout << "扫描失败" << std::endl;
            }
            break;

        case 2:
        {
            auto networks = wifi.getScanResults();
            auto savedNetworks = wifi.getSavedNetworks();

            if (networks.empty() && savedNetworks.empty())
            {
                std::cout << "没有可用的网络，请先扫描网络或保存网络配置" << std::endl;
                break;
            }

            std::cout << "请选择连接方式:" << std::endl;
            std::cout << "1. 从扫描结果中选择网络连接" << std::endl;
            std::cout << "2. 从已保存网络中选择连接" << std::endl;
            std::cout << "0. 取消" << std::endl;
            std::cout << "请选择: ";

            std::getline(std::cin, input);
            if (input == "0")
                break;

            if (input == "1")
            {
                if (networks.empty())
                {
                    std::cout << "请先扫描网络" << std::endl;
                    break;
                }

                displayNetworks(networks);
                std::cout << "\n请选择要连接的网络序号 (0取消): ";
                std::getline(std::cin, input);

                try
                {
                    int networkChoice = std::stoi(input);
                    if (networkChoice == 0)
                        break;

                    if (networkChoice > 0 && networkChoice <= static_cast<int>(networks.size()))
                    {
                        const auto &selectedNetwork = networks[networkChoice - 1];
                        std::string password;

                        if (selectedNetwork.security != SecurityMode::OPEN)
                        {
                            std::cout << "请输入密码: ";
                            std::getline(std::cin, password);
                        }

                        std::cout << "正在连接到: " << selectedNetwork.ssid << std::endl;
                        if (wifi.connectToNetwork(selectedNetwork.ssid, password))
                        {
                            std::cout << "连接成功!" << std::endl;
                            std::this_thread::sleep_for(std::chrono::seconds(2));
                            displayConnectionStatus(wifi);
                        }
                        else
                        {
                            std::cout << "连接失败" << std::endl;
                        }
                    }
                    else
                    {
                        std::cout << "无效的网络序号" << std::endl;
                    }
                }
                catch (...)
                {
                    std::cout << "无效输入" << std::endl;
                }
            }
            else if (input == "2")
            {
                auto savedNetworks = wifi.getSavedNetworks();
                if (savedNetworks.empty())
                {
                    std::cout << "没有已保存的网络" << std::endl;
                    break;
                }

                displaySavedNetworks(wifi);
                std::cout << "\n请选择要连接的网络序号 (0取消): ";
                std::getline(std::cin, input);

                try
                {
                    int networkChoice = std::stoi(input);
                    if (networkChoice == 0)
                        break;

                    if (networkChoice > 0 && networkChoice <= static_cast<int>(savedNetworks.size()))
                    {
                        const auto &selectedNetwork = savedNetworks[networkChoice - 1];
                        std::string password;

                        std::cout << "正在连接到: " << selectedNetwork.ssid << std::endl;
                        if (wifi.connectToNetwork(selectedNetwork.ssid, ""))
                        {
                            std::cout << "连接成功!" << std::endl;
                            std::this_thread::sleep_for(std::chrono::seconds(2));
                            displayConnectionStatus(wifi);
                        }
                        else
                        {
                            std::cout << "连接失败，可能是密码已更改，请重新输入密码" << std::endl;
                            if (selectedNetwork.security != SecurityMode::OPEN)
                            {
                                std::cout << "请输入新密码: ";
                                std::getline(std::cin, password);
                            }

                            std::cout << "正在重新连接到: " << selectedNetwork.ssid << std::endl;
                            if (wifi.connectToNetwork(selectedNetwork.ssid, password))
                            {
                                std::cout << "连接成功!" << std::endl;
                                std::this_thread::sleep_for(std::chrono::seconds(2));
                                displayConnectionStatus(wifi);
                            }
                            else
                            {
                                std::cout << "连接失败，请检查网络设置" << std::endl;
                            }
                        }
                    }
                    else
                    {
                        std::cout << "无效的网络序号" << std::endl;
                    }
                }
                catch (...)
                {
                    std::cout << "无效输入" << std::endl;
                }
            }
            else
            {
                std::cout << "无效选择，请重新输入" << std::endl;
            }
            break;
        }

        case 3:
            if (wifi.disconnect())
            {
                std::cout << "已断开连接" << std::endl;
            }
            else
            {
                std::cout << "断开连接失败或未连接" << std::endl;
            }
            break;

        case 4:
            displayConnectionStatus(wifi);
            break;

        case 5:
            if (wifi.autoConnectToSavedNetworks())
            {
                std::cout << "自动连接成功" << std::endl;
            }
            else
            {
                std::cout << "自动连接失败" << std::endl;
            }
            break;

        case 6:
            manageSavedNetworks(wifi);
            break;

        case 7:
        {
            std::string ipChoice;
            while (true)
            {
                std::cout << "\n=== 静态IP配置管理 ===" << std::endl;
                displayStaticIPConfig(wifi);

                std::cout << "1. 设置静态IP配置" << std::endl;
                std::cout << "2. 清除静态IP配置(使用DHCP)" << std::endl;
                std::cout << "3. 查看当前IP配置" << std::endl;
                std::cout << "0. 返回上级菜单" << std::endl;
                std::cout << "请选择操作: ";

                std::getline(std::cin, ipChoice);

                if (ipChoice == "0")
                    break;

                try
                {
                    int ipMenuChoice = std::stoi(ipChoice);
                    switch (ipMenuChoice)
                    {
                    case 1:
                        setStaticIPConfiguration(wifi);
                        break;
                    case 2:
                        if (wifi.clearStaticIPConfig())
                        {
                            std::cout << "静态IP配置已清除" << std::endl;
                        }
                        break;
                    case 3:
                        displayStaticIPConfig(wifi);
                        break;
                    default:
                        std::cout << "无效选择" << std::endl;
                        break;
                    }
                }
                catch (...)
                {
                    std::cout << "无效输入" << std::endl;
                }
            }
            break;
        }

        case 0:
            return;

        default:
            std::cout << "无效选择，请重新输入" << std::endl;
            break;
        }
    }
}

// ==================== AP模式功能菜单 ====================
void apModeMenu(WifiInterface &wifi)
{
    std::string input;
    int choice;

    while (true)
    {
        std::cout << "\n=== AP模式功能菜单 ===" << std::endl;
        std::cout << "1. 启动AP热点" << std::endl;
        std::cout << "2. 停止AP热点" << std::endl;
        std::cout << "3. 配置AP参数" << std::endl;
        std::cout << "4. 查看AP状态" << std::endl;
        std::cout << "5. 查看已连接客户端" << std::endl;
        std::cout << "6. 断开指定客户端" << std::endl;
        std::cout << "0. 返回主菜单" << std::endl;
        std::cout << "请选择操作: ";

        std::getline(std::cin, input);

        try
        {
            choice = std::stoi(input);
        }
        catch (...)
        {
            std::cout << "无效输入，请重新选择" << std::endl;
            continue;
        }

        switch (choice)
        {
        case 1:
            if (wifi.startAP())
            {
                std::cout << "AP热点启动成功" << std::endl;
            }
            else
            {
                std::cout << "AP热点启动失败" << std::endl;
            }
            break;
        case 2:
            if (wifi.stopAP())
            {
                std::cout << "AP热点已停止" << std::endl;
            }
            else
            {
                std::cout << "AP热点停止失败或未运行" << std::endl;
            }
            break;
        case 3:
        {
            APConfig config = wifi.getAPConfig();
            std::cout << "提示：在配置过程中，输入0可以随时退出配置" << std::endl;
            std::cout << "请输入新的SSID (" << config.ssid << "): ";
            std::getline(std::cin, input);
            if (input == "0")
                break;
            if (!input.empty())
                config.ssid = input;

            std::cout << "请输入新的密码 (留空保持原密码): ";
            std::getline(std::cin, input);
            if (input == "0")
                break;
            if (!input.empty())
                config.password = input;

            std::cout << "请输入信道 (" << config.channel << "): ";
            std::getline(std::cin, input);
            if (input == "0")
                break;
            if (!input.empty())
                config.channel = std::stoi(input);

            std::cout << "请输入最大客户端数 (" << config.maxClients << "): ";
            std::getline(std::cin, input);
            if (input == "0")
                break;
            if (!input.empty())
                config.maxClients = std::stoi(input);

            if (wifi.setAPConfig(config))
            {
                std::cout << "AP配置更新成功" << std::endl;
            }
            else
            {
                std::cout << "AP配置更新失败" << std::endl;
            }
            break;
        }
        case 4:
        {
            APConfig config = wifi.getAPConfig();
            std::cout << "\n=== AP配置信息 ===" << std::endl;
            std::cout << "SSID: " << config.ssid << std::endl;
            std::cout << "密码: " << (config.password.empty() ? "无密码" : "已设置") << std::endl;
            std::cout << "信道: " << config.channel << std::endl;
            std::cout << "最大客户端数: " << config.maxClients << std::endl;
            std::cout << "安全模式: ";
            switch (config.security)
            {
            case SecurityMode::OPEN:
                std::cout << "开放网络" << std::endl;
                break;
            case SecurityMode::WEP:
                std::cout << "WEP" << std::endl;
                break;
            case SecurityMode::WPA_PSK:
                std::cout << "WPA-PSK" << std::endl;
                break;
            case SecurityMode::WPA2_PSK:
                std::cout << "WPA2-PSK" << std::endl;
                break;
            case SecurityMode::WPA_WPA2_PSK:
                std::cout << "WPA/WPA2-PSK" << std::endl;
                break;
            default:
                std::cout << "未知" << std::endl;
                break;
            }
            std::cout << "AP状态: " << (wifi.isAPRunning() ? "运行中" : "已停止") << std::endl;
            if (wifi.isAPRunning())
            {
                std::cout << "AP IP地址: " << wifi.getAPIPAddress() << std::endl;
                std::cout << "已连接客户端数: " << wifi.getClientCount() << std::endl;
            }
            break;
        }
        case 5:
        {
            auto clients = wifi.getConnectedClients();
            if (clients.empty())
            {
                std::cout << "没有客户端连接" << std::endl;
            }
            else
            {
                std::cout << "已连接客户端列表:" << std::endl;
                for (size_t i = 0; i < clients.size(); ++i)
                {
                    std::cout << i + 1 << ". MAC: " << clients[i].macAddress
                              << ", IP: " << clients[i].ipAddress << std::endl;
                }
            }
            break;
        }
        case 6:
        {
            auto clients = wifi.getConnectedClients();
            if (clients.empty())
            {
                std::cout << "没有客户端连接" << std::endl;
                break;
            }

            std::cout << "请选择要断开的客户端序号: ";
            std::getline(std::cin, input);
            try
            {
                int clientIndex = std::stoi(input) - 1;
                if (clientIndex >= 0 && clientIndex < static_cast<int>(clients.size()))
                {
                    if (wifi.disconnectClient(clients[clientIndex].macAddress))
                    {
                        std::cout << "客户端已断开" << std::endl;
                    }
                    else
                    {
                        std::cout << "断开客户端失败" << std::endl;
                    }
                }
                else
                {
                    std::cout << "无效的客户端序号" << std::endl;
                }
            }
            catch (...)
            {
                std::cout << "无效输入" << std::endl;
            }
            break;
        }
        case 0:
            return;
        default:
            std::cout << "无效选择，请重新输入" << std::endl;
            break;
        }
    }
}

// ==================== AP+STA共存模式功能菜单 ====================
void apStaModeMenu(WifiInterface &wifi)
{
    std::string input;
    int choice;

    while (true)
    {
        std::cout << "\n=== AP+STA共存模式功能菜单 ===" << std::endl;
        std::cout << "1. 启动AP+STA模式" << std::endl;
        std::cout << "2. 停止AP+STA模式" << std::endl;
        std::cout << "3. 查看STA连接状态" << std::endl;
        std::cout << "4. 查看AP状态" << std::endl;
        std::cout << "5. STA功能菜单" << std::endl;
        std::cout << "6. AP功能菜单" << std::endl;
        std::cout << "0. 返回主菜单" << std::endl;
        std::cout << "请选择操作: ";
        std::getline(std::cin, input);

        try
        {
            choice = std::stoi(input);
        }
        catch (...)
        {
            std::cout << "无效输入，请重新选择" << std::endl;
            continue;
        }

        switch (choice)
        {
        case 1:
            if (wifi.setOperationMode(WifiMode::WIFI_MODE_AP_STA))
            {
                std::cout << "AP+STA模式启动成功" << std::endl;
            }
            else
            {
                std::cout << "AP+STA模式启动失败" << std::endl;
            }
            break;

        case 2:
            if (wifi.setOperationMode(WifiMode::WIFI_MODE_ALL_OFF))
            {
                std::cout << "AP+STA模式已停止,所有WiFi服务已关闭" << std::endl;
            }
            else
            {
                std::cout << "AP+STA模式停止失败" << std::endl;
            }
            break;

        case 3:
            displayConnectionStatus(wifi);
            break;

        case 4:
            std::cout << "AP状态: " << (wifi.isAPRunning() ? "运行中" : "已停止") << std::endl;
            if (wifi.isAPRunning())
            {
                std::cout << "AP IP地址: " << wifi.getAPIPAddress() << std::endl;
                std::cout << "已连接客户端数: " << wifi.getClientCount() << std::endl;
            }
            break;

        case 5:
            staModeMenu(wifi);
            break;

        case 6:
            apModeMenu(wifi);
            break;

        case 0:
            return;

        default:
            std::cout << "无效选择，请重新输入" << std::endl;
            break;
        }
    }
}

// ==================== 主菜单 ====================
int main()
{
#ifdef _WIN32
    WifiInterface wifi("Wi-Fi", "Microsoft Wi-Fi Direct Virtual Adapter");
#else
    WifiInterface wifi("wlan0", "wlan1");
#endif // _WIN32

    std::string input;
    int choice;

    std::cout << " === WiFi接口测试程序 === " << std::endl;
#ifdef _WIN32
    std::cout << "STA接口: Wi-Fi" << std::endl;
    std::cout << "AP接口: Microsoft Wi-Fi Direct Virtual Adapter" << std::endl;
#else
    std::cout << "STA接口: wlan0" << std::endl;
    std::cout << "AP接口: wlan1" << std::endl;
#endif // _WIN32
    WifiMode currentMode = wifi.getCurrentMode();
    std::cout << "当前开启的工作模式为:";
    switch (currentMode)
    {
    case WifiMode::WIFI_MODE_STA:
        std::cout << "STA模式" << std::endl;
        break;
    case WifiMode::WIFI_MODE_AP:
        std::cout << "AP模式" << std::endl;
        break;
    case WifiMode::WIFI_MODE_AP_STA:
        std::cout << "AP+STA共存模式" << std::endl;
        break;
    case WifiMode::WIFI_MODE_ALL_OFF:
        std::cout << "全关闭模式" << std::endl;
        break;
    default:
        std::cout << "未知模式" << std::endl;
        break;
    }

    std::cout << "WiFi接口初始化成功" << std::endl;
    std::cout << "MAC地址: " << wifi.getMACAddress() << std::endl;

    while (true)
    {
        std::cout << "\n=== 主菜单 ===" << std::endl;
        std::cout << "1. STA模式" << std::endl;
        std::cout << "2. AP模式" << std::endl;
        std::cout << "3. AP+STA共存模式" << std::endl;
        std::cout << "4. 全关闭模式" << std::endl;
        std::cout << "5. 切换工作模式" << std::endl;
        std::cout << "0. 退出程序" << std::endl;
        std::cout << "请选择操作模式: ";

        std::getline(std::cin, input);

        try
        {
            choice = std::stoi(input);
        }
        catch (...)
        {
            std::cout << "无效输入，请重新选择" << std::endl;
            continue;
        }

        switch (choice)
        {
        case 1:
            if (currentMode == WifiMode::WIFI_MODE_ALL_OFF)
            {
                std::cout << "当前处于全关闭模式，无法使用STA功能" << std::endl;
                std::cout << "请先选择\"5.切换工作模式\"切换到STA模式" << std::endl;
            }
            else
            {
                staModeMenu(wifi);
            }
            break;

        case 2:
            if (currentMode == WifiMode::WIFI_MODE_ALL_OFF)
            {
                std::cout << "当前处于全关闭模式，无法使用AP功能" << std::endl;
                std::cout << "请先选择\"5.切换工作模式\"切换到AP模式" << std::endl;
            }
            else
            {
                apModeMenu(wifi);
            }
            break;

        case 3:
            if (currentMode == WifiMode::WIFI_MODE_ALL_OFF)
            {
                std::cout << "当前处于全关闭模式，无法使用AP+STA功能" << std::endl;
                std::cout << "请先选择\"5.切换工作模式\"切换到AP+STA模式" << std::endl;
            }
            else
            {
                apStaModeMenu(wifi);
            }
            break;

        case 4:
            std::cout << "正在关闭所有WiFi服务..." << std::endl;
            if (wifi.setOperationMode(WifiMode::WIFI_MODE_ALL_OFF))
            {
                currentMode = WifiMode::WIFI_MODE_ALL_OFF;
                std::cout << "所有WiFi服务已关闭" << std::endl;
            }
            else
            {
                std::cout << "关闭所有WiFi服务失败" << std::endl;
            }
            break;

        case 5:
        {
            std::cout << "当前工作模式: ";
            switch (wifi.getCurrentMode())
            {
            case WifiMode::WIFI_MODE_STA:
                std::cout << "STA模式" << std::endl;
                break;
            case WifiMode::WIFI_MODE_AP:
                std::cout << "AP模式" << std::endl;
                break;
            case WifiMode::WIFI_MODE_AP_STA:
                std::cout << "AP+STA共存模式" << std::endl;
                break;
            case WifiMode::WIFI_MODE_ALL_OFF:
                std::cout << "全关闭模式" << std::endl;
                break;
            default:
                std::cout << "未知模式" << std::endl;
                break;
            }

            std::cout << "请选择新的工作模式:" << std::endl;
            std::cout << "1. STA模式" << std::endl;
            std::cout << "2. AP模式" << std::endl;
            std::cout << "3. AP+STA共存模式" << std::endl;
            std::cout << "4. 全关闭模式" << std::endl;
            std::cout << "0. 取消" << std::endl;
            std::cout << "请选择: ";

            std::getline(std::cin, input);
            if (input == "0")
                break;

            WifiMode newMode;
            switch (std::stoi(input))
            {
            case 1:
                newMode = WifiMode::WIFI_MODE_STA;
                break;
            case 2:
                newMode = WifiMode::WIFI_MODE_AP;
                break;
            case 3:
                newMode = WifiMode::WIFI_MODE_AP_STA;
                break;
            case 4:
                newMode = WifiMode::WIFI_MODE_ALL_OFF;
                break;
            default:
                std::cout << "无效选择" << std::endl;
                continue;
            }

            if (wifi.setOperationMode(newMode))
            {
                currentMode = wifi.getCurrentMode();

                std::cout << "工作模式切换成功" << std::endl;
            }
            else
            {
                std::cout << "工作模式切换失败" << std::endl;
            }
            break;
        }

        case 0:
            std::cout << "退出测试程序" << std::endl;
            return 0;

        default:
            std::cout << "无效选择，请重新输入" << std::endl;
            break;
        }
    }

    return 0;
}
#endif