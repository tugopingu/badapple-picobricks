#!/usr/bin/env python3

# SPDX-License-Identifier: MIT
#
# framebuffergenerator.py
#
# badapple-picobricks
#
# Copyright (c) 2026 Tuna Necmi İnal

import numpy as np
import cv2
import sys

badapple = cv2.VideoCapture("badapple.webm")

ret, prevFrame = badapple.read()
if not ret:
    print("video not found")
    sys.exit()
prevGray = cv2.cvtColor(prevFrame, cv2.COLOR_BGR2GRAY)
ret, prevBinary = cv2.threshold(prevGray, 127, 1, cv2.THRESH_BINARY)

compressedFrames = []
xorDeltaStartingBit = []

while badapple.isOpened():
    ret, frame = badapple.read()
    if ret:
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        ret, binary = cv2.threshold(gray, 127, 1, cv2.THRESH_BINARY)
        xorDelta = prevBinary ^ binary

        flatXor = xorDelta.flatten(order="C")
        compressedPixels = []
        startingBit = False
        if flatXor[0] == 1:
            startingBit = True
        bitCounter = 0
        bitPattern = startingBit
        xorDeltaStartingBit.append(int(flatXor[0]))
        for i in flatXor:
            if i == bitPattern:
                bitCounter += 1
            else:
                compressedPixels.append(bitCounter)
                bitPattern ^= 1
                bitCounter = 1
        compressedPixels.append(bitCounter)
        compressedFrames.append(compressedPixels)

        prevBinary = binary
    else:
        break

frameSizes = []
frameNumber = 0
print("const uint8_t frameData[] = {", end="")
for i in compressedFrames:
    bytes = 0
    for j in i:
        if j <= 128:
            print(j - 1, end=",")
        else:
            print(((j - 1) >> 8) + 128, end=",")
            print((j - 1) % 256, end=",")
        bytes += 1
    frameNumber += 1
    frameSizes.append(bytes)
print("};")

print("const uint16_t frameSizes[] = {", end="")
for i in frameSizes:
    print(i, end=",")
print("};")

print("const uint8_t startingBits[] = {", end="")
for i in xorDeltaStartingBit:
    print(i, end=",")
print("};")

badapple.release()
