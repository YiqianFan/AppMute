# -*- mode: python ; coding: utf-8 -*-
block_cipher = None

a = Analysis(
    ['src/main.py'],
    pathex=[],
    binaries=[],
    datas=[('config.toml', '.')],
    hiddenimports=['comtypes', 'pycaw', 'keyboard', 'psutil', 'pystray', 'PIL', 'toml'],
    hookspath=[],
    hooksconfig={},
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=False,
    name='AppMute',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=False,
    disable_windowed_traceback=False,
    onefile=True,
)
