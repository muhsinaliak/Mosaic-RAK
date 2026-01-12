#!/usr/bin/env python3
"""
OTA Firmware Publisher - GitHub Release Otomatik Yayinlama Araci
Mosaic RAK Gateway icin tasarlandi
"""

import os
import sys
import json
import requests
import tkinter as tk
from tkinter import ttk, filedialog, messagebox, scrolledtext
from datetime import datetime
import threading

# GitHub Ayarlari
GITHUB_USER = "muhsinaliak"
GITHUB_REPO = "Mosaic-RAK"

# Uygulama dizini (exe veya script)
if getattr(sys, 'frozen', False):
    APP_DIR = os.path.dirname(sys.executable)
else:
    APP_DIR = os.path.dirname(os.path.abspath(__file__))

SETTINGS_FILE = os.path.join(APP_DIR, ".ota_publisher_settings.json")

class OTAPublisher:
    def __init__(self, root):
        self.root = root
        self.root.title("Mosaic RAK - OTA Publisher")
        self.root.geometry("600x700")
        self.root.resizable(True, True)

        # Degiskenler
        self.firmware_path = tk.StringVar()
        self.filesystem_path = tk.StringVar()
        self.version = tk.StringVar()
        self.github_token = tk.StringVar()
        self.min_version = tk.StringVar(value="1.0.0")

        self.create_widgets()
        self.load_settings()
        self.auto_detect_binaries()

    def create_widgets(self):
        # Ana frame
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.pack(fill=tk.BOTH, expand=True)

        # GitHub Token
        ttk.Label(main_frame, text="GitHub Token:").pack(anchor=tk.W)
        token_frame = ttk.Frame(main_frame)
        token_frame.pack(fill=tk.X, pady=(0, 10))
        self.token_entry = ttk.Entry(token_frame, textvariable=self.github_token, show="*", width=50)
        self.token_entry.pack(side=tk.LEFT, fill=tk.X, expand=True)
        ttk.Button(token_frame, text="Goster/Gizle", command=self.toggle_token).pack(side=tk.LEFT, padx=(5, 0))

        # Versiyon
        ttk.Label(main_frame, text="Yeni Versiyon (ornek: 7.2.0):").pack(anchor=tk.W)
        ttk.Entry(main_frame, textvariable=self.version, width=20).pack(anchor=tk.W, pady=(0, 10))

        # Minimum Versiyon
        ttk.Label(main_frame, text="Minimum Desteklenen Versiyon:").pack(anchor=tk.W)
        ttk.Entry(main_frame, textvariable=self.min_version, width=20).pack(anchor=tk.W, pady=(0, 10))

        # Firmware Dosyasi
        ttk.Label(main_frame, text="Firmware Dosyasi (.bin):").pack(anchor=tk.W)
        fw_frame = ttk.Frame(main_frame)
        fw_frame.pack(fill=tk.X, pady=(0, 5))
        ttk.Entry(fw_frame, textvariable=self.firmware_path, width=50).pack(side=tk.LEFT, fill=tk.X, expand=True)
        ttk.Button(fw_frame, text="Sec...", command=self.select_firmware).pack(side=tk.LEFT, padx=(5, 0))

        self.firmware_size_label = ttk.Label(main_frame, text="Boyut: -")
        self.firmware_size_label.pack(anchor=tk.W, pady=(0, 10))

        # Filesystem Dosyasi
        ttk.Label(main_frame, text="Filesystem Dosyasi (littlefs.bin):").pack(anchor=tk.W)
        fs_frame = ttk.Frame(main_frame)
        fs_frame.pack(fill=tk.X, pady=(0, 5))
        ttk.Entry(fs_frame, textvariable=self.filesystem_path, width=50).pack(side=tk.LEFT, fill=tk.X, expand=True)
        ttk.Button(fs_frame, text="Sec...", command=self.select_filesystem).pack(side=tk.LEFT, padx=(5, 0))

        self.filesystem_size_label = ttk.Label(main_frame, text="Boyut: -")
        self.filesystem_size_label.pack(anchor=tk.W, pady=(0, 10))

        # Degisiklik Notlari
        ttk.Label(main_frame, text="Degisiklik Notlari (Changelog):").pack(anchor=tk.W)
        self.changelog_text = scrolledtext.ScrolledText(main_frame, height=8, width=60)
        self.changelog_text.pack(fill=tk.X, pady=(0, 10))
        self.changelog_text.insert(tk.END, "## Degisiklikler\n- Yeni ozellik eklendi\n- Hata duzeltmeleri\n\n## Notlar\n- Guncelleme sonrasi cihaz otomatik yeniden baslatilacaktir")

        # Butonlar
        btn_frame = ttk.Frame(main_frame)
        btn_frame.pack(fill=tk.X, pady=10)

        ttk.Button(btn_frame, text="Onizleme", command=self.preview).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(btn_frame, text="GitHub'a Yayinla", command=self.publish).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(btn_frame, text="Ayarlari Kaydet", command=self.save_settings).pack(side=tk.RIGHT)

        # Log Alani
        ttk.Label(main_frame, text="Log:").pack(anchor=tk.W)
        self.log_text = scrolledtext.ScrolledText(main_frame, height=10, width=60, state=tk.DISABLED)
        self.log_text.pack(fill=tk.BOTH, expand=True)

        # Progress Bar
        self.progress = ttk.Progressbar(main_frame, mode='indeterminate')
        self.progress.pack(fill=tk.X, pady=(10, 0))

    def toggle_token(self):
        if self.token_entry.cget('show') == '*':
            self.token_entry.config(show='')
        else:
            self.token_entry.config(show='*')

    def select_firmware(self):
        path = filedialog.askopenfilename(
            title="Firmware Dosyasi Sec",
            filetypes=[("Binary files", "*.bin"), ("All files", "*.*")],
            initialdir=os.path.dirname(self.firmware_path.get()) if self.firmware_path.get() else None
        )
        if path:
            self.firmware_path.set(path)
            size = os.path.getsize(path)
            self.firmware_size_label.config(text=f"Boyut: {size:,} byte ({size/1024:.1f} KB)")

    def select_filesystem(self):
        path = filedialog.askopenfilename(
            title="Filesystem Dosyasi Sec",
            filetypes=[("Binary files", "*.bin"), ("All files", "*.*")],
            initialdir=os.path.dirname(self.filesystem_path.get()) if self.filesystem_path.get() else None
        )
        if path:
            self.filesystem_path.set(path)
            size = os.path.getsize(path)
            self.filesystem_size_label.config(text=f"Boyut: {size:,} byte ({size/1024:.1f} KB)")

    def log(self, message):
        self.log_text.config(state=tk.NORMAL)
        self.log_text.insert(tk.END, f"{datetime.now().strftime('%H:%M:%S')} - {message}\n")
        self.log_text.see(tk.END)
        self.log_text.config(state=tk.DISABLED)
        self.root.update()

    def validate(self):
        if not self.github_token.get():
            messagebox.showerror("Hata", "GitHub Token gerekli!")
            return False
        if not self.version.get():
            messagebox.showerror("Hata", "Versiyon numarasi gerekli!")
            return False
        if not self.firmware_path.get() or not os.path.exists(self.firmware_path.get()):
            messagebox.showerror("Hata", "Gecerli bir firmware dosyasi secin!")
            return False
        if not self.filesystem_path.get() or not os.path.exists(self.filesystem_path.get()):
            messagebox.showerror("Hata", "Gecerli bir filesystem dosyasi secin!")
            return False
        return True

    def get_manifest(self):
        version = self.version.get()
        if not version.startswith('v'):
            tag = f"v{version}"
        else:
            tag = version
            version = version[1:]

        changelog_lines = self.changelog_text.get("1.0", tk.END).strip().split('\n')
        changelog = f"v{version} Degisiklikler:\n" + "\n".join(changelog_lines)

        return {
            "version": version,
            "firmware_url": f"https://github.com/{GITHUB_USER}/{GITHUB_REPO}/releases/download/{tag}/firmware.bin",
            "filesystem_url": f"https://github.com/{GITHUB_USER}/{GITHUB_REPO}/releases/download/{tag}/littlefs.bin",
            "firmware_size": os.path.getsize(self.firmware_path.get()),
            "filesystem_size": os.path.getsize(self.filesystem_path.get()),
            "changelog": changelog,
            "release_date": datetime.now().strftime("%Y-%m-%d"),
            "min_version": self.min_version.get()
        }, tag

    def preview(self):
        if not self.validate():
            return

        manifest, tag = self.get_manifest()
        preview_text = json.dumps(manifest, indent=2, ensure_ascii=False)

        preview_window = tk.Toplevel(self.root)
        preview_window.title("manifest.json Onizleme")
        preview_window.geometry("500x400")

        text = scrolledtext.ScrolledText(preview_window, width=60, height=20)
        text.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        text.insert(tk.END, preview_text)
        text.config(state=tk.DISABLED)

    def publish(self):
        if not self.validate():
            return

        if not messagebox.askyesno("Onay", "GitHub'a yayinlamak istediginizden emin misiniz?"):
            return

        # Arkaplanda calistir
        self.progress.start()
        thread = threading.Thread(target=self._publish_thread)
        thread.start()

    def _publish_thread(self):
        try:
            manifest, tag = self.get_manifest()
            token = self.github_token.get()
            headers = {
                "Authorization": f"token {token}",
                "Accept": "application/vnd.github.v3+json"
            }

            # 1. Release olustur
            self.log(f"Release olusturuluyor: {tag}")
            release_data = {
                "tag_name": tag,
                "name": f"Release {tag}",
                "body": manifest["changelog"],
                "draft": False,
                "prerelease": False
            }

            response = requests.post(
                f"https://api.github.com/repos/{GITHUB_USER}/{GITHUB_REPO}/releases",
                headers=headers,
                json=release_data
            )

            if response.status_code == 201:
                release = response.json()
                upload_url = release["upload_url"].replace("{?name,label}", "")
                self.log(f"Release olusturuldu: {release['html_url']}")
            elif response.status_code == 422:
                # 422 olabilecek durumlar: tag zaten var VEYA validasyon hatasi
                error_info = response.json()
                self.log(f"422 Detay: {error_info}")

                # Tag/release zaten var mi kontrol et
                self.log("Mevcut release araniyor...")
                response = requests.get(
                    f"https://api.github.com/repos/{GITHUB_USER}/{GITHUB_REPO}/releases/tags/{tag}",
                    headers=headers
                )
                if response.status_code == 200:
                    release = response.json()
                    upload_url = release["upload_url"].replace("{?name,label}", "")
                    self.log(f"Mevcut release bulundu: {release['html_url']}")
                else:
                    # Release yok, tag olusturup tekrar dene
                    self.log("Release bulunamadi, yeni release olusturuluyor...")

                    # Oncelikle tum release'leri kontrol et
                    response = requests.get(
                        f"https://api.github.com/repos/{GITHUB_USER}/{GITHUB_REPO}/releases",
                        headers=headers
                    )
                    self.log(f"Mevcut release sayisi: {len(response.json()) if response.status_code == 200 else 'Bilinmiyor'}")

                    raise Exception(f"Release olusturulamadi. 422 hatasi: {error_info}")
            else:
                raise Exception(f"Release olusturulamadi ({response.status_code}): {response.text}")

            # 2. Firmware yukle
            self.log("Firmware yukleniyor...")
            with open(self.firmware_path.get(), "rb") as f:
                firmware_data = f.read()

            upload_headers = headers.copy()
            upload_headers["Content-Type"] = "application/octet-stream"

            response = requests.post(
                f"{upload_url}?name=firmware.bin",
                headers=upload_headers,
                data=firmware_data
            )

            if response.status_code in [201, 422]:  # 422 = zaten var
                self.log("Firmware yuklendi!")
            else:
                self.log(f"Firmware yukleme uyarisi: {response.status_code}")

            # 3. Filesystem yukle
            self.log("Filesystem yukleniyor...")
            with open(self.filesystem_path.get(), "rb") as f:
                fs_data = f.read()

            response = requests.post(
                f"{upload_url}?name=littlefs.bin",
                headers=upload_headers,
                data=fs_data
            )

            if response.status_code in [201, 422]:
                self.log("Filesystem yuklendi!")
            else:
                self.log(f"Filesystem yukleme uyarisi: {response.status_code}")

            self.log("=" * 40)
            self.log(f"BASARILI! {tag} yayinlandi!")
            self.log(f"Release: https://github.com/{GITHUB_USER}/{GITHUB_REPO}/releases/tag/{tag}")

            self.root.after(0, lambda: messagebox.showinfo("Basarili", f"{tag} basariyla yayinlandi!"))

        except Exception as e:
            self.log(f"HATA: {str(e)}")
            self.root.after(0, lambda: messagebox.showerror("Hata", str(e)))
        finally:
            self.root.after(0, self.progress.stop)

    def save_settings(self):
        settings = {
            "github_token": self.github_token.get(),
            "last_firmware_dir": os.path.dirname(self.firmware_path.get()) if self.firmware_path.get() else "",
            "min_version": self.min_version.get()
        }
        with open(SETTINGS_FILE, "w") as f:
            json.dump(settings, f)
        self.log(f"Ayarlar kaydedildi: {SETTINGS_FILE}")

    def load_settings(self):
        if os.path.exists(SETTINGS_FILE):
            try:
                with open(SETTINGS_FILE, "r") as f:
                    settings = json.load(f)
                self.github_token.set(settings.get("github_token", ""))
                self.min_version.set(settings.get("min_version", "1.0.0"))
            except:
                pass

    def auto_detect_binaries(self):
        """Otomatik olarak .pio/build/gateway dizinindeki bin dosyalarini bul"""
        build_dir = os.path.join(APP_DIR, ".pio", "build", "gateway")

        firmware_path = os.path.join(build_dir, "firmware.bin")
        if os.path.exists(firmware_path) and not self.firmware_path.get():
            self.firmware_path.set(firmware_path)
            size = os.path.getsize(firmware_path)
            self.firmware_size_label.config(text=f"Boyut: {size:,} byte ({size/1024:.1f} KB)")
            self.log(f"Firmware otomatik bulundu: {firmware_path}")

        fs_path = os.path.join(build_dir, "littlefs.bin")
        if os.path.exists(fs_path) and not self.filesystem_path.get():
            self.filesystem_path.set(fs_path)
            size = os.path.getsize(fs_path)
            self.filesystem_size_label.config(text=f"Boyut: {size:,} byte ({size/1024:.1f} KB)")

def main():
    root = tk.Tk()
    app = OTAPublisher(root)
    root.mainloop()

if __name__ == "__main__":
    main()
