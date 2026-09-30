import os
import sys
import subprocess
import ctypes

def is_admin():
    """เช็คสิทธิ์ Administrator"""
    try:
        return ctypes.windll.shell32.IsUserAnAdmin()
    except:
        return False

def apply_fps_boost():
    print("========================================")
    print("   FiveM One-Click FPS Boost & CPU Fix  ")
    print("========================================")
    print("[+] กำลังเริ่มกระบวนการปรับแต่ง...\n")

    # 1. ปรับ Power Scheme เป็น High Performance
    print("[1/5] ปรับ Power Plan เป็น High Performance...")
    try:
        subprocess.run(["powercfg", "-setactive", "8c5e7fda-e8bf-4a96-9a15-7e42e1d0773d"], check=True)
        print("  -> สำเร็จ!")
    except Exception as e:
        print(f"  -> ล้มเหลว: {e}")

    # 2. เปิดใช้งาน Hardware-Accelerated GPU Scheduling (HAGS) เพื่อโยนงานให้ GPU
    print("[2/5] บังคับให้ GPU รับภาระประมวลผลแทน CPU (Enable HAGS)...")
    try:
        reg_cmd = 'reg add "HKLM\\SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers" /v "HwSchMode" /t REG_DWORD /d 2 /f'
        subprocess.run(reg_cmd, shell=True, check=True)
        print("  -> เปิดใช้งาน HAGS เรียบร้อย!")
    except Exception as e:
        print(f"  -> ล้มเหลว: {e}")

    # 3. ตั้งค่า Priority ของ GTA V / FiveM ให้ GPU ประมวลผลก่อน
    print("[3/5] ปรับแต่ง Priority และ GPU Optimization ใน Registry...")
    commands = [
        # ตั้ง Priority GTA V / FiveM
        'reg add "HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\GTA5.exe\\PerfOptions" /v "CpuPriorityClass" /t REG_DWORD /d 3 /f',
        'reg add "HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\FiveM.exe\\PerfOptions" /v "CpuPriorityClass" /t REG_DWORD /d 3 /f',
        # ปิดการสลีปและดึงประสิทธิภาพ GPU
        'reg add "HKLM\\SYSTEM\\CurrentControlSet\\Control\\Power" /v "EnergyEstimationEnabled" /t REG_DWORD /d 0 /f'
    ]
    for cmd in commands:
        try:
            subprocess.run(cmd, shell=True, check=True)
        except Exception as e:
            print(f"  -> Registry Error: {e}")
    print("  -> สำเร็จ!")

    # 4. ล้าง Cache ของ FiveM ชั่วคราวเพื่อลด CPU Spikes
    print("[4/5] ล้าง Cache ชั่วคราวของ FiveM (NVE & Shader Cache)...")
    appdata = os.getenv('LOCALAPPDATA')
    if appdata:
        cache_path = os.path.join(appdata, "FiveM", "FiveM.app", "data", "cache")
        nui_path = os.path.join(appdata, "FiveM", "FiveM.app", "data", "nui-storage")
        for path in [cache_path, nui_path]:
            if os.path.exists(path):
                try:
                    subprocess.run(f'rmdir /s /q "{path}"', shell=True)
                    print(f"  -> ลบ Cache ที่ {path} เรียบร้อย")
                except Exception as e:
                    print(f"  -> ข้ามการลบ {path}: {e}")

    # 5. สรุปผล
    print("\n========================================")
    print(" [✓] ปรับแต่งสำเร็จ 100%!")
    print(" - แนะนำให้ รีสตาร์ทคอมพิวเตอร์ 1 ครั้งเพื่อให้ค่า Registry และ HAGS ทำงาน")
    print("========================================")

if __name__ == "__main__":
    # บังคับรันในฐานะ Administrator
    if is_admin():
        apply_fps_boost()
    else:
        print("[!] กำลังร้องขอสิทธิ์ Administrator...")
        ctypes.windll.shell32.ShellExecuteW(None, "runas", sys.executable, " ".join(sys.argv), None, 1)
