"""Graphical configuration editor for AppMute."""
import tkinter as tk
from tkinter import ttk, messagebox
import toml
from pathlib import Path


class ConfigGUI:
    def __init__(self, config_path: str = "config.toml"):
        self.config_path = config_path
        self.rows: list = []

        self.root = tk.Tk()
        self.root.title("AppMute 配置")
        self.root.geometry("540x460")
        self.root.resizable(False, False)
        self.root.configure(bg="#f0f0f0")
        self.root.attributes("-topmost", True)

        tk.Label(self.root, text="AppMute 快捷键配置",
                 font=("Microsoft YaHei", 16, "bold"), bg="#f0f0f0", fg="#333").pack(pady=(20, 5))
        tk.Label(self.root, text="点击快捷键按钮，再按组合键即可设置",
                 font=("Microsoft YaHei", 9), bg="#f0f0f0", fg="#888").pack(pady=(0, 12))

        hdr = tk.Frame(self.root, bg="#e0e0e0")
        hdr.pack(fill="x", padx=20)
        tk.Label(hdr, text="快捷键", font=("Microsoft YaHei", 10, "bold"),
                 width=14, bg="#e0e0e0").pack(side="left", padx=(10, 5), pady=6)
        tk.Label(hdr, text="应用名（进程名）", font=("Microsoft YaHei", 10, "bold"),
                 width=20, bg="#e0e0e0").pack(side="left", padx=5, pady=6)
        tk.Label(hdr, text="操作", font=("Microsoft YaHei", 10, "bold"),
                 width=10, bg="#e0e0e0").pack(side="left", padx=5, pady=6)

        cf = tk.Frame(self.root, bg="#f0f0f0")
        cf.pack(fill="both", expand=True, padx=20, pady=(0, 5))
        cnv = tk.Canvas(cf, bg="#f0f0f0", highlightthickness=0, height=220)
        scr = ttk.Scrollbar(cf, orient="vertical", command=cnv.yview)
        self.list_fr = tk.Frame(cnv, bg="#f0f0f0")
        self.list_fr.bind("<Configure>",
                          lambda _: cnv.configure(scrollregion=cnv.bbox("all")))
        cnv.create_window((0, 0), window=self.list_fr, anchor="nw")
        cnv.configure(yscrollcommand=scr.set)
        cnv.pack(side="left", fill="both", expand=True)
        scr.pack(side="right", fill="y")

        ifr = tk.Frame(self.root, bg="#f0f0f0")
        ifr.pack(fill="x", padx=20, pady=(5, 8))
        tk.Label(ifr, text="检测间隔 (ms):", font=("Microsoft YaHei", 9), bg="#f0f0f0").pack(side="left")
        self.interval_val = tk.StringVar(value="1000")
        tk.Entry(ifr, textvariable=self.interval_val, width=10,
                 font=("Microsoft YaHei", 9)).pack(side="left", padx=5)

        bfr = tk.Frame(self.root, bg="#f0f0f0")
        bfr.pack(pady=(8, 15))
        tk.Button(bfr, text="保存", command=self._save,
                  font=("Microsoft YaHei", 11, "bold"), bg="#2196F3", fg="white",
                  relief="flat", cursor="hand2", width=10).pack(side="left", padx=10)
        tk.Button(bfr, text="取消", command=self.root.destroy,
                  font=("Microsoft YaHei", 11), bg="#999", fg="white",
                  relief="flat", cursor="hand2", width=10).pack(side="left", padx=10)
        tk.Button(bfr, text="+ 添加", command=self._add_row,
                  font=("Microsoft YaHei", 10), bg="#4CAF50", fg="white",
                  relief="flat", cursor="hand2", width=8).pack(side="left", padx=10)

        self.root.protocol("WM_DELETE_WINDOW", self.root.destroy)
        self._load()
        self.root.mainloop()

    def _load(self):
        p = Path(self.config_path)
        data = {"hotkeys": {}, "general": {"check_interval": 1000}}
        if p.exists():
            try:
                data = toml.load(p)
            except Exception:
                pass
        self.interval_val.set(str(data.get("general", {}).get("check_interval", 1000)))
        for hk, app in data.get("hotkeys", {}).items():
            self._add_row(hk, app)
        if not data.get("hotkeys"):
            self._add_row()

    def _add_row(self, hotkey: str = "", app: str = ""):
        fr = tk.Frame(self.list_fr, bg="#f0f0f0")
        fr.pack(fill="x", pady=2)

        hk_btn = tk.Button(fr, text=hotkey if hotkey else "（点击设置）",
                           font=("Consolas", 9), bg="#e3f2fd", fg="#1565C0",
                           relief="raised", cursor="hand2", width=14, anchor="w")
        hk_btn.pack(side="left", padx=(5, 10))

        app_ent = tk.Entry(fr, width=22, font=("Microsoft YaHei", 10))
        app_ent.insert(0, app)
        app_ent.pack(side="left", padx=5)

        def delete():
            self.rows.remove((hk_btn, app_ent))
            fr.destroy()

        tk.Button(fr, text="×", command=delete,
                  font=("Microsoft YaHei", 12, "bold"), bg="#f44336", fg="white",
                  relief="flat", cursor="hand2", width=3).pack(side="left", padx=5)

        self.rows.append((hk_btn, app_ent))

        hk_btn._capturing = False
        hk_btn._keys_down = []

        def on_btn_click(_):
            self._start_capture(hk_btn)

        def on_key_down(e):
            if not hk_btn._capturing:
                return
            # Track modifiers
            if e.keysym in ("Shift_L", "Shift_R", "Control_L", "Control_R",
                             "Alt_L", "Alt_R", "Meta_L", "Meta_R"):
                return
            hk_btn._keys_down.append(e.keysym)

        def on_key_up(e):
            if not hk_btn._capturing:
                return
            if e.keysym in ("Shift_L", "Shift_R", "Control_L", "Control_R",
                             "Alt_L", "Alt_R", "Meta_L", "Meta_R"):
                return
            combo = self._build_combo(hk_btn)
            if combo:
                hk_btn.configure(text=combo, bg="#e3f2fd", fg="#1565C0")
            hk_btn._capturing = False
            hk_btn._keys_down.clear()

        def on_focus_out(_):
            if hk_btn._capturing:
                hk_btn.configure(text="（点击设置）", bg="#e3f2fd", fg="#1565C0")
                hk_btn._capturing = False
                hk_btn._keys_down.clear()

        hk_btn.bind("<Button-1>", on_btn_click)
        hk_btn.bind("<KeyPress>", on_key_down)
        hk_btn.bind("<KeyRelease>", on_key_up)
        hk_btn.bind("<FocusOut>", on_focus_out)
        hk_btn.configure(takefocus=True)

    def _start_capture(self, btn):
        for b, _ in self.rows:
            if b._capturing and b is not btn:
                b.configure(text="（点击设置）", bg="#e3f2fd", fg="#1565C0")
                b._capturing = False
                b._keys_down.clear()
        btn._capturing = True
        btn._keys_down = []
        btn.configure(text="（等待按键...）", bg="#90CAF9", fg="white")
        btn.focus_set()

    def _build_combo(self, btn) -> str:
        parts = []
        for key in btn._keys_down:
            # Get modifiers from event state
            pass
        # Use _keys_down directly: each key in the list
        keys = btn._keys_down
        parts = []

        def mod_of(keysym):
            m = {"Shift": "shift", "Control": "ctrl", "Alt": "alt", "Meta": "win"}
            for k, v in m.items():
                if k in keysym:
                    return v
            return None

        for key in keys:
            mn = mod_of(key)
            if mn and mn not in parts:
                parts.append(mn)

        # Last key is the main key
        if keys:
            main_key = keys[-1]
            mn = mod_of(main_key)
            if mn:
                parts.append(main_key.replace("Control_", "").replace("Alt_", "").replace("Shift_", "").replace("Meta_", "").lower())
            else:
                parts.append(main_key.lower() if len(main_key) == 1 else main_key)

        # Remove modifier from main key position if it's already the last
        seen_mods = set()
        final_parts = []
        for p in parts:
            mn = mod_of(p) if not isinstance(p, str) else None
            is_mod = p in ("ctrl", "alt", "shift", "win")
            if is_mod:
                seen_mods.add(p)
                final_parts.append(p)
            elif p not in seen_mods:
                final_parts.append(p)

        return "+".join(final_parts)

    def _save(self):
        hotkeys = {}
        for hk_btn, app_ent in self.rows:
            hk = hk_btn.cget("text").strip()
            app = app_ent.get().strip()
            if hk and app and hk not in ("（点击设置）", "（等待按键...）"):
                hotkeys[hk.lower()] = app.lower()

        try:
            interval = int(self.interval_val.get())
        except ValueError:
            messagebox.showerror("错误", "检测间隔必须是整数")
            return

        try:
            data_str = toml.dumps({"hotkeys": hotkeys, "general": {"check_interval": interval}})
            with open(self.config_path, "w", encoding="utf-8") as f:
                f.write(data_str)
            messagebox.showinfo("成功", "配置已保存")
            self.root.destroy()
        except Exception as e:
            messagebox.showerror("错误", f"保存失败: {e}")


def main():
    ConfigGUI()


if __name__ == "__main__":
    main()
