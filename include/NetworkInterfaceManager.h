#ifndef NETWORKINTERFACEMANAGER_H
#define NETWORKINTERFACEMANAGER_H

#include <QObject>
#include <QNetworkInterface>
#include <QTimer>
#include <QStringList>
#include <QHostAddress>

class NetworkInterfaceManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkInterfaceManager(QObject *parent = nullptr);
    ~NetworkInterfaceManager();

    // Interface monitoring
    void startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const;

    // Interface information
    QList<QNetworkInterface> getAllInterfaces() const;
    QList<QNetworkInterface> getActiveInterfaces() const;
    QNetworkInterface getWireGuardInterface() const;
    bool hasWireGuardInterface() const;
    
    // Address information
    QList<QHostAddress> getAllAddresses() const;
    QList<QHostAddress> getWireGuardAddresses() const;
    QHostAddress getWireGuardAddress() const;
    QHostAddress getBestLocalAddress(const QHostAddress& destAddress) const;
    
    // Interface status
    bool isWireGuardActive() const;
    QString getInterfaceStatus() const;

signals:
    void interfaceRemoved(const QString& interfaceName);
    void wireGuardInterfaceStateChanged(bool active);
    void interfacesChanged();

private slots:
    void checkInterfaces();

private:
    void updateInterfaceList();
    bool isWireGuardInterface(const QNetworkInterface& netInterface) const;
    
    QTimer* m_monitorTimer;
    QList<QNetworkInterface> m_lastInterfaces;
    bool m_lastWireGuardState;
    bool m_monitoring;
    
    static const int MONITOR_INTERVAL_MS = 2000; // Check every 2 seconds
};

#endif // NETWORKINTERFACEMANAGER_H
