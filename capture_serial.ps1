# Capture serial data from COM34 for analysis
param(
    [int]$Duration = 10,  # Capture duration in seconds
    [string]$Port = "COM34",
    [int]$BaudRate = 115200
)

Write-Host "Capturing serial data from $Port for $Duration seconds..."

$port = New-Object System.IO.Ports.SerialPort
$port.PortName = $Port
$port.BaudRate = $BaudRate
$port.Parity = [System.IO.Ports.Parity]::None
$port.DataBits = 8
$port.StopBits = [System.IO.Ports.StopBits]::One
$port.Open()

$endTime = (Get-Date).AddSeconds($Duration)
$output = @()

while ((Get-Date) -lt $endTime) {
    if ($port.BytesToRead -gt 0) {
        $line = $port.ReadLine()
        Write-Host $line
        $output += $line
    }
    Start-Sleep -Milliseconds 10
}

$port.Close()

# Save to file
$output | Out-File -FilePath "serial_capture.txt" -Encoding UTF8
Write-Host "`nCaptured $($output.Count) lines. Saved to serial_capture.txt"

