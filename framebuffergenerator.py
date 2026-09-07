import numpy as np
import cv2 as cv

badapple = cv.VideoCapture("badapple.webm")

def
frame = badapple.read()

badapple.release()
