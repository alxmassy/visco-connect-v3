#include "Logger.h"
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QMutexLocker>

Logger::Logger()
    : m_logLevel(LogLevel::Info)
    , m_logToFile(false)
{
    // Set default log file path
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(appDataPath);
    setLogFile(appDataPath + "/visco-connect.log");
}

Logger::~Logger()
{
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
}

Logger& Logger::instance()
{
    static Logger instance;
    return instance;
}

void Logger::setLogFile(const QString& filePath)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
    
    m_logFile.setFileName(filePath);
    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        m_logStream.setDevice(&m_logFile);
        m_logToFile = true;
    } else {
        m_logToFile = false;
    }
}

void Logger::setLogLevel(LogLevel level)
{
    QMutexLocker locker(&m_mutex);
    m_logLevel = level;
}

void Logger::log(LogLevel level, const QString& message, const QString& category)
{
    if (level < m_logLevel) {
        return;
    }
    
    QMutexLocker locker(&m_mutex);
    
    QString formattedMessage = formatMessage(level, message, category);
    
    // Emit signal for UI logging
    emit logMessage(formattedMessage);
    
    // Write to file if enabled
    if (m_logToFile && m_logFile.isOpen()) {
        m_logStream << formattedMessage << Qt::endl;
        m_logStream.flush();
    }
}

void Logger::debug(const QString& message, const QString& category)
{
    log(LogLevel::Debug, message, category);
}

void Logger::info(const QString& message, const QString& category)
{
    log(LogLevel::Info, message, category);
}

void Logger::warning(const QString& message, const QString& category)
{
    log(LogLevel::Warning, message, category);
}

void Logger::error(const QString& message, const QString& category)
{
    log(LogLevel::Error, message, category);
}

QString Logger::formatMessage(LogLevel level, const QString& message, const QString& category) const
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString levelStr = logLevelToString(level);
    
    return QString("[%1] [%2] [%3] %4")
           .arg(timestamp)
           .arg(levelStr)
           .arg(category)
           .arg(message);
}

QString Logger::logLevelToString(LogLevel level) const
{
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO ";
        case LogLevel::Warning: return "WARN ";
        case LogLevel::Error:   return "ERROR";
        default:                return "UNKNW";
    }
}
