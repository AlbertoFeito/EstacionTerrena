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
#ifndef USBDEVICEWATCHER_H_
#define USBDEVICEWATCHER_H_

#include "usbdeviceswatcher_global.h"

#include <QString>
#include <QSharedPointer>
#include <QObject>

class UsbDevicesWatcherPrivate;

class USBDEVICESWATCHER_EXPORT UsbDevicesWatcher : public QObject
{
    Q_OBJECT
public:
    explicit UsbDevicesWatcher(QObject *parent = nullptr);
    ~UsbDevicesWatcher();

    bool init();
    void cancel();

Q_SIGNALS:
    void deviceConnected(const QString &serialNumber);
    void deviceDisconnected(const QString &serialNumber);

private:
    QSharedPointer<UsbDevicesWatcherPrivate> d_ptr;
};

#endif /* USBDEVICEWATCHER_H_ */
