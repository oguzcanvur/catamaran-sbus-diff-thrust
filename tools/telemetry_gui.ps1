# Katamaran telemetri arayüzü - ST-LINK sanal COM portundan gelen satırları görselleştirir.
# Kullanım: powershell -ExecutionPolicy Bypass -File tools\telemetry_gui.ps1 [-Port COM16]
param(
    [string]$Port = "",
    [int]$Baud = 115200
)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()

if (-not $Port) {
    $dev = Get-CimInstance Win32_PnPEntity |
        Where-Object { $_.Name -match 'STLink Virtual COM Port' } | Select-Object -First 1
    if ($dev) { $Port = [regex]::Match($dev.Name, 'COM\d+').Value }
}
if (-not $Port) {
    [System.Windows.Forms.MessageBox]::Show("ST-LINK COM portu bulunamadı. Kart USB'ye bağlı mı?", "Telemetri") | Out-Null
    exit 1
}

$sp = New-Object System.IO.Ports.SerialPort $Port, $Baud, 'None', 8, 'One'
try { $sp.Open() } catch {
    [System.Windows.Forms.MessageBox]::Show("$Port açılamadı. Başka bir program (terminal, CubeIDE konsolu) portu kullanıyor olabilir.`n`n$($_.Exception.Message)", "Telemetri") | Out-Null
    exit 1
}

$S = @{
    ch1 = 1500; ch2 = 1500; ch3 = 0; ch4 = 0; sol = 1500; sag = 1500
    fs = 0; lost = 0; pkt = 0; err = 0; st = '---'
    last = [datetime]::MinValue; buf = ''
    raw1 = $null; raw2 = $null; nl = $null; nr = $null; sw = $null
    calOn = $false; min1 = $null; max1 = $null; min2 = $null; max2 = $null; c1 = $null; c2 = $null
}
$rx = 'CH1\(dumen\)=\s*(\d+)\s+CH2\(gaz\)=\s*(\d+)\s+CH3=\s*(\d+)\s+CH4=\s*(\d+)\s*\|\s*SOL=\s*(\d+)\s+SAG=\s*(\d+)\s*\|\s*FS=(\d)\s+LOST=(\d)\s*\|\s*paket=(\d+)\s+hata=(\d+)(?:\s*\|\s*RAW1=\s*(\d+)\s+RAW2=\s*(\d+))?(?:\s*\|\s*NL=\s*(\d+)\s+NR=\s*(\d+))?(?:\s*\|\s*SW=\s*(\d+))?'

# ---------- Renkler / yazı tipleri ----------
$cBg     = [System.Drawing.Color]::FromArgb(24, 26, 31)
$cPanel  = [System.Drawing.Color]::FromArgb(36, 39, 46)
$cGrid   = [System.Drawing.Color]::FromArgb(70, 74, 84)
$cText   = [System.Drawing.Color]::FromArgb(230, 232, 236)
$cMuted  = [System.Drawing.Color]::FromArgb(150, 155, 165)
$cFwd    = [System.Drawing.Color]::FromArgb(46, 194, 126)
$cRev    = [System.Drawing.Color]::FromArgb(240, 140, 60)
$cDot    = [System.Drawing.Color]::FromArgb(80, 160, 255)
$cDead   = [System.Drawing.Color]::FromArgb(55, 60, 70)
$cOk     = [System.Drawing.Color]::FromArgb(46, 194, 126)
$cBad    = [System.Drawing.Color]::FromArgb(235, 80, 80)
$cWarn   = [System.Drawing.Color]::FromArgb(240, 190, 60)

$fTitle  = New-Object System.Drawing.Font('Segoe UI', 14, [System.Drawing.FontStyle]::Bold)
$fLabel  = New-Object System.Drawing.Font('Segoe UI', 10)
$fValue  = New-Object System.Drawing.Font('Consolas', 12, [System.Drawing.FontStyle]::Bold)
$fSmall  = New-Object System.Drawing.Font('Consolas', 10)

