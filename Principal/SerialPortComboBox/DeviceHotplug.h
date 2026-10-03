#pragma once
#include <QObject>
#include <QUuid>
#include <QSharedPointer>
#ifdef Q_OS_WIN
#include <Windows.h>
#else
// Fuera de Windows no hay notificaciones de dispositivos: se define el GUID
// de la clase de puertos COM para que el código cliente compile igual.
static const QUuid GUID_DEVINTERFACE_COMPORT(0x86E0D1E0, 0x8089, 0x11D0,
                                             0x9C, 0xE4, 0x08, 0x00, 0x3E, 0x30, 0x1F, 0x73);
#endif
class DeviceHotplugPrivate;

/**
 * @brief 设备插拔事件监听
 * @author 龚建波
 * @date 2022-12-24
 * @details
 * 设备类文档，可以在设备管理器找自己的设备 GUID
 * https://learn.microsoft.com/zh-cn/windows-hardware/drivers/install/overview-of-device-setup-classes
 * @history
 * 2023-03-20
 * 参考 https://github.com/wang-bin/qdevicewatcher
 * 由 QAbstractNativeEventFilter 过滤事件改为了创建一个 win32 窗口来接收事件
 */
class DeviceHotplug : public QObject
{
    Q_OBJECT
public:
    explicit DeviceHotplug(QObject *parent = nullptr);
    ~DeviceHotplug();

    // RegisterDeviceNotification 注册对应的 GUID 消息通知
    // 暂未考虑重复注册和注册失败的处理
    void init(const QVector<QUuid> &uuids);

    // UnregisterDeviceNotification
    // 会在析构中自动调用一次
    void free();

signals:
    // 设备插入
    void deviceAttached(quint16 vid, quint16 pid);
    // 设备拔出
    void deviceDetached(quint16 vid, quint16 pid);

private:
    QSharedPointer<DeviceHotplugPrivate> dptr;
};
