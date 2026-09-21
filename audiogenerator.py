#!/usr/bin/env python3

# SPDX-License-Identifier: MIT
#
# messagegenerator.py
#
# badapple-picobricks
#
# Copyright (c) 2026

import mido
import math

mid = mido.MidiFile("monophonicapple.mid")
print("const Note notes[] = {")
# The video is black for around 1.3 seconds, so I added this gap
# manually so the sound better matches the video
print("{0, 1300000},")
for msg in mid:
    if msg.is_meta:
        continue
    if msg.type == "note_off":
        freq = math.floor((2 ** ((msg.note - 69) / 12)) * 440)
        us = math.floor(msg.time * 1000000)
        print("{", freq, ", ", us, "},")
    elif msg.type == "note_on":
        silence = math.floor(msg.time * 1000000)
        if silence != 0:
            print("{0, ", silence, "},")
print("};")