function Norm([int]$us) { [Math]::Max(-1.0, [Math]::Min(1.0, ($us - 1500) / 500.0)) }

# ---------- Form ----------
$form = New-Object System.Windows.Forms.Form
$form.Text = "Katamaran Telemetri - $Port"
$form.ClientSize = New-Object System.Drawing.Size(930, 690)
$form.FormBorderStyle = 'FixedSingle'
$form.MaximizeBox = $false
$form.BackColor = $cBg
$form.StartPosition = 'CenterScreen'

$pb = New-Object System.Windows.Forms.PictureBox
$pb.Dock = 'Fill'
$pb.BackColor = $cBg
$form.Controls.Add($pb)

# ---------- Kalibrasyon paneli ----------
$cal = New-Object System.Windows.Forms.Panel
$cal.Dock = 'Right'
$cal.Width = 290
$cal.BackColor = $cPanel
$form.Controls.Add($cal)

function New-Label($text, $y, $h, $font, $color) {
    $l = New-Object System.Windows.Forms.Label
    $l.Text = $text; $l.Left = 14; $l.Top = $y; $l.Width = 266; $l.Height = $h
    $l.Font = $font; $l.ForeColor = $color; $l.BackColor = $cPanel
    $cal.Controls.Add($l); return $l
}
function New-Btn($text, $y) {
    $b = New-Object System.Windows.Forms.Button
    $b.Text = $text; $b.Left = 14; $b.Top = $y; $b.Width = 262; $b.Height = 32
    $b.FlatStyle = 'Flat'; $b.ForeColor = $cText; $b.BackColor = $cDead
    $b.Font = $fLabel
    $cal.Controls.Add($b); return $b
}

[void](New-Label "KALİBRASYON (sağ kol)" 12 26 $fTitle $cText)
$lblTable = New-Label "" 46 80 $fSmall $cText
$lblHelp  = New-Label ("1) Trim'leri sıfırla, butona bas`n" +
                       "2) Sağ kolu 4 uca ve çapraz turla`n" +
                       "3) Kolu bırak, merkezi kaydet`n" +
                       "4) Kaydet, sonra karta yükle") 132 72 $fLabel $cMuted
$btnReset  = New-Btn "1) Min/Max ölçümünü başlat" 210
$btnCenter = New-Btn "2) Merkezi kaydet (kol bırakık)" 250
$btnSave   = New-Btn "3) Kalibrasyonu kaydet" 290
$lblMsg    = New-Label "" 330 100 $fLabel $cMuted

# ---------- Motor nötr testi paneli ----------
$nt = New-Object System.Windows.Forms.Panel
$nt.Dock = 'Bottom'
$nt.Height = 250
$nt.BackColor = [System.Drawing.Color]::FromArgb(30, 33, 39)
$form.Controls.Add($nt)

$T = @{ l = 1500; r = 1500; lo_l = $null; hi_l = $null; lo_r = $null; hi_r = $null; nudInit = $false; pending = $null }

function Add-Ctl($c, $x, $y, $w, $h) {
    $c.Left = $x; $c.Top = $y; $c.Width = $w; $c.Height = $h
    $nt.Controls.Add($c); return $c
}
function New-NtLabel($text, $x, $y, $w, $h, $font, $color) {
    $l = New-Object System.Windows.Forms.Label
    $l.Text = $text; $l.Font = $font; $l.ForeColor = $color; $l.BackColor = $nt.BackColor
    return (Add-Ctl $l $x $y $w $h)
}
function New-NtBtn($text, $x, $y, $w) {
    $b = New-Object System.Windows.Forms.Button
    $b.Text = $text; $b.FlatStyle = 'Flat'; $b.ForeColor = $cText; $b.BackColor = $cDead; $b.Font = $fLabel
    return (Add-Ctl $b $x $y $w 30)
}

