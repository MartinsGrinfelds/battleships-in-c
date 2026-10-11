#!/usr/bin/env python3
"""Sends fake mouse and keyboard input to X display (uses XTest extension).
Used by GUI test to play the game like a person would.

Usage: xinput.py COMMAND... where command is one of:
  move X Y      move mouse to screen position
  click X Y     move mouse and click left button
  rclick X Y    move mouse and click right button
  key NAME      press and release key (X keysym name: Return, Up, r, 1, Tab, ...)
  type TEXT     type text (letters, digits and spaces)
  sleep SECONDS
"""
import ctypes
import sys
import time

xlib = ctypes.CDLL("libX11.so.6")
xtst = ctypes.CDLL("libXtst.so.6")
xlib.XOpenDisplay.restype = ctypes.c_void_p
xlib.XOpenDisplay.argtypes = [ctypes.c_char_p]
xlib.XStringToKeysym.restype = ctypes.c_ulong
xlib.XStringToKeysym.argtypes = [ctypes.c_char_p]
xlib.XKeysymToKeycode.restype = ctypes.c_ubyte
xlib.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
xlib.XFlush.argtypes = [ctypes.c_void_p]
xtst.XTestFakeMotionEvent.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_ulong]
xtst.XTestFakeButtonEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
xtst.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]

display = xlib.XOpenDisplay(None)
if not display:
    sys.exit("Can't open X display (is DISPLAY set?)")

# Raylib reads input once per frame, so every event is held for a few frames.
HOLD = 0.06


def move(x, y):
    xtst.XTestFakeMotionEvent(display, -1, int(x), int(y), 0)
    xlib.XFlush(display)
    time.sleep(HOLD)


def click(x, y, button=1):
    move(x, y)
    xtst.XTestFakeButtonEvent(display, button, 1, 0)
    xlib.XFlush(display)
    time.sleep(HOLD)
    xtst.XTestFakeButtonEvent(display, button, 0, 0)
    xlib.XFlush(display)
    time.sleep(HOLD)


def key(name):
    keycode = xlib.XKeysymToKeycode(display, xlib.XStringToKeysym(name.encode()))
    if not keycode:
        sys.exit("Unknown key: " + name)
    xtst.XTestFakeKeyEvent(display, keycode, 1, 0)
    xlib.XFlush(display)
    time.sleep(HOLD)
    xtst.XTestFakeKeyEvent(display, keycode, 0, 0)
    xlib.XFlush(display)
    time.sleep(HOLD)


def type_text(text):
    shift = xlib.XKeysymToKeycode(display, xlib.XStringToKeysym(b"Shift_L"))
    for character in text:
        if character.isupper():
            # Capital letters need Shift held down.
            xtst.XTestFakeKeyEvent(display, shift, 1, 0)
            key(character.lower())
            xtst.XTestFakeKeyEvent(display, shift, 0, 0)
            xlib.XFlush(display)
        else:
            key("space" if character == " " else character)


arguments = sys.argv[1:]
while arguments:
    command = arguments.pop(0)
    if command == "move":
        move(arguments.pop(0), arguments.pop(0))
    elif command == "click":
        click(arguments.pop(0), arguments.pop(0))
    elif command == "rclick":
        click(arguments.pop(0), arguments.pop(0), 3)
    elif command == "key":
        key(arguments.pop(0))
    elif command == "type":
        type_text(arguments.pop(0))
    elif command == "sleep":
        time.sleep(float(arguments.pop(0)))
    else:
        sys.exit("Unknown command: " + command)
