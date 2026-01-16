#include "CameraConfig.h"
#include <QJsonObject>
#include <QUuid>
#include <QHostAddress>

CameraConfig::CameraConfig()
    : m_port(554)
    , m_enabled(true)
    , m_externalPort(8551)
    , m_brand("Generic")
    , m_serverId(-1)
    , m_serverCameraId("")
{
    m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
}

CameraConfig::CameraConfig(const QString& name, const QString& ipAddress, int port, 
                          const QString& username, const QString& password, bool enabled)
    : m_name(name)
    , m_ipAddress(ipAddress)
    , m_port(port)
    , m_username(username)
    , m_password(password)
    , m_enabled(enabled)
    , m_externalPort(8551)
    , m_brand("Generic")
    , m_serverId(-1)
    , m_serverCameraId("")
{
    m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QJsonObject CameraConfig::toJson() const
{
    QJsonObject json;
    json["id"] = m_id;
    json["name"] = m_name;
    json["ipAddress"] = m_ipAddress;
    json["port"] = m_port;
    json["username"] = m_username;
    json["password"] = m_password;
    json["enabled"] = m_enabled;
    json["externalPort"] = m_externalPort;
    json["brand"] = m_brand;
    json["model"] = m_model;
    json["serverId"] = m_serverId;
    json["serverCameraId"] = m_serverCameraId;
    return json;
}

void CameraConfig::fromJson(const QJsonObject& json)
{
    m_id = json["id"].toString();
    m_name = json["name"].toString();
    m_ipAddress = json["ipAddress"].toString();
    m_port = json["port"].toInt(554);
    m_username = json["username"].toString();
    m_password = json["password"].toString();
    m_enabled = json["enabled"].toBool(true);
    m_externalPort = json["externalPort"].toInt(8551);
    m_brand = json["brand"].toString("Generic");
    m_model = json["model"].toString();
    m_serverId = json["serverId"].toInt(-1);
    m_serverCameraId = json["serverCameraId"].toString("");
    
    // Generate ID if not present (for backward compatibility)
    if (m_id.isEmpty()) {
        m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
}

bool CameraConfig::isValid() const
{
    if (m_name.isEmpty() || m_ipAddress.isEmpty()) {
        return false;
    }
    
    QHostAddress address(m_ipAddress);
    if (address.isNull()) {
        return false;
    }
    
    if (m_port < 1 || m_port > 65535) {
        return false;
    }
    
    if (m_externalPort < 1 || m_externalPort > 65535) {
        return false;
    }
    
    return true;
}