[void](New-NtLabel "MOTOR NÖTR TESTİ" 14 10 230 26 $fTitle $cText)
$chkTest = New-Object System.Windows.Forms.CheckBox
$chkTest.Text = "Test modunu aç  (PERVANELER SÖKÜLÜ OLMALI)"
$chkTest.Font = $fLabel; $chkTest.ForeColor = $cWarn; $chkTest.BackColor = $nt.BackColor
[void](Add-Ctl $chkTest 250 12 420 24)

$rows = @{}
foreach ($k in 'l', 'r') {
    $y = if ($k -eq 'l') { 48 } else { 108 }
    $name = if ($k -eq 'l') { 'SOL' } else { 'SAĞ' }
    [void](New-NtLabel $name 14 ($y + 6) 50 22 $fValue $cText)

    $tb = New-Object System.Windows.Forms.TrackBar
    $tb.Minimum = 1350; $tb.Maximum = 1650; $tb.Value = 1500
    $tb.TickFrequency = 25; $tb.SmallChange = 1; $tb.LargeChange = 10
    $tb.BackColor = $nt.BackColor
    [void](Add-Ctl $tb 64 $y 400 45)

    $val = New-NtLabel "1500 µs" 470 ($y + 6) 90 22 $fValue $cText
    $bm  = New-NtBtn "−1" 562 $y 40
    $bp  = New-NtBtn "+1" 606 $y 40
    $blo = New-NtBtn "Alt sınır" 656 $y 90
    $bhi = New-NtBtn "Üst sınır" 750 $y 90
    $b15 = New-NtBtn "1500" 844 $y 70
    $res = New-NtLabel "alt: --   üst: --   nötr: --" 562 ($y + 34) 360 20 $fSmall $cMuted

    $rows[$k] = @{ tb = $tb; val = $val; res = $res }

    $tb.Tag = $k; $bm.Tag = $k; $bp.Tag = $k; $blo.Tag = $k; $bhi.Tag = $k; $b15.Tag = $k
    $tb.Add_ValueChanged({ $T[$this.Tag] = $this.Value; Update-NtRows })
    $bm.Add_Click({ $r = $rows[$this.Tag].tb; if ($r.Value -gt $r.Minimum) { $r.Value = $r.Value - 1 } })
    $bp.Add_Click({ $r = $rows[$this.Tag].tb; if ($r.Value -lt $r.Maximum) { $r.Value = $r.Value + 1 } })
    $b15.Add_Click({ $rows[$this.Tag].tb.Value = 1500 })
    $blo.Add_Click({ $T["lo_$($this.Tag)"] = $T[$this.Tag]; Update-NtRows; Set-NudFromMeasure $this.Tag })
    $bhi.Add_Click({ $T["hi_$($this.Tag)"] = $T[$this.Tag]; Update-NtRows; Set-NudFromMeasure $this.Tag })
}

function New-Nud($x, $y) {
    $n = New-Object System.Windows.Forms.NumericUpDown
    $n.Minimum = 1350; $n.Maximum = 1650; $n.Value = 1500; $n.Font = $fValue
    $n.BackColor = $cPanel; $n.ForeColor = $cText
    return (Add-Ctl $n $x $y 80 28)
}
[void](New-NtLabel "Sol nötr" 14 172 70 22 $fLabel $cText)
$nudL = New-Nud 86 168
[void](New-NtLabel "Sağ nötr" 180 172 70 22 $fLabel $cText)
$nudR = New-Nud 252 168
$btnTrimSave = New-NtBtn "Karta yükle (kalıcı)" 348 167 180
$lblBoard = New-NtLabel "Karttaki nötr: --" 540 172 380 22 $fValue $cMuted
$lblNt = New-NtLabel ("Kaydırıcıyı yavaşça oynat. Motorun DURDUĞU en düşük değerde 'Alt sınır', " +
                      "en yüksek değerde 'Üst sınır' bas; nötr kutuya yazılır. Elle de girebilirsin.") 14 206 900 40 $fLabel $cMuted

function Set-NudFromMeasure($k) {
    $n = Get-Neutral $k
    if ($null -eq $n) { return }
    if ($k -eq 'l') { $nudL.Value = $n } else { $nudR.Value = $n }
}

