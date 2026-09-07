import mido
import math

mid = mido.MidiFile("monophonicapple.mid")
for msg in mid:
    if msg.is_meta:
        continue
    elif msg.type == "note_off":
        freq = math.floor((2 ** ((msg.note - 49) / 12)) * 440)
        ms = math.floor(msg.time * 1000)
        print("{", freq, ", ", ms, "},")
    elif msg.type == "note_on":
        silence = math.floor(msg.time * 1000)
        if silence != 0:
            print("{0, ", silence, "},")
