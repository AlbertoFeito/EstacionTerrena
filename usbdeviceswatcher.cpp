/**
* The MIT License (MIT)
*
* Copyright (c) 2021 Assam Boudjelthia
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION
*/
#include "usbdeviceswatcher.h"
#include "usbdeviceswatcher_p.h"

#include <QLoggingCategory>

#if defined(Q_OS_WIN)
#include <winuser.h>
#include <Dbt.h>
static constexpr const wchar_t HwndClassName[] = L"UsbDevicesWatcherPrivate";
#elif defined(Q_OS_MACOS)
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <CoreFoundation/CoreFoundation.h>
#else
#include <locale.h>
#include <unistd.h>
#endif

UsbDevicesWatcherPrivate::UsbDevicesWatcherPrivate(QObject *parent)
    : QObject(parent)
{
}

UsbDevicesWatcherPrivate *UsbDevicesWatcherPrivate::instance()
{
    static UsbDevicesWatcherPrivate instance;
    return &instance;
}

#if defined(Q_OS_WIN)
bool UsbDevicesWatcherPrivate::initDeviceMonitor()
{
    WNDCLASSEX wx;
    ZeroMemory(&wx, sizeof(wx));
    wx.cbSize = sizeof(WNDCLASSEX);
    wx.lpfnWndProc = reinterpret_cast<WNDPROC>(messageHandler);
    wx.lpszClassName = HwndClassName;

    if (RegisterClassEx(&wx)) {
        const wchar_t msgWindowName[] = L"UsbDevicesWatcherMessageWindow";
        m_hwnd = CreateWindow(wx.lpszClassName, msgWindowName, WS_ICONIC,
                              0, 0, CW_USEDEFAULT, 0, HWND_MESSAGE,
                              nullptr, GetModuleHandle(0), this);
        if (!m_hwnd || !m_deviceNotification) {
            qWarning() << "Failed to create the Message Window";
            return false;
        }
    } else {
        qWarning() << "Failed to register class with RegisterClassEx()";
        return false;
    }
    return true;
}

void UsbDevicesWatcherPrivate::releaseDeviceMonitor()
{
    if (m_deviceNotification) {
        UnregisterDeviceNotification(m_deviceNotification);
        m_deviceNotification = nullptr;
    }

    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        CloseHandle(m_hwnd);
        m_hwnd = nullptr;
    }

    UnregisterClass(HwndClassName, nullptr);
}

