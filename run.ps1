Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# 1. เช็คสิทธิ์ Admin
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Start-Process powershell -ArgumentList "-NoProfile -ExecutionPolicy Bypass -Command `"$((New-Object System.Net.WebClient).DownloadString('https://raw.githubusercontent.com/chelseaZZZ1/boostfps/main/run.ps1'))`"" -Verb RunAs
    exit
}

# 2. ฟังก์ชั่น Boost FPS & Clear Cache
function Start-Boost {
    $cleared = 0
    $fivemCache = "$env:LOCALAPPDATA\FiveM\FiveM.app\data"
    $folders = @('cache', 'server-cache', 'server-cache-priv', 'nui-storage')
    
    if (Test-Path $fivemCache) {
        foreach ($f in $folders) {$path = Join-Path $fivemCache$f
            if (Test-Path $path) {
                Remove-Item -Path $path -Recurse -Force -ErrorAction SilentlyContinue$cleared++
            }
        }
    }
    
    Set-ItemProperty -Path "HKCU:\System\GameConfigStore" -Name "GameDVR_Enabled" -Value 0 -ErrorAction SilentlyContinue
    Set-ItemProperty -Path "HKLM:\SOFTWARE\Policies\Microsoft\Windows\GameDVR" -Name "AllowGameDVR" -Value 0 -ErrorAction SilentlyContinue

    return "Cleaned $cleared FiveM cache folders and Disabled GameDVR!"
}

# 3. ฟังก์ชั่น Lock High Priority
function Start-PriorityLock {
    $cmd = {
        while ($true) {
            Get-Process -Name "FiveM*", "GTA5*" -ErrorAction SilentlyContinue | ForEach-Object {
                $_.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::High
            }
            Start-Sleep -Seconds 5
        }
    }
    [scriptblock]::Create($cmd).BeginInvoke()
    return "Process Priority Lock Activated (High Priority)!"
}

# 4. สร้าง GUI
$form = New-Object System.Windows.Forms.Form
$form.Text = "FiveM Ultra FPS Booster"
$form.Size = New-Object System.Drawing.Size(460, 520)$form.StartPosition = "CenterScreen"
$form.BackColor = [System.Drawing.ColorTranslator]::FromHtml("#090a0f")
$form.FormBorderStyle = "FixedDialog"
$form.MaximizeBox =$false

# Banner Image
$pictureBox = New-Object System.Windows.Forms.PictureBox
$pictureBox.Size = New-Object System.Drawing.Size(400, 130)
$pictureBox.Location = New-Object System.Drawing.Point(22, 20)$pictureBox.SizeMode = "StretchImage"
try {
    $imgUrl = "https://images.unsplash.com/photo-1542751371-adc38448a05e?w=600&q=80"
    $req = [System.Net.WebRequest]::Create($imgUrl)
    $resp =$req.GetResponse()
    $pictureBox.Image = [System.Drawing.Image]::FromStream($resp.GetResponseStream())
} catch {
    $bmp = New-Object System.Drawing.Bitmap(400, 130)$g = [System.Drawing.Graphics]::FromImage($bmp)$g.Clear([System.Drawing.ColorTranslator]::FromHtml("#10121b"))
    $font = New-Object System.Drawing.Font("Arial", 16, [System.Drawing.FontStyle]::Bold)
    $brush = New-Object System.Drawing.SolidBrush([System.Drawing.ColorTranslator]::FromHtml("#00f0ff"))
    $g.DrawString("FIVEM ULTRA BOOST", $font,$brush, 80, 50)
    $pictureBox.Image =$bmp
}
$form.Controls.Add($pictureBox)

# Title
$title = New-Object System.Windows.Forms.Label
$title.Text = "FIVEM FPS BOOST VIP"
$title.Font = New-Object System.Drawing.Font("Arial", 14, [System.Drawing.FontStyle]::Bold)
$title.ForeColor = [System.Drawing.ColorTranslator]::FromHtml("#00f0ff")
$title.Size = New-Object System.Drawing.Size(400, 30)
$title.Location = New-Object System.Drawing.Point(22, 165)$title.TextAlign = "MiddleCenter"
$form.Controls.Add($title)

# Console Output
$console = New-Object System.Windows.Forms.TextBox
$console.Multiline = $true$console.ReadOnly = $true$console.Size = New-Object System.Drawing.Size(400, 90)
$console.Location = New-Object System.Drawing.Point(22, 360)$console.BackColor = [System.Drawing.ColorTranslator]::FromHtml("#050608")
$console.ForeColor = [System.Drawing.ColorTranslator]::FromHtml("#00ff66")
$console.Font = New-Object System.Drawing.Font("Consolas", 9)
$console.Text = "> Ready to boost your FiveM..."
$form.Controls.Add($console)

# Buttons
$btnBoost = New-Object System.Windows.Forms.Button
$btnBoost.Text = "BOOST FPS AND CLEAR CACHE"
$btnBoost.Size = New-Object System.Drawing.Size(400, 45)
$btnBoost.Location = New-Object System.Drawing.Point(22, 210)$btnBoost.FlatStyle = "Flat"
$btnBoost.BackColor = [System.Drawing.ColorTranslator]::FromHtml("#7000ff")
$btnBoost.ForeColor = [System.Drawing.Color]::White$btnBoost.Font = New-Object System.Drawing.Font("Arial", 10, [System.Drawing.FontStyle]::Bold)
$btnBoost.Add_Click({$console.Text = "> Running FPS Boost..."
    $res = Start-Boost
    $console.Text =$res
})
$form.Controls.Add($btnBoost)

$btnPriority = New-Object System.Windows.Forms.Button
$btnPriority.Text = "LOCK PROCESS PRIORITY (HIGH)"
$btnPriority.Size = New-Object System.Drawing.Size(400, 45)
$btnPriority.Location = New-Object System.Drawing.Point(22, 270)$btnPriority.FlatStyle = "Flat"
$btnPriority.BackColor = [System.Drawing.ColorTranslator]::FromHtml("#ff0055")
$btnPriority.ForeColor = [System.Drawing.Color]::White$btnPriority.Font = New-Object System.Drawing.Font("Arial", 10, [System.Drawing.FontStyle]::Bold)
$btnPriority.Add_Click({$console.Text = "> Activating Priority Lock..."
    $res = Start-PriorityLock
    $console.Text =$res
})
$form.Controls.Add($btnPriority)

[System.Windows.Forms.Application]::Run($form)
