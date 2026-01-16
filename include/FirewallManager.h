#ifndef FIREWALLMANAGER_H
#define FIREWALLMANAGER_H

#include <QObject>
#include <QString>

/**
 * @brief The FirewallManager class handles Windows Firewall rule management
 * 
 * This class provides functionality to add and remove Windows Firewall rules
 * required for the application to function properly.
 */
class FirewallManager : public QObject
{
    Q_OBJECT

public:
    explicit FirewallManager(QObject *parent = nullptr);
    
    /**
     * @brief Check if the required firewall rules exist
     * @return true if all required rules exist
     */
    bool areFirewallRulesPresent();
    
    /**
     * @brief Add all required firewall rules
     * @return true if all rules were added successfully
     */
    bool addFirewallRules();
    
    /**
     * @brief Remove all application firewall rules
     * @return true if all rules were removed successfully
     */
    bool removeFirewallRules();
    
    /**
     * @brief Check and add firewall rules if needed (safe method)
     * @return true if rules are present or were added successfully
     */
    bool ensureFirewallRules();

signals:
    void firewallRulesAdded();
    void firewallRulesRemoved();
    void firewallError(const QString& error);

private:
    bool executeFirewallCommand(const QString& command);
    bool isRulePresent(const QString& ruleName);
    
    // Rule names
    static const QString TCP_RULE_NAME;
    static const QString ICMP_RULE_NAME;
};

#endif // FIREWALLMANAGER_H
