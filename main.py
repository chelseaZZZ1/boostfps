import os
import sys
import ctypes
import shutil
import subprocess
import threading
import time
import webview

# เช็คสิทธิ์ Administrator (จำเป็นสำหรับการปรับ Process Priority)
def is_admin():
    try:
        return ctypes.windll.shell32.IsUserAnAdmin()
    except:
        return False

class FiveMBoosterAPI:
    def __init__(self):
        self._window = None

    def set_window(self, window):
        self._window = window

    def boost_fps(self):
        """ล้างแคช FiveM และปรับแต่ง Windows เพื่อ Boost FPS"""
        logs = []
        
        # 1. Clear FiveM Cache
        appdata = os.getenv('LOCALAPPDATA')
        fivem_cache = os.path.join(appdata, 'FiveM', 'FiveM.app', 'data')
        
        cache_folders = ['cache', 'server-cache', 'server-cache-priv', 'nui-storage']
        cleared_count = 0
        
        if os.path.exists(fivem_cache):
            for folder in cache_folders:
                target = os.path.join(fivem_cache, folder)
                if os.path.exists(target):
                    try:
                        shutil.rmtree(target)
                        cleared_count += 1
                    except Exception as e:
                        pass
            logs.append(f"⚡ Cleaned {cleared_count} FiveM cache folders!")
        else:
            logs.append("⚠️ FiveM cache path not found (Default path checked)")

        # 2. Disable Game DVR (Windows Graphic Tweaks)
        try:
            subprocess.run('reg add "HKCU\\System\\GameConfigStore" /v "GameDVR_Enabled" /t REG_DWORD /d 0 /f', shell=True, capture_output=True)
            subprocess.run('reg add "HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\GameDVR" /v "AllowGameDVR" /t REG_DWORD /d 0 /f', shell=True, capture_output=True)
            logs.append("🚀 GameDVR & Windows Gaming Overlay Disabled!")
        except Exception as e:
            logs.append("⚠️ Failed to adjust GameDVR settings")

        return {"status": "success", "message": "\n".join(logs)}

    def start_priority_lock(self):
        """ลูปเบื้องหลังเพื่อตรวจจับและปรับ Priority ของ FiveM/GTA5 ให้เป็น High"""
        def lock_loop():
            cmd = '''powershell -Command "Get-WmiObject Win32_Process | Where-Object { $_.Name -match 'FiveM|GTA5' } | ForEach-Object { $_.SetPriority(128) } "'''
            while True:
                try:
                    subprocess.run(cmd, shell=True, capture_output=True)
                except:
                    pass
                time.sleep(5)

        thread = threading.Thread(target=lock_loop, daemon=True)
        thread.start()
        return {"status": "success", "message": "🔥 Process Priority Lock Activated (High Priority)!"}

