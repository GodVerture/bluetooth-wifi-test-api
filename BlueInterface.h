#ifndef BLUE_INTERFACE_H
#define BLUE_INTERFACE_H

#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <ctime>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <set>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif // _WIN32

struct BluetoothDevice
{
    std::string name;    // 蓝牙设备名称
    std::string address; // 蓝牙设备地址
    bool isPaired;       // 是否已配对
    bool isConnected;    // 是否已连接
    bool autoConnect;    // 是否自动连接
};

class BlueInterface
{
public:
    BlueInterface();
    virtual ~BlueInterface();

    /**
     * 开启蓝牙
     * @return 成功返回true，失败返回false
     */
    bool enableBluetooth();

    /**
     * 关闭蓝牙
     * @return 成功返回true，失败返回false
     */
    bool disableBluetooth();

    /**
     * 判断蓝牙是否已开启
     * @return 已开启返回true，未开启返回false
     */
    bool isBluetoothEnabled();

    /**
     * 开始扫描蓝牙设备
     * @param duration 扫描持续时间（秒），默认10秒
     * @return 成功返回true，失败返回false
     */
    bool startScanning(int duration = 10);

    /**
     * 停止扫描
     * @return 成功返回true，失败返回false
     */
    bool stopScanning();

    /**
     * 获取扫描结果
     * @return 扫描到的蓝牙设备列表
     */
    std::vector<BluetoothDevice> getScanResults();

    /**
     * 清空扫描结果
     */
    void clearScanResults();

    /**
     * 蓝牙配对
     * @param device 要配对的蓝牙设备
     * @return 成功返回true，失败返回false
     */
    bool pairDevice(const BluetoothDevice &device);

    /**
     * 取消配对设备
     * @param device 要取消配对的蓝牙设备
     * @return 成功返回true，失败返回false
     */
    bool unpairDevice(const BluetoothDevice &device);

    /**
     * 获取已配对的蓝牙设备列表
     * @return 已配对的蓝牙设备列表
     */
    std::vector<BluetoothDevice> getPairedDevices();

    /**
     * 连接蓝牙设备
     * @param device 要连接的蓝牙设备
     * @return 成功返回true，失败返回false
     */
    bool connectToDevice(const BluetoothDevice &device);

    /**
     * 断开蓝牙设备连接
     * @param device 要断开连接的蓝牙设备
     * @return 成功返回true，失败返回false
     */
    bool disconnectDevice(const BluetoothDevice &device);

    /**
     * 获取已连接的蓝牙设备列表
     * @return 已连接的蓝牙设备列表
     */
    std::vector<BluetoothDevice> getConnectedDevices();

    /**
     * 判断蓝牙设备是否已连接
     * @param deviceAddress 蓝牙设备地址
     * @return 已连接返回true，未连接返回false
     */
    bool isDeviceConnected(const std::string &deviceAddress);

    /**
     * 设置蓝牙设备自动连接
     * @param deviceAddress 蓝牙设备地址
     * @param autoConnect 是否自动连接
     * @return 成功返回true，失败返回false
     */
    bool setAutoConnect(const std::string &deviceAddress, bool autoConnect);

    /**
     * 自动连接已配对的蓝牙设备
     * @return 成功返回true，失败返回false
     */
    bool autoConnectToPairedDevices();

    /**
     * 设置蓝牙设备名称
     * @param deviceAddress 蓝牙设备地址
     * @param deviceName 蓝牙设备名称
     * @return 成功返回true，失败返回false
     */
    bool setDeviceName(const std::string &deviceAddress, const std::string &deviceName);

    /**
     * 获取蓝牙设备名称
     * @param deviceAddress 蓝牙设备地址
     * @return 蓝牙设备名称
     */
    std::string getDeviceName(const std::string &deviceAddress);

    /**
     * 设置蓝牙适配器本身的名称（本机蓝牙名称）
     * @param adapterName 适配器名称
     * @return 成功返回true，失败返回false
     */
    bool setAdapterName(const std::string &adapterName);

    /**
     * 重置蓝牙适配器名称为默认值
     * @return 成功返回true，失败返回false
     */
    bool resetAdapterName();

    /**
     * 查看蓝牙适配器名称
     * @return 蓝牙适配器名称
     */
    std::string getAdapterName();

    /**
     * 获取蓝牙设备自动连接状态
     * @param deviceAddress 蓝牙设备地址
     * @return 是否自动连接
     */
    bool getAutoConnectStatus(const std::string &deviceAddress);

    /**
     * 获取已保存的蓝牙设备列表
     * @return 已保存的蓝牙设备列表
     */
    std::vector<BluetoothDevice> getSavedDevices();

private:
    bool bluetoothEnabled_;                          // 蓝牙状态
    bool isScanning_;                                // 是否正在扫描
    std::vector<BluetoothDevice> scanResults_;       // 扫描结果
    std::string adapterName_;                        // 蓝牙适配器名称
    std::map<std::string, bool> autoConnectDevices_; // 自动连接设备映射表

    bool validateBluetoothState();
    bool validateDeviceAddress(const std::string &deviceAddress);
    bool validateDevice(const BluetoothDevice &device);

    std::string executeCommand(const std::string &command);
    bool executeCommandWithResult(const std::string &command);
    bool parseScanResults(const std::string &scanOutput);
    bool parseDeviceLine(const std::string &line, BluetoothDevice &device);
    void saveDeviceConfig();
    void loadDeviceConfig();
};

#endif // BLUE_INTERFACE_H