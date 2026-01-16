#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QObject>
#include <QMutex>
#include <QTextStream>
#include <QFile>

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger : public QObject
{
    Q_OBJECT

public:
    static Logger& instance();
    
    void setLogFile(const QString& filePath);
    void setLogLevel(LogLevel level);
    
    void log(LogLevel level, const QString& message, const QString& category = "General");
    
    void debug(const QString& message, const QString& category = "General");
    void info(const QString& message, const QString& category = "General");
    void warning(const QString& message, const QString& category = "General");
    void error(const QString& message, const QString& category = "General");

signals:
    void logMessage(const QString& message);

private:
    Logger();
    ~Logger();
    
    QString formatMessage(LogLevel level, const QString& message, const QString& category) const;
    QString logLevelToString(LogLevel level) const;
    
    QMutex m_mutex;
    QFile m_logFile;
    QTextStream m_logStream;
    LogLevel m_logLevel;
    bool m_logToFile;
};

// Convenience macros
#define LOG_DEBUG(msg, cat) Logger::instance().debug(msg, cat)
#define LOG_INFO(msg, cat) Logger::instance().info(msg, cat)
#define LOG_WARNING(msg, cat) Logger::instance().warning(msg, cat)
#define LOG_ERROR(msg, cat) Logger::instance().error(msg, cat)

#endif // LOGGER_H