function Get-Neutral($k) {
    $lo = $T["lo_$k"]; $hi = $T["hi_$k"]
    if ($null -eq $lo -or $null -eq $hi -or $lo -gt $hi) { return $null }
    return [int][Math]::Round(($lo + $hi) / 2.0)
}
function Update-NtRows {
    foreach ($k in 'l', 'r') {
        $rows[$k].val.Text = "$($T[$k]) µs"
        $lo = $T["lo_$k"]; $hi = $T["hi_$k"]; $n = Get-Neutral $k
        $rows[$k].res.Text = "alt: {0}   üst: {1}   nötr: {2}" -f `
            $(if ($null -eq $lo) { '--' } else { $lo }),
            $(if ($null -eq $hi) { '--' } else { $hi }),
            $(if ($null -eq $n) { '--' } else { $n })
    }
}

$chkTest.Add_CheckedChanged({
    if (-not $chkTest.Checked) {
        try { $sp.Write("X`n") } catch { }
        $lblNt.ForeColor = $cMuted; $lblNt.Text = "Test modu kapandı. Motorlar kumandaya döndü."
    } else {
        $lblNt.ForeColor = $cWarn
        $lblNt.Text = "Test modu AÇIK: ESC'lere kaydırıcıdaki değer gidiyor (kumanda devre dışı)."
    }
})

$btnTrimSave.Add_Click({
    $nl = [int]$nudL.Value; $nr = [int]$nudR.Value
    try { $sp.Write("N $nl $nr`n") } catch {
        $lblNt.ForeColor = $cBad; $lblNt.Text = "Gönderilemedi: $($_.Exception.Message)"; return
    }
    $T.pending = @{ l = $nl; r = $nr; t = Get-Date }
    $lblNt.ForeColor = $cWarn; $lblNt.Text = "Gönderildi: sol $nl µs, sağ $nr µs. Kart onayı bekleniyor..."

    # Kaynak koddaki varsayılanları da eşitle (flash silinirse bunlar kullanılır)
    $path = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\Core\Inc\motor_trim.h'))
    $stamp = Get-Date -Format 'yyyy-MM-dd HH:mm'
    $txt = @"
/* ESC nötr (durma) noktaları - µs. VARSAYILAN degerler:
 * Asil degerler kartin flash'inda saklanir (settings.c), arayuzden "Karta yukle" ile degisir.
 * Flash'ta gecerli kayit yoksa bunlar kullanilir. Arayuz bu dosyayi da gunceller.
 * Son guncelleme: $stamp */
#ifndef MOTOR_TRIM_H
#define MOTOR_TRIM_H

#define LEFT_NEUTRAL_US    $nl
#define RIGHT_NEUTRAL_US   $nr

#endif /* MOTOR_TRIM_H */
"@
    try { [IO.File]::WriteAllText($path, ($txt -replace "`r`n", "`n"), (New-Object Text.UTF8Encoding $false)) } catch { }
})

function Update-Board {
    if ($null -eq $S.nl) { return }
    $lblBoard.Text = "Karttaki nötr: sol $($S.nl)  sağ $($S.nr)"
    $lblBoard.ForeColor = $cText
    if (-not $T.nudInit) { $nudL.Value = $S.nl; $nudR.Value = $S.nr; $T.nudInit = $true }
    if ($T.pending) {
        if ($S.nl -eq $T.pending.l -and $S.nr -eq $T.pending.r) {
            $lblNt.ForeColor = $cOk
            $lblNt.Text = "Kart onayladı: sol $($S.nl) µs, sağ $($S.nr) µs flash'a kaydedildi. Kapatıp açsan da korunur."
            $T.pending = $null
        } elseif (((Get-Date) - $T.pending.t).TotalSeconds -gt 2) {
            $lblNt.ForeColor = $cBad
            $lblNt.Text = "Kart onaylamadı (değerler 1350-1650 aralığında olmalı). Tekrar dene."
            $T.pending = $null
        }
    }
}

