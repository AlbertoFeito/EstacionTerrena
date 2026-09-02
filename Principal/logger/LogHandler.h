#ifndef LOGHANDLER_H
#define LOGHANDLER_H

#include "Singleton.h"

#include <QWidget>
#include <QPlainTextEdit>

//#define LogHandlerInstance Singleton<LogHandler>::getInstance()

struct LogHandlerPrivate;

class LogHandler : public QPlainTextEdit {
    Q_OBJECT

public:
    // Método estático para obtener la instancia singleton
    static LogHandler& getInstance() {
        static LogHandler instance;
        return instance;
    }

    // Métodos existentes
    void uninstallMessageHandler();
    void installMessageHandler();

    // Nuevo método para establecer texto
    void appendLogMessage(const QString &message);
    void setLogHandlerVisible(bool visible);

    ~LogHandler();
    QString getLastMsg() const;
    static void updateLogText(const QString &msg);
protected:
    // Constructor protegido
    LogHandler(QWidget *parent = nullptr);

private:
    // Prevenir la copia y asignación
    LogHandler(const LogHandler&) = delete;
    LogHandler& operator=(const LogHandler&) = delete;

    // Miembro de datos privado
    LogHandlerPrivate *d;
};

#endif // LOGHANDLER_H
