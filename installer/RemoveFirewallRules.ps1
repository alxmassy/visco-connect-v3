# Remove Firewall Rules for Camera Server Qt6
Write-Host "Removing firewall rules for Camera Server Qt6..."

try {
    # Remove TCP rule
    netsh advfirewall firewall delete rule name="Camera Server Echo"
    Write-Host "TCP rule removed successfully"
    
    # Remove ICMP rule
    netsh advfirewall firewall delete rule name="ICMP Allow incoming V4 echo request"
    Write-Host "ICMP rule removed successfully"
} catch {
    Write-Host "Error removing firewall rules: $_"
}

exit 0