function Fmt($v) { if ($null -eq $v) { '  -- ' } else { '{0,5}' -f $v } }
function Update-CalTable {
    $lblTable.Text = ("       ham   min  merkez  max`n" +
                      "CH1  {0} {1} {2} {3}`n" +
                      "CH2  {4} {5} {6} {7}`n`n" +
                      "Ölçüm: {8}") -f `
        (Fmt $S.raw1), (Fmt $S.min1), (Fmt $S.c1), (Fmt $S.max1),
        (Fmt $S.raw2), (Fmt $S.min2), (Fmt $S.c2), (Fmt $S.max2),
        $(if ($S.calOn) { 'AKTİF' } else { 'kapalı' })
}

$btnReset.Add_Click({
    if ($null -eq $S.raw1) {
        $lblMsg.ForeColor = $cBad; $lblMsg.Text = "Ham değer gelmiyor. Kart güncel yazılımla çalışıyor mu?"; return
    }
    $S.min1 = $S.raw1; $S.max1 = $S.raw1; $S.min2 = $S.raw2; $S.max2 = $S.raw2
    $S.c1 = $null; $S.c2 = $null; $S.calOn = $true
    $lblMsg.ForeColor = $cWarn
    $lblMsg.Text = "Sağ kolu yavaşça tüm uçlara götür (ileri, geri, sağ, sol) ve birkaç tur çevir."
})
$btnCenter.Add_Click({
    if ($null -eq $S.raw1) { return }
    $S.c1 = $S.raw1; $S.c2 = $S.raw2
    $lblMsg.ForeColor = $cOk; $lblMsg.Text = "Merkez kaydedildi: CH1=$($S.c1) CH2=$($S.c2)"
})
$btnSave.Add_Click({
    $err = @()
    foreach ($k in 1, 2) {
        $mn = $S."min$k"; $c = $S."c$k"; $mx = $S."max$k"
        if ($null -eq $mn -or $null -eq $c -or $null -eq $mx) { $err += "CH${k}: ölçüm eksik"; continue }
        if (-not ($mn -lt $c -and $c -lt $mx)) { $err += "CH${k}: min < merkez < max olmalı"; continue }
        if (($c - $mn) -lt 300 -or ($mx - $c) -lt 300) { $err += "CH${k}: kol uçlara tam gitmemiş (aralık çok dar)" }
    }
    if ($err.Count) { $lblMsg.ForeColor = $cBad; $lblMsg.Text = ($err -join "`n"); return }

    $path = Join-Path $PSScriptRoot '..\Core\Inc\rc_calibration.h'
    $stamp = Get-Date -Format 'yyyy-MM-dd HH:mm'
    $txt = @"
/* Kumanda kalibrasyonu - ham S.BUS degerleri (0..2047).
 * Bu dosya tools/telemetry_gui.ps1 "Kalibrasyonu kaydet" butonu ile yeniden yazilir.
 * Olcum: $stamp */
#ifndef RC_CALIBRATION_H
#define RC_CALIBRATION_H

#define CAL_CH1_MIN      $($S.min1)
#define CAL_CH1_CENTER   $($S.c1)
#define CAL_CH1_MAX      $($S.max1)

#define CAL_CH2_MIN      $($S.min2)
#define CAL_CH2_CENTER   $($S.c2)
#define CAL_CH2_MAX      $($S.max2)

#endif /* RC_CALIBRATION_H */
"@
    [IO.File]::WriteAllText([IO.Path]::GetFullPath($path), ($txt -replace "`r`n", "`n"), (New-Object Text.UTF8Encoding $false))
    $S.calOn = $false
    $lblMsg.ForeColor = $cOk
    $lblMsg.Text = "Kaydedildi: Core\Inc\rc_calibration.h`nŞimdi yazılımı derleyip karta yükle."
})