# HTML/CSS/JS UI Design (Dark / Cyberpunk Neon)
HTML_CONTENT = """
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>FiveM Ultra FPS Booster</title>
    <link href="https://fonts.googleapis.com/css2?family=Orbitron:wght@600;900&family=Rajdhani:wght@500;700&display=swap" rel="stylesheet">
    <style>
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            user-select: none;
        }
        body {
            background: #090a0f;
            color: #fff;
            font-family: 'Rajdhani', sans-serif;
            height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            overflow: hidden;
            border: 2px solid #00f0ff33;
        }
        .card {
            background: rgba(16, 18, 27, 0.85);
            backdrop-filter: blur(12px);
            border: 1px solid rgba(0, 240, 255, 0.2);
            border-radius: 16px;
            padding: 30px;
            width: 440px;
            text-align: center;
            box-shadow: 0 0 30px rgba(0, 240, 255, 0.15);
        }
        .banner-img {
            width: 100%;
            height: 140px;
            object-fit: cover;
            border-radius: 10px;
            margin-bottom: 20px;
            border: 1px solid #00f0ff55;
            box-shadow: 0 0 15px rgba(0, 240, 255, 0.2);
        }
        h1 {
            font-family: 'Orbitron', sans-serif;
            font-size: 22px;
            color: #00f0ff;
            text-shadow: 0 0 10px rgba(0, 240, 255, 0.6);
            margin-bottom: 20px;
            letter-spacing: 1px;
        }
        .btn {
            background: linear-gradient(135deg, #00f0ff 0%, #7000ff 100%);
            border: none;
            color: #fff;
            padding: 14px 28px;
            font-family: 'Orbitron', sans-serif;
            font-size: 14px;
            font-weight: 900;
            border-radius: 8px;
            cursor: pointer;
            width: 100%;
            margin-bottom: 12px;
            transition: all 0.3s ease;
            box-shadow: 0 0 15px rgba(0, 240, 255, 0.4);
        }
        .btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 0 25px rgba(0, 240, 255, 0.8);
        }
        .btn:active {
            transform: translateY(1px);
        }
        .btn-secondary {
            background: linear-gradient(135deg, #ff0055 0%, #7000ff 100%);
            box-shadow: 0 0 15px rgba(ff, 0, 85, 0.4);
        }
        .btn-secondary:hover {
            box-shadow: 0 0 25px rgba(255, 0, 85, 0.8);
        }
        #console {
            background: #050608;
            border: 1px solid #1f2430;
            border-radius: 8px;
            padding: 12px;
            font-family: monospace;
            font-size: 12px;
            color: #00ff66;
            height: 90px;
            overflow-y: auto;
            text-align: left;
            margin-top: 15px;
        }
    </style>
</head>
<body>
    <div class="card">
        <!-- ภาพพร้อมระบุ onerror ป้องกันปัญหาภาพ undefined -->
        <img id="heroImg" src="https://images.unsplash.com/photo-1542751371-adc38448a05e?w=600&q=80" 
             alt="Header" class="banner-img" 
             onerror="this.onerror=null; this.src='data:image/svg+xml;utf8,<svg xmlns=\'http://www.w3.org/2000/svg\' width=\'100%\' height=\'100%\' viewBox=\'0 0 600 200\'><rect width=\'100%\' height=\'100%\' fill=\'%230f172a\'/><text x=\'50%\' y=\'50%\' fill=\'%2300f0ff\' font-family=\'sans-serif\' font-size=\'24\' text-anchor=\'middle\'>FIVEM ULTRA BOOST</text></svg>';" />
        
        <h1>FIVEM FPS BOOST VIP</h1>
        
        <button class="btn" onclick="runBoost()">⚡ BOOST FPS & CLEAR CACHE</button>
        <button class="btn btn-secondary" onclick="lockPriority()">🔒 LOCK PROCESS PRIORITY (HIGH)</button>
        
        <div id="console">> Ready to boost your FiveM...</div>
    </div>

    <script>
        function log(msg) {
            const consoleBox = document.getElementById('console');
            consoleBox.innerText = msg;
        }

        function runBoost() {
            log("> Running FPS Boost & Cleaning Cache...");
            pywebview.api.boost_fps().then(res => {
                log(res.message);
            });
        }

        function lockPriority() {
            log("> Activating Process Priority Lock...");
            pywebview.api.start_priority_lock().then(res => {
                log(res.message);
            });
        }
    </script>
</body>
</html>
"""

def main():
    if not is_admin():
        # Re-run Script เป็น Administrator ถ้ายังไม่มีสิทธิ์
        ctypes.windll.shell32.ShellExecuteW(None, "runas", sys.executable, " ".join(sys.argv), None, 1)
        sys.exit()

    api = FiveMBoosterAPI()
    window = webview.create_window(
        'FiveM Ultra FPS Booster',
        html=HTML_CONTENT,
        width=480,
        height=580,
        resizable=False,
        frameless=False,
        easy_drag=True
    )
    api.set_window(window)
    webview.start(debug=False)

if __name__ == '__main__':
    main()