LRESULT UsbDevicesWatcherPrivate::messageHandler(HWND__* hwnd, UINT message,
                                                 WPARAM wparam, LPARAM lparam)
{
    static UsbDevicesWatcherPrivate *instance = nullptr;

    // NOTE: https://docs.microsoft.com/en-us/windows/win32/winmsg/window-notifications
    switch (message) {
    case WM_NCCREATE:
        return 1L;
    case WM_CREATE:
        if (!instance)
            instance = (UsbDevicesWatcherPrivate*)
                    ((CREATESTRUCT*) (lparam))->lpCreateParams;
        if (instance && !instance->m_isCancelled) {
            DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
            ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
            NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
            NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
            // USB guid
            NotificationFilter.dbcc_classguid = {0xA5DCBF10, 0x6530, 0x11D2,
                            {0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED}};
            instance->m_deviceNotification = RegisterDeviceNotification(
                        hwnd, &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
            return 0L;
        }
        return 1L;
    case WM_DEVICECHANGE:
        if (instance && !instance->m_isCancelled) {
            const PDEV_BROADCAST_HDR lpdb = (PDEV_BROADCAST_HDR) lparam;
            const PDEV_BROADCAST_DEVICEINTERFACE lpdbv =
                    (PDEV_BROADCAST_DEVICEINTERFACE) lpdb;
            if (lpdb->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
                const QString path = QString::fromStdWString(lpdbv->dbcc_name);
                // NOTE: Let's send only the serial number here, but other
                // info can be retrieved from the path is needed.
                const QStringList pathBits = path.split('#');
                QString serialNumber;
                // Sample path: \\\\?\\USB#VID_XXXX&PID_XXXX#SerialNumber#{UUID}
                if (pathBits.size() > 3)
                    serialNumber = pathBits.at(2);
                if (!serialNumber.isEmpty()) {
                    switch (wparam) {
                    case DBT_DEVICEARRIVAL:
                        emit instance->deviceConnected(serialNumber);
                        break;
                    case DBT_DEVICEREMOVECOMPLETE:
                        emit instance->deviceDisconnected(serialNumber);
                        break;
                    }
                }
            }
        }
        return 0L;
    }

    return 0L;
}
#elif defined(Q_OS_MACOS)
static QString getDeviceUid(io_service_t usbDevice)
{
    QString uid;
    const CFStringRef cfUid = static_cast<CFStringRef>(IORegistryEntryCreateCFProperty(
                                                           usbDevice,
                                                           CFSTR(kUSBSerialNumberString),
                                                           kCFAllocatorDefault, 0));
    if (cfUid) {
        uid = QString::fromCFString(cfUid);
        CFRelease(cfUid);
    }

    return uid;
}

void deviceConnectedCallback(void *refCon, io_iterator_t iterator)
{
    qWarning() << "here connected";
    io_service_t usbDevice;

    while ((usbDevice = IOIteratorNext(iterator))) {
        QString uid = getDeviceUid(usbDevice);
        if (uid.isEmpty())
            qWarning() << "Failed to retrieve connected device's UID";
        else
            emit UsbDevicesWatcherPrivate::instance()->deviceConnected(uid);
        const kern_return_t released = IOObjectRelease(usbDevice);
        if (released != KERN_SUCCESS)
            qWarning() << "Failed to release the connected  usb device after reading info";
    }
}

void deviceDisconnectedCallback(void *refCon, io_iterator_t iterator)
{
    qWarning() << "here disconnected";
    io_service_t usbDevice;

    while ((usbDevice = IOIteratorNext(iterator))) {
        QString uid = getDeviceUid(usbDevice);
        if (uid.isEmpty())
            qWarning() << "Failed to retrieve disconnected device's UID";
        else
            emit UsbDevicesWatcherPrivate::instance()->deviceDisconnected(uid);
        const kern_return_t released = IOObjectRelease(usbDevice);
        if (released != KERN_SUCCESS)
            qWarning() << "Failed to release the disconnected usb device after reading info";
    }
}

bool UsbDevicesWatcherPrivate::initDeviceMonitor()
{
    IONotificationPortRef notificationPort = IONotificationPortCreate(kIOMasterPortDefault);
    CFRunLoopSourceRef runLoopSource = IONotificationPortGetRunLoopSource(notificationPort);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopDefaultMode);

    const CFMutableDictionaryRef  matchingDictionary = IOServiceMatching(kIOUSBDeviceClassName);
    // IOServiceAddMatchingNotification releases this, so we retain for the next call
    CFRetain(matchingDictionary);

    // Now set up a notification to be called when a device is first matched by I/O Kit.
    io_iterator_t addedIter = 0;
    const kern_return_t addResult = IOServiceAddMatchingNotification(notificationPort,
                                                kIOMatchedNotification, matchingDictionary,
                                                deviceConnectedCallback, NULL, &addedIter);
    if (addResult != KERN_SUCCESS) {
        qWarning() << "Failed to add device connected callback.";
        return false;
    }

    io_iterator_t removedIter = 0;
    const kern_return_t removeResult = IOServiceAddMatchingNotification(notificationPort,
                                                    kIOTerminatedNotification, matchingDictionary,
                                                    deviceDisconnectedCallback, NULL, &removedIter);
    if (addResult != KERN_SUCCESS) {
        qWarning() << "Failed to add device disconnected callback.";
        return false;
    }

    // Iterate once to get already-present devices and arm the notification
    io_service_t usbDev;
    while ((usbDev = IOIteratorNext(addedIter))) { IOObjectRelease(usbDev); }
    while ((usbDev = IOIteratorNext(removedIter))) { IOObjectRelease(usbDev); }

    return true;
}

