#ifndef WIREGUARDCONFIGDIALOG_H
#define WIREGUARDCONFIGDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QListWidget>
#include <QTabWidget>
#include <QCheckBox>
#include "WireGuardManager.h"

class WireGuardConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WireGuardConfigDialog(WireGuardManager* wgManager, QWidget *parent = nullptr);
    explicit WireGuardConfigDialog(WireGuardManager* wgManager, const WireGuardConfig& config, QWidget *parent = nullptr);
    ~WireGuardConfigDialog();

    WireGuardConfig getConfiguration() const;
    void setConfiguration(const WireGuardConfig& config);

private slots:
    void generateKeypair();
    void copyPublicKeyToClipboard();
    void loadFromFile();
    void saveToFile();
    void validateAndAccept();
    void resetToDefaults();

private:
    void setupUI();
    void connectSignals();
    bool validateConfiguration();
    
    // Core components
    WireGuardManager* m_wireGuardManager;
    WireGuardConfig m_config;
      // UI Components - Simplified Interface
    QLineEdit* m_nameEdit;
    QLineEdit* m_privateKeyEdit;
    QLineEdit* m_publicKeyEdit;
    QPushButton* m_generateKeyButton;
    QPushButton* m_copyPublicKeyButton;
    QLineEdit* m_addressEdit;
    QLineEdit* m_dnsEdit;
    QSpinBox* m_listenPortSpin;
    
    // Single peer configuration (simplified)
    QLineEdit* m_peerPublicKeyEdit;
    QLineEdit* m_peerPresharedKeyEdit;
    QLineEdit* m_peerEndpointEdit;
    QLineEdit* m_peerAllowedIPsEdit;
    QSpinBox* m_peerKeepaliveSpin;
    
    // Dialog buttons
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;
    QPushButton* m_resetButton;
    QPushButton* m_loadFileButton;
    QPushButton* m_saveFileButton;
    
    // Constants
    static const QStringList DEFAULT_DNS_SERVERS;
    static const QStringList COMMON_ALLOWED_IPS;
};

#endif // WIREGUARDCONFIGDIALOG_H
