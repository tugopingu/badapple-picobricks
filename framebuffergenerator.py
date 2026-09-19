#!/usr/bin/env python3

# SPDX-License-Identifier: MIT
#
# framebuffergenerator.py
#
# badapple-picobricks
#
# Copyright (c) 2026

import numpy as np
import cv2
import sys

badapple = cv2.VideoCapture("badapple.webm")

ret, prev_frame = badapple.read()
if not ret:
    print("video not found")
    sys.exit()
prev_gray = cv2.cvtColor(prev_frame, cv2.COLOR_BGR2GRAY)
ret, prev_binary = cv2.threshold(prev_gray, 127, 1, cv2.THRESH_BINARY)

compressed_frames = []
xor_delta_starting_bit = []

while badapple.isOpened():
    ret, frame = badapple.read()
    if ret:
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        ret, binary = cv2.threshold(gray, 127, 1, cv2.THRESH_BINARY)
        xor_delta = prev_binary ^ binary

        flat_xor = xor_delta.flatten(order="F")
        compressed_pixels = []
        starting_bit = False
        if flat_xor[0] == 1:
            starting_bit = True
        bit_counter = 0
        bit_pattern = starting_bit
        xor_delta_starting_bit.append(int(flat_xor[0]))
        for i in flat_xor:
            if i == bit_pattern:
                bit_counter += 1
            else:
                compressed_pixels.append(bit_counter)
                bit_pattern ^= 1
                bit_counter = 1
        compressed_pixels.append(bit_counter)
        compressed_frames.append(compressed_pixels)

        prev_binary = binary
    else:
        break

frame_sizes = []
frame_number = 0
print("const uint8_t frame_data[] = {", end="")
for i in compressed_frames:
    bytes = 0
    for j in i:
        if j <= 128:
            print(j - 1, end=",")
        else:
            print(((j - 1) >> 8) + 128, end=",")
            print((j - 1) % 256, end=",")
        bytes += 1
    frame_number += 1
    frame_sizes.append(bytes)
print("};")

print("const uint16_t frame_sizes[] = {", end="")
for i in frame_sizes:
    print(i, end=",")
print("};")

print("const uint8_t starting_bits[] = {", end="")
for i in xor_delta_starting_bit:
    print(i, end=",")
print("};")

badapple.release()
