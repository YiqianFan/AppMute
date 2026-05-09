"""Generate tray icon using Windows API via ctypes - no PIL needed."""
import ctypes
from ctypes import wintypes
import struct


def create_speaker_icon() -> bytes:
    """Create a 32x32 speaker icon as BMP data, returned as bytes."""
    # Icon info header
    biWidth = 32
    biHeight = 32
    biBitCount = 32  # RGBA

    # BITMAPINFOHEADER = 40 bytes
    buf = struct.pack('<IiiHHIIIIII',
        40,          # biSize
        32,          # biWidth
        64,          # biHeight (doubled for AND mask)
        1,           # biPlanes
        32,          # biBitCount
        0,           # biCompression (BI_RGB)
        0,           # biSizeImage (can be 0 for uncompressed)
        0,           # biXPelsPerMeter
        0,           # biYPelsPerMeter
        0,           # biClrUsed
        0,          # biClrImportant
    )

    # Pixel data: 32x32 BGRA (bottom-up)
    # Background: transparent (0x00000000)
    # Speaker shape: white (0xFFFFFF00 with alpha 255)
    pixels = bytearray(32 * 32 * 4)  # BGRA

    def set_pixel(x, y, r, g, b, a=255):
        if 0 <= x < 32 and 0 <= y < 32:
            idx = ((31 - y) * 32 + x) * 4  # bottom-up
            pixels[idx] = b
            pixels[idx+1] = g
            pixels[idx+2] = r
            pixels[idx+3] = a

    def fill_ellipse(cx, cy, rx, ry, r, g, b, a=255):
        for y in range(max(0, cy-ry), min(32, cy+ry)):
            for x in range(max(0, cx-rx), min(32, cx+rx)):
                if ((x-cx)**2)/(rx**2) + ((y-cy)**2)/(ry**2) <= 1:
                    set_pixel(x, y, r, g, b, a)

    def fill_polygon(points, r, g, b, a=255):
        # Simple polygon fill using scanline
        pts = sorted(points, key=lambda p: p[1])
        if len(pts) < 3:
            return
        for y in range(32):
            xs = []
            for i in range(len(pts)):
                p1, p2 = pts[i], pts[(i+1) % len(pts)]
                if min(p1[1], p2[1]) <= y < max(p1[1], p2[1]):
                    x = int(p1[0] + (p2[0]-p1[0]) * (y-p1[1]) / (p2[1]-p1[1]))
                    xs.append(x)
            xs.sort()
            for i in range(0, len(xs)-1, 2):
                for x in range(max(0,xs[i]), min(32,xs[i+1])):
                    set_pixel(x, y, r, g, b, a)

    # Draw speaker body (ellipse)
    fill_ellipse(12, 16, 8, 10, 255, 255, 255)
    # Draw speaker cone (triangle pointing right)
    fill_polygon([(18, 10), (28, 4), (28, 28), (18, 22)], 255, 255, 255)
    # Draw sound waves (small arcs as lines - skip for simplicity)

    return buf + bytes(pixels)


def make_icon_handle(pixels_bgra: bytes, width: int, height: int):
    """Create a Windows HICON from BGRA pixel data."""
    user32 = ctypes.windll.user32

    class BITMAPINFOHEADER(ctypes.Structure):
        _fields_ = [
            ('biSize', wintypes.DWORD),
            ('biWidth', ctypes.c_long),
            ('biHeight', ctypes.c_long),
            ('biPlanes', wintypes.WORD),
            ('biBitCount', wintypes.WORD),
            ('biCompression', wintypes.DWORD),
            ('biSizeImage', wintypes.DWORD),
            ('biXPelsPerMeter', ctypes.c_long),
            ('biYPelsPerMeter', ctypes.c_long),
            ('biClrUsed', wintypes.DWORD),
            ('biClrImportant', wintypes.DWORD),
        ]

    class RGBQUAD(ctypes.Structure):
        _fields_ = [
            ('rgbBlue', wintypes.BYTE),
            ('rgbGreen', wintypes.BYTE),
            ('rgbRed', wintypes.BYTE),
            ('rgbAlpha', wintypes.BYTE),
        ]

    bmi = BITMAPINFOHEADER()
    bmi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bmi.biWidth = width
    bmi.biHeight = -height  # top-down
    bmi.biPlanes = 1
    bmi.biBitCount = 32
    bmi.biCompression = 0  # BI_RGB

    pBits = ctypes.create_string_buffer(pixels_bgra)

    hdc = user32.GetDC(None)
    try:
        bmi_ptr = ctypes.byref(bmi)
        pBits_ptr = ctypes.cast(pBits, ctypes.c_void_p)

        hbmp = ctypes.windll.gdi32.CreateCompatibleDC(hdc)
        try:
            dibsect = ctypes.windll.gdi32.CreateDIBSection(
                hbmp, bmi_ptr, 0, ctypes.byref(ctypes.c_void_p()), None, 0
            )
            if not dibsect:
                return None

            old = ctypes.windll.gdi32.SelectObject(hbmp, dibsect)
            try:
                ctypes.windll.gdi32.SetDIBits(
                    hbmp, dibsect, 0, height,
                    pBits_ptr, bmi_ptr, 0  # DIB_RGB_COLORS
                )
            finally:
                ctypes.windll.gdi32.SelectObject(hbmp, old)

            icon_info = ctypes.Structure
            class ICONINFO(ctypes.Structure):
                _fields_ = [
                    ('fIcon', wintypes.BOOL),
                    ('xHotspot', wintypes.DWORD),
                    ('yHotspot', wintypes.DWORD),
                    ('hbmMask', wintypes.HBITMAP),
                    ('hbmColor', wintypes.HBITMAP),
                ]

            ii = ICONINFO()
            ii.fIcon = True
            ii.hbmMask = dibsect
            ii.hbmColor = dibsect

            hicon = user32.CreateIconIndirect(ctypes.byref(ii))
            return hicon
        finally:
            ctypes.windll.gdi32.DeleteObject(dibsect)
            ctypes.windll.gdi32.DeleteDC(hbmp)
    finally:
        user32.ReleaseDC(None, hdc)
