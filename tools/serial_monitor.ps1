# Nucleo ST-LINK sanal COM portundaki telemetriyi canlı gösterir.
# Kullanım: powershell -ExecutionPolicy Bypass -File tools\serial_monitor.ps1 [-Port COM16] [-Baud 115200]
# Çıkış: Ctrl+C
param(
    [string]$Port = "",
    [int]$Baud = 115200
)

if (-not $Port) {
    $dev = Get-CimInstance Win32_PnPEntity |
        Where-Object { $_.Name -match 'STLink Virtual COM Port' } |
        Select-Object -First 1
    if (-not $dev) { Write-Host "ST-LINK COM portu bulunamadi. Kart USB'ye bagli mi?" -ForegroundColor Red; exit 1 }
    $Port = [regex]::Match($dev.Name, 'COM\d+').Value
}

$sp = New-Object System.IO.Ports.SerialPort $Port, $Baud, 'None', 8, 'One'
$sp.NewLine = "`r`n"
$sp.ReadTimeout = 1000
try {
    $sp.Open()
} catch {
    Write-Host "$Port acilamadi (baska bir program kullaniyor olabilir): $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

Write-Host "=== $Port @ $Baud dinleniyor - cikmak icin Ctrl+C ===" -ForegroundColor Cyan
try {
    while ($true) {
        try { $line = $sp.ReadLine() } catch [System.TimeoutException] { continue }
        if     ($line -match '^\[OK')     { Write-Host $line -ForegroundColor Green }
        elseif ($line -match '^\[KAYIP')  { Write-Host $line -ForegroundColor Red }
        elseif ($line -match '^\[ARMING') { Write-Host $line -ForegroundColor Yellow }
        else                              { Write-Host $line }
    }
} finally {
    $sp.Close()
}
