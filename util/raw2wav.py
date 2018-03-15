#!/usr/bin/env python
#
# Convert raw audio (/tmp/playback.raw, /tmp/recording.raw and /tmp/out.raw) to wave file
# Usage:
#   python raw2wav.py [channels] [rate]
#
# By default, recording channels = 2, rate = 16000


import sys
import wave


channels = 2 if len(sys.argv) < 2 else int(sys.argv[1])
rate = 16000 if len(sys.argv) < 5 else int(sys.argv[4])
    

with open('/tmp/playback.raw', 'rb') as raw:
    wav = wave.open('playback.wav', 'wb')
    wav.setframerate(rate)
    wav.setsampwidth(2)
    wav.setnchannels(1)
    wav.writeframes(raw.read())
    wav.close()

with open('/tmp/recording.raw', 'rb') as raw:
    wav = wave.open('recording.wav', 'wb')
    wav.setframerate(rate)
    wav.setsampwidth(2)
    wav.setnchannels(channels)
    wav.writeframes(raw.read())
    wav.close()

with open('/tmp/out.raw', 'rb') as raw:
    wav = wave.open('out.wav', 'wb')
    wav.setframerate(rate)
    wav.setsampwidth(2)
    wav.setnchannels(channels)
    wav.writeframes(raw.read())
    wav.close()