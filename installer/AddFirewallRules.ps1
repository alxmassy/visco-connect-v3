# Add Firewall Rules for Camera Server Qt6
Write-Host "Adding firewall rules for Camera Server Qt6..."

try {
    # Add TCP rule for port 7777
    netsh advfirewall firewall add rule name="Camera Server Echo" dir=in action=allow protocol=TCP localport=7777
    Write-Host "TCP rule added successfully"
    
    # Add ICMP rule
    netsh advfirewall firewall add rule name="ICMP Allow incoming V4 echo request" protocol=icmpv4:8,any dir=in action=allow
    Write-Host "ICMP rule added successfully"
} catch {
    Write-Host "Error adding firewall rules: $_"
}

exit 0
