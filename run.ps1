# เช็ค Python Environment
if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    Write-Host "❌ Python is not installed! Please install Python first." -ForegroundColor Red
    exit
}

# Install PyWebView
Write-Host "📦 Checking & Installing Required Packages..." -ForegroundColor Cyan
pip install pywebview --quiet

# Run Application with Admin Privilege
Write-Host "🚀 Launching FiveM FPS Booster..." -ForegroundColor Green
Start-Process python -ArgumentList "main.py" -Verb RunAs