void UsbDevicesWatcherPrivate::releaseDeviceMonitor()
{
}
#else
bool UsbDevicesWatcherPrivate::initDeviceMonitor()
{
    dev_udev = udev_new();
    if (!dev_udev) {
        qWarning() << "udev_new() failed";
        return false;
    }

    dev_mon = udev_monitor_new_from_netlink(dev_udev, "udev");
    if (!dev_mon) {
        qWarning() << "udev_monitor_new_from_netlink(udev) failed";
        return false;
    }

    udev_monitor_filter_add_match_subsystem_devtype(dev_mon, "usb",  nullptr);
    udev_monitor_enable_receiving(dev_mon);
    return true;
}

void UsbDevicesWatcherPrivate::releaseDeviceMonitor()
{
    if (dev_mon) {
        udev_monitor_filter_remove(dev_mon);
        udev_monitor_unref(dev_mon);
        dev_mon = nullptr;
    }

    if (dev_udev) {
        udev_unref(dev_udev);
        dev_udev = nullptr;
    }
}

void UsbDevicesWatcherPrivate::run_from_thread()
{
    const int fd = udev_monitor_get_fd(dev_mon);
    while (!m_isCancelled) {
        fd_set fds;
        struct timeval tv;
        int ret;

        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 250 * 1000; // 250ms
        ret = select(fd + 1, &fds, nullptr, nullptr, &tv);

        if (ret > 0 && FD_ISSET(fd, &fds)) {
            struct udev_device *dev;
            dev = udev_monitor_receive_device(dev_mon);
            if (!dev)
                continue;

            QString path = "/"; // TODO( udev_get_dev_path(dev_udev));
            const char *sysName = udev_device_get_sysname(dev);
            if (!sysName)
                continue;

            path.append(sysName);
            const char *action = udev_device_get_action(dev);
            if (!action)
                continue;

            // get parent device to allow clients to get USB dev info.
            dev = udev_device_get_parent_with_subsystem_devtype(dev, "usb",
                                                                "usb_device");

            const QString actionStr = action;
            if (actionStr == "add")
                deviceConnected(path);
            else
                deviceDisconnected(path);

            udev_device_unref(dev);
        }
    }
}
#endif

/*
     Creates a USB device watcher object.

     To receive events of inserted or release USB devices,
     connect to \l {UsbDevicesWatcher::deviceConnected()},
     and \l {UsbDevicesWatcher::deviceDisconnected()} signals.
*/
UsbDevicesWatcher::UsbDevicesWatcher(QObject *parent)
    : QObject(parent),
      d_ptr(new UsbDevicesWatcherPrivate())
{
    // Debug only signals
    connect(this, &UsbDevicesWatcher::deviceConnected, this,
            [](const QString &path) {
        qDebug() << "Device connected:" << path;
    });
    connect(this, &UsbDevicesWatcher::deviceDisconnected, this,
            [](const QString &path) {
        qDebug() << "Device removed  :" << path;
    });

    connect(d_ptr.data(), &UsbDevicesWatcherPrivate::deviceConnected,
            this, &UsbDevicesWatcher::deviceConnected);
    connect(d_ptr.data(), &UsbDevicesWatcherPrivate::deviceDisconnected,
            this, &UsbDevicesWatcher::deviceDisconnected);
}

/*
     Does cleanup and releases the platform specific device monitors.
*/
UsbDevicesWatcher::~UsbDevicesWatcher()
{
    cancel();
}

/*
     Initializes the platform specific device monitors.
*/
bool UsbDevicesWatcher::init()
{
    cancel();
    d_ptr->m_isCancelled = false;
    const bool initialised = d_ptr->initDeviceMonitor();
    if (!initialised) {
        qWarning() << "Failed to initialize the USB device watcher";
        return initialised;
    }
    return initialised;
}

/*
     Explicitely releases the platform specific device monitors.
*/
void UsbDevicesWatcher::cancel()
{
    d_ptr->m_isCancelled = true;
    d_ptr->releaseDeviceMonitor();
}
