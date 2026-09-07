import mido
import math

mid = mido.MidiFile("monophonicapple.mid")
print("Message messages[] = {")
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
