"""
Read audio from a file, and then search the keyword "snowboy"
"""

import sys
import signal
import time
import logging

from voice_engine.raw_file_source import Source
from voice_engine.channel_picker import ChannelPicker
from voice_engine.ns import NS
from voice_engine.kws import KWS
from avs.alexa import Alexa
from pixels import pixels

logging.basicConfig(level=logging.INFO)


def main():
    src = Source('/tmp/ec.output', rate=16000, channels=2)
    ch0 = ChannelPicker(channels=src.channels, pick=1)
    ns = NS()
    kws = KWS(model='alexa', sensitivity=0.7)
    alexa = Alexa()

    src.pipeline(ch0, ns, kws, alexa)

    def on_detected(keyword):
        print('detected {}'.format(keyword))
        alexa.listen()

    kws.set_callback(on_detected)

    alexa.state_listener.on_listening = pixels.listen
    alexa.state_listener.on_thinking = pixels.think
    alexa.state_listener.on_speaking = pixels.speak
    alexa.state_listener.on_finished = pixels.off

    src.pipeline_start()

    is_quit = []
    def signal_handler(sig, frame):
        is_quit.append(True)
        print('quit')
    signal.signal(signal.SIGINT, signal_handler)

    while src.is_active() and not is_quit:
        time.sleep(1)

    src.pipeline_stop()


if __name__ == '__main__':
    main()