$pb.Add_Paint({
    $g = $_.Graphics
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::ClearTypeGridFit

    $bText  = New-Object System.Drawing.SolidBrush $cText
    $bMuted = New-Object System.Drawing.SolidBrush $cMuted
    $bPanel = New-Object System.Drawing.SolidBrush $cPanel
    $pGrid  = New-Object System.Drawing.Pen $cGrid, 1

    # ----- Durum satırı -----
    $stale = ((Get-Date) - $S.last).TotalSeconds -gt 1
    if ($stale)                  { $stTxt = "VERİ YOK";            $stCol = $cBad }
    elseif ($S.st -eq 'BASLAT')  { $stTxt = "BAŞLATILIYOR (ESC'ler hazırlanıyor)"; $stCol = $cWarn }
    elseif ($S.st -eq 'TEST')    { $stTxt = "TEST MODU - motorlar PC'den sürülüyor"; $stCol = $cWarn }
    elseif ($S.st -eq 'KAYIP')   { $stTxt = "SİNYAL KAYIP - motorlar durduruldu"; $stCol = $cBad }
    elseif ($S.st -eq 'DISARM')  { $stTxt = "DISARM - motorlar kilitli (CH10 kapalı)"; $stCol = $cMuted }
    elseif ($S.st -eq 'ARM?')    { $stTxt = "ARM BEKLİYOR - kolu ortala / CH10'u kapat-aç"; $stCol = $cWarn }
    else                         { $stTxt = "ARMED - sürüş aktif"; $stCol = $cOk }
    $g.FillEllipse((New-Object System.Drawing.SolidBrush $stCol), 20, 18, 16, 16)
    $g.DrawString($stTxt, $fTitle, (New-Object System.Drawing.SolidBrush $stCol), 44, 11)

    # ----- Sağ joystick kutusu -----
    $bx = 20; $by = 60; $bs = 260; $half = $bs / 2
    $g.FillRectangle($bPanel, $bx, $by, $bs, $bs)
    $dz = $half * 35 / 500.0     # ölü bölge ±35 µs
    $g.FillRectangle((New-Object System.Drawing.SolidBrush $cDead), ($bx + $half - $dz), ($by + $half - $dz), (2 * $dz), (2 * $dz))
    $g.DrawLine($pGrid, $bx + $half, $by, $bx + $half, $by + $bs)
    $g.DrawLine($pGrid, $bx, $by + $half, $bx + $bs, $by + $half)
    $g.DrawRectangle($pGrid, $bx, $by, $bs, $bs)
    $g.DrawString("İLERİ", $fSmall, $bMuted, $bx + $half - 20, $by + 4)
    $g.DrawString("GERİ",  $fSmall, $bMuted, $bx + $half - 16, $by + $bs - 20)
    $g.DrawString("SOL",   $fSmall, $bMuted, $bx + 4, $by + $half - 18)
    $g.DrawString("SAĞ",   $fSmall, $bMuted, $bx + $bs - 32, $by + $half - 18)

    $dx = $bx + $half + (Norm $S.ch1) * ($half - 10)
    $dy = $by + $half - (Norm $S.ch2) * ($half - 10)
    $dotCol = if ($stale) { $cGrid } else { $cDot }
    $g.FillEllipse((New-Object System.Drawing.SolidBrush $dotCol), $dx - 10, $dy - 10, 20, 20)

    $g.DrawString("Sağ kol", $fLabel, $bText, $bx, $by + $bs + 8)
    $g.DrawString(("CH1 dümen {0,4} µs" -f $S.ch1), $fSmall, $bText, $bx, $by + $bs + 30)
    $g.DrawString(("CH2 gaz   {0,4} µs" -f $S.ch2), $fSmall, $bText, $bx, $by + $bs + 48)

    # ----- Motor çubukları -----
    $motors = @(@{ n = 'SOL MOTOR'; v = $S.sol; x = 330 }, @{ n = 'SAĞ MOTOR'; v = $S.sag; x = 480 })
    foreach ($m in $motors) {
        $mx = $m.x; $mw = 110; $my = 60; $mh = 260; $cy = $my + $mh / 2
        $g.FillRectangle($bPanel, $mx, $my, $mw, $mh)
        $n = Norm $m.v
        $h = [Math]::Abs($n) * ($mh / 2)
        if ($n -ge 0) { $g.FillRectangle((New-Object System.Drawing.SolidBrush $cFwd), $mx, $cy - $h, $mw, $h) }
        else          { $g.FillRectangle((New-Object System.Drawing.SolidBrush $cRev), $mx, $cy, $mw, $h) }
        $g.DrawLine((New-Object System.Drawing.Pen $cText, 2), $mx, $cy, $mx + $mw, $cy)
        $g.DrawRectangle($pGrid, $mx, $my, $mw, $mh)

        $g.DrawString($m.n, $fLabel, $bText, $mx, $my + $mh + 8)
        $g.DrawString(("{0,4} µs" -f $m.v), $fValue, $bText, $mx, $my + $mh + 28)
        $pct = [int]([Math]::Round($n * 100))
        $dir = if ($pct -gt 0) { "İLERİ" } elseif ($pct -lt 0) { "GERİ" } else { "DUR" }
        $g.DrawString(("{0} %{1}" -f $dir, [Math]::Abs($pct)), $fSmall, $bMuted, $mx, $my + $mh + 50)
    }

    # ----- Alt bilgi -----
    $info = "CH10(arm) {6}   CH3 {0}  CH4 {1}   |   paket {2}  hata {3}   |   FS {4}  LOST {5}" -f `
        $S.ch3, $S.ch4, $S.pkt, $S.err, $S.fs, $S.lost, $(if ($null -eq $S.sw) { '--' } else { $S.sw })
    $g.DrawString($info, $fSmall, $bMuted, 20, 412)
})

# ---------- Seri port okuma (50 ms) ----------
$timer = New-Object System.Windows.Forms.Timer
$timer.Interval = 50
$timer.Add_Tick({
    try { $S.buf += $sp.ReadExisting() } catch { }
    $parts = $S.buf -split "`r`n"
    $S.buf = $parts[-1]
    for ($i = 0; $i -lt $parts.Length - 1; $i++) {
        $line = $parts[$i]
        $m = [regex]::Match($line, $rx)
        if ($m.Success) {
            $S.ch1 = [int]$m.Groups[1].Value; $S.ch2 = [int]$m.Groups[2].Value
            $S.ch3 = [int]$m.Groups[3].Value; $S.ch4 = [int]$m.Groups[4].Value
            $S.sol = [int]$m.Groups[5].Value; $S.sag = [int]$m.Groups[6].Value
            $S.fs  = [int]$m.Groups[7].Value; $S.lost = [int]$m.Groups[8].Value
            $S.pkt = $m.Groups[9].Value;      $S.err = $m.Groups[10].Value
            if ($m.Groups[13].Success) { $S.nl = [int]$m.Groups[13].Value; $S.nr = [int]$m.Groups[14].Value }
            if ($m.Groups[11].Success) {
                $S.raw1 = [int]$m.Groups[11].Value; $S.raw2 = [int]$m.Groups[12].Value
                if ($S.calOn) {
                    $S.min1 = [Math]::Min($S.min1, $S.raw1); $S.max1 = [Math]::Max($S.max1, $S.raw1)
                    $S.min2 = [Math]::Min($S.min2, $S.raw2); $S.max2 = [Math]::Max($S.max2, $S.raw2)
                }
            }
            $S.st  = if ($line -match '^\[\s*([^\s\]]+)') { $Matches[1] } else { '?' }
            if ($m.Groups[15].Success) { $S.sw = [int]$m.Groups[15].Value }
            $S.last = Get-Date
        }
    }
    if ($S.buf.Length -gt 4096) { $S.buf = '' }
    Update-CalTable
    Update-Board
    if ($chkTest.Checked) {
        try { $sp.Write(("T {0} {1}`n" -f $T.l, $T.r)) } catch { }
    }
    $pb.Invalidate()
})

$form.Add_FormClosing({ $timer.Stop(); try { $sp.Write("X`n"); $sp.Close() } catch { } })
$timer.Start()
[void]$form.ShowDialog()
