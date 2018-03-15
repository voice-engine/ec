"""
Read audio from a file, and then search the keyword "snowboy"
"""

import sys
import time
from voice_engine.raw_file_source import Source
from voice_engine.channel_picker import ChannelPicker
from voice_engine.kws import KWS


def main():
    src = Source('/tmp/ec.output', rate=16000, channels=2)
    ch0 = ChannelPicker(channels=src.channels, pick=1)
    kws = KWS(sensitivity=0.7)

    src.pipeline(ch0, kws)

    def on_detected(keyword):
        print('detected {}'.format(keyword))

    kws.set_callback(on_detected)

    src.pipeline_start()
    while src.is_active():
        try:
            time.sleep(1)
        except KeyboardInterrupt:
            break

    src.pipeline_stop()


if __name__ == '__main__':
    main()
