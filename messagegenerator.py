#!/usr/bin/env python3

# SPDX-License-Identifier: GPL-3.0-or-later
#
# messagegenerator.py
#
# badapple-picobricks
#
# Copyright (c) 2026 Tuna Necmi İnal

import mido
import math

mid = mido.MidiFile("monophonicapple.mid")
print("const Message messages[] = {")
for msg in mid:
    if msg.is_meta:
        continue
    elif msg.type == "note_off" or msg.type == "note_on":
        freq = math.floor((2 ** ((msg.note - 49) / 12)) * 440)
        us = math.floor(msg.time * 1000000)
        msgtype = 0
        if msg.type == "note_on":
            msgtype = 1
        print("{", freq, ",", us, ",", msgtype, "},")
print("};")
