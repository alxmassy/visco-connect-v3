#ifndef CAMERACONFIG_H
#define CAMERACONFIG_H

#include <QString>
#include <QJsonObject>

class CameraConfig
{
public:
    CameraConfig();
    CameraConfig(const QString& name, const QString& ipAddress, int port, 
                 const QString& username, const QString& password, bool enabled = true);    // Getters
    QString name() const { return m_name; }
    QString ipAddress() const { return m_ipAddress; }
    int port() const { return m_port; }
    QString username() const { return m_username; }
    QString password() const { return m_password; }
    bool isEnabled() const { return m_enabled; }
    int externalPort() const { return m_externalPort; }
    QString id() const { return m_id; }
    QString brand() const { return m_brand; }
    QString model() const { return m_model; }
    int serverId() const { return m_serverId; }
    QString serverCameraId() const { return m_serverCameraId; }    // Setters
    void setName(const QString& name) { m_name = name; }
    void setIpAddress(const QString& ipAddress) { m_ipAddress = ipAddress; }
    void setPort(int port) { m_port = port; }
    void setUsername(const QString& username) { m_username = username; }
    void setPassword(const QString& password) { m_password = password; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    void setExternalPort(int externalPort) { m_externalPort = externalPort; }
    void setBrand(const QString& brand) { m_brand = brand; }
    void setModel(const QString& model) { m_model = model; }
    void setServerId(int serverId) { m_serverId = serverId; }
    void setServerCameraId(const QString& serverCameraId) { m_serverCameraId = serverCameraId; }

    // JSON serialization
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);

    // Validation
    bool isValid() const;

private:
    QString m_id;
    QString m_name;
    QString m_ipAddress;
    int m_port;
    QString m_username;
    QString m_password;
    bool m_enabled;
    int m_externalPort;
    QString m_brand;
    QString m_model;
    int m_serverId;
    QString m_serverCameraId;
};

#endif // CAMERACONFIG_H
