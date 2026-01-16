#include "FirewallManager.h"
#include "Logger.h"
#include <QProcess>
#include <QDebug>
#include <QCoreApplication>

// Static constants
const QString FirewallManager::TCP_RULE_NAME = "Visco Connect Demo Echo";
const QString FirewallManager::ICMP_RULE_NAME = "ICMP Allow incoming V4 echo request";

FirewallManager::FirewallManager(QObject *parent)
    : QObject(parent)
{
}

bool FirewallManager::areFirewallRulesPresent()
{
    LOG_INFO("Checking firewall rules presence", "FirewallManager");
    
    bool tcpRuleExists = isRulePresent(TCP_RULE_NAME);
    bool icmpRuleExists = isRulePresent(ICMP_RULE_NAME);
    
    LOG_INFO(QString("TCP rule present: %1, ICMP rule present: %2")
             .arg(tcpRuleExists ? "Yes" : "No")
             .arg(icmpRuleExists ? "Yes" : "No"), "FirewallManager");
    
    return tcpRuleExists && icmpRuleExists;
}

bool FirewallManager::addFirewallRules()
{
    LOG_INFO("Adding firewall rules", "FirewallManager");
    
    // Add TCP rule for port 7777
    QString tcpCommand = QString("netsh advfirewall firewall add rule name=\"%1\" dir=in action=allow protocol=TCP localport=7777")
                        .arg(TCP_RULE_NAME);
    
    bool tcpSuccess = executeFirewallCommand(tcpCommand);
    if (!tcpSuccess) {
        LOG_ERROR("Failed to add TCP firewall rule", "FirewallManager");
        emit firewallError("Failed to add TCP firewall rule for port 7777");
    }
    
    // Add ICMP rule
    QString icmpCommand = QString("netsh advfirewall firewall add rule name=\"%1\" protocol=icmpv4:8,any dir=in action=allow")
                         .arg(ICMP_RULE_NAME);
    
    bool icmpSuccess = executeFirewallCommand(icmpCommand);
    if (!icmpSuccess) {
        LOG_ERROR("Failed to add ICMP firewall rule", "FirewallManager");
        emit firewallError("Failed to add ICMP firewall rule");
    }
    
    bool allSuccess = tcpSuccess && icmpSuccess;
    
    if (allSuccess) {
        LOG_INFO("All firewall rules added successfully", "FirewallManager");
        emit firewallRulesAdded();
    } else {
        LOG_ERROR("Failed to add some firewall rules", "FirewallManager");
    }
    
    return allSuccess;
}

bool FirewallManager::removeFirewallRules()
{
    LOG_INFO("Removing firewall rules", "FirewallManager");
    
    // Remove TCP rule
    QString tcpCommand = QString("netsh advfirewall firewall delete rule name=\"%1\"")
                        .arg(TCP_RULE_NAME);
    
    bool tcpSuccess = executeFirewallCommand(tcpCommand);
    
    // Remove ICMP rule
    QString icmpCommand = QString("netsh advfirewall firewall delete rule name=\"%1\"")
                         .arg(ICMP_RULE_NAME);
    
    bool icmpSuccess = executeFirewallCommand(icmpCommand);
    
    bool allSuccess = tcpSuccess && icmpSuccess;
    
    if (allSuccess) {
        LOG_INFO("All firewall rules removed successfully", "FirewallManager");
        emit firewallRulesRemoved();
    } else {
        LOG_WARNING("Some firewall rules may not have been removed", "FirewallManager");
    }
    
    return allSuccess;
}

bool FirewallManager::ensureFirewallRules()
{
    LOG_INFO("Ensuring firewall rules are present", "FirewallManager");
    
    if (areFirewallRulesPresent()) {
        LOG_INFO("Firewall rules already present", "FirewallManager");
        return true;
    }
    
    LOG_INFO("Firewall rules missing, attempting to add them", "FirewallManager");
    return addFirewallRules();
}

bool FirewallManager::executeFirewallCommand(const QString& command)
{
    LOG_INFO(QString("Executing firewall command: %1").arg(command), "FirewallManager");
    
    QProcess process;
    process.setProgram("cmd.exe");
    process.setArguments({"/c", command});
    
    // Start the process and wait for it to finish
    process.start();
    
    if (!process.waitForStarted(5000)) {
        LOG_ERROR("Failed to start firewall command process", "FirewallManager");
        return false;
    }
    
    if (!process.waitForFinished(10000)) {
        LOG_ERROR("Firewall command process timed out", "FirewallManager");
        process.kill();
        return false;
    }
    
    int exitCode = process.exitCode();
    QString output = process.readAllStandardOutput();
    QString errorOutput = process.readAllStandardError();
    
    LOG_INFO(QString("Firewall command exit code: %1").arg(exitCode), "FirewallManager");
    
    if (!output.isEmpty()) {
        LOG_INFO(QString("Firewall command output: %1").arg(output), "FirewallManager");
    }
    
    if (!errorOutput.isEmpty()) {
        LOG_WARNING(QString("Firewall command error output: %1").arg(errorOutput), "FirewallManager");
    }
    
    return exitCode == 0;
}

bool FirewallManager::isRulePresent(const QString& ruleName)
{
    QString checkCommand = QString("netsh advfirewall firewall show rule name=\"%1\"").arg(ruleName);
    
    QProcess process;
    process.setProgram("cmd.exe");
    process.setArguments({"/c", checkCommand});
    
    process.start();
    
    if (!process.waitForStarted(5000)) {
        LOG_ERROR("Failed to start rule check process", "FirewallManager");
        return false;
    }
    
    if (!process.waitForFinished(10000)) {
        LOG_ERROR("Rule check process timed out", "FirewallManager");
        process.kill();
        return false;
    }
    
    int exitCode = process.exitCode();
    QString output = process.readAllStandardOutput();
    
    // If the rule exists, netsh will return exit code 0 and show rule details
    // If the rule doesn't exist, it will return a non-zero exit code
    bool ruleExists = (exitCode == 0) && output.contains(ruleName);
    
    LOG_DEBUG(QString("Rule '%1' exists: %2").arg(ruleName).arg(ruleExists ? "Yes" : "No"), "FirewallManager");
    
    return ruleExists;
}
