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
#ifndef USBDEVICEWATCHER_P_H
#define USBDEVICEWATCHER_P_H

#include <QObject>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <guiddef.h>
#elif defined(Q_OS_LINUX)
#include <libudev.h>
#endif

// Private header notice

class UsbDevicesWatcherPrivate : public QObject
{
    Q_OBJECT
public:
    explicit UsbDevicesWatcherPrivate(QObject *parent = nullptr);
    ~UsbDevicesWatcherPrivate() = default;

    static UsbDevicesWatcherPrivate *instance();

    bool initDeviceMonitor();
    void releaseDeviceMonitor();

#if defined(Q_OS_WIN)
    static LRESULT messageHandler(HWND__* hwnd, UINT message,
                                  WPARAM wparam, LPARAM lparam);
    HWND m_hwnd = nullptr;
    HDEVNOTIFY m_deviceNotification = nullptr;
#elif defined(Q_OS_LINUX)
    udev *dev_udev = nullptr;
    udev_monitor *dev_mon = nullptr;
#endif
    bool m_isCancelled = false;

Q_SIGNALS:
    void deviceConnected(const QString &serialNumber);
    void deviceDisconnected(const QString &serialNumber);
};

#endif // USBDEVICEWATCHER_P_H
