ec - Echo Canceller
===================

The project enables Acoustic Echo Cancellation (AEC) on Linux.
It supports PC, Raspberry Pi, ReSpeaker Core V2 and Pi-like devices.

It's a part of [voice-engine](https://github.com/voice-engine).
The goal is to make an open source smart speaker for daily use.
It uses Linux ALSA API and SpeexDSP's AEC algorithm.

### Tested Hardware
+ [VOICEN Linear 4 Mic Array Kit](https://www.makerfabs.com/voicen-linear-4-mic-array-kit.html)
+ Raspberry Pi with ReSpeaker 2 Mics hat and ReSpeaker 4 Mic Linear Array
+ x86/64 Linux PC

### Build
```
sudo apt-get -y install libasound2-dev libspeexdsp-dev
git clone https://github.com/voice-engine/ec.git
cd ec
make
```

The commands will create `ec` and `ec_hw`.
For devices without hardware audio loopback, ec is used. Otherwise, `ec_hw` is used.
The hardware audio loopback means that audio output is captured by extra ADC and sent back as input audio channel.

--------------------------------------------------------------

### `ec` for devices without hardware audio loopback
For devices without hardware audio loopback, we need a way to get audio output data.
`ec` will be audio output broker reading audio data from a named pipe and playing it.
Meanwhile it will record audio and remove the playing audio from the recording,
and then output it to another named pipe. The audio stream diagram is like:


```
                   +-------------------+  +-------------------+
                   |  PulseAudio       +  |  PulseAudio       |
                   |  module+pipe+source  |  module+pipe+sink |
                   +----------+--------+  +-------+-----------+
+--------------+              ^                   |             +------------+
|ALSA          |              |                   |             |ALSA        |
|FIFO plugin   |              |                   v             |file plugin |
+------+-------+       +------+-------+    +------+-------+     +----+-------+
       ^               |    FIFO      |    |    FIFO      |          |
       +---------------+/tmp/ec.output|    | /tmp/ec.input| <--------+
                       +------+-------+    +------+-------+
                              ^                   |
                              |                   |
                              |                   |
                              |                   |
                           +--+--+                | playback
                           | AEC | <--------------+
                           +--+--+                |
                              ^                   |
                              |                   v
                       +------+-------+    +------+-------+
                       |  hw:x,y      |    |  hw:x,y      |
                       |  plughw:x,y  |    |  plughw:x,y  |
                       +--------------+    +--------------+
```

To use `ec`:

1. Run `./ec -h` to show its command line options
2. Run `arecord -L` and `aplay -L` to get audio devices' name
3. Select audio input/output devices `./ec -i {input device name} -o {output device name} -c {input channels}`

    ```
    # terminal #1, run ec
    ./ec -h
    ./ec -i plughw:1 -o plughw:1 -s

    # terminal #2, play 16k fs, 16 bits, 1 channel raw audio
    cat 16k_s16le_mono_audio.raw > /tmp/ec.input

    # terminal #3, record
    cat /tmp/ec.output > 16k_s16le_stereo_audio.raw
    ```
    `ec` reads playback raw audio from the FIFO `/tmp/ec.input` and writes processed recording audio to the FIFO `/tmp/ec.output`

#### Use `ec` with ALSA plugins as ALSA devices
ALSA's [file plugin](https://www.alsa-project.org/alsa-doc/alsa-lib/pcm_plugins.html) can be used to configure the FIFO `/tmp/ec.input` as a playback device. As the file plugin requires a slave device to support capturing, but nomally we don't have an extra capture device, so [the FIFO plugin](https://github.com/voice-engine/alsa_plugin_fifo) is written to use the FIFO `/tmp/ec.output` as a capture device.

1. install the FIFO plugin

   ```
   git clone https://github.com/voice-engine/alsa_plugin_fifo.git
   cd alsa_plugin_fifo
   make && sudo make install
   ```

2. copy [asound.conf](asound.conf) to `~/.asoundrc` (or `/etc/asound.conf`) to apply the configuration.


#### Use `ec` with PulseAudio
PulseAudio has [module-pipe-sink](https://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/User/Modules/#index1h3) and [module-pipe-source](https://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/User/Modules/#index2h3) which can be used to configure `/tmp/ec.input` and `/tmp/ec.output` as the default audio output and input.

PulseAudio (version < 12) will create `/tmp/ec.input` and `/tmp/ec.output`. If the two FIFO already exist, PulseAudio will fail, we should delete the two FIFO (`rm /tmp/ec.input /tmp/ec.output`) and then launch PulseAudio. It means we must start PulseAudio before we start `ec`. For example, use the following commands to setup PulseAudio and `ec`.

```
pacmd load-module module-pipe-sink sink_name=ec.sink format=s16 rate=16000 channels=1 file=/tmp/ec.input
pacmd load-module module-pipe-source source_name=ec.source format=s16 rate=16000 channels=2 file=/tmp/ec.output
pacmd set-default-sink ec.sink
pacmd set-default-source ec.source

./ec -i plughw:0 -o plughw:0
```

 We can also use PulseAudio's configuration file to load `module-pip-sink` and `module-pipe-source`. Just replace `/etc/pulse/pulse.default.pa` with [pulse.default.pa](pulse.default.pa).

If you have installed PulseAudio but want to disable it, we need to disable PulseAudio's autospawn feature by adding `autospawn=no` to `~/.config/pulse/client.conf` or `/etc/pulse/client.conf`, and then run `pulseaudio -k`

#### `ec` for Raspberry Pi
>The sound from the on-board audio jack of the Raspberry Pi has serious distortion, do not use the on-board audio jack!

+ ReSpeaker 2 Mic Hat for Raspberry Pi

  The delay between playback and recording is about 200. Try `./ec -i plughw:1 -o plughw:1 -d 200`

-----------------------------------------------------------------------------

### `ec_hw` for devices with hardware audio loopback
For devices such as ReSpeaker Core V2, ReSpeaker 6 Mic Array for Pi and ReSpeaker Linear 4 Mic Array for Pi,
audio output is captured as one of the input channels. The audio stream diagram is simpler.

```
                       +---------+          +-------+         +------+
                       |         |          |       |         |      |
                       |         |          |       |         | FILE |
Microphones ---------->|         |--------> |  AEC  |-------> | FIFO |----> Audio Input
                       |   ADC   |          |       |         |      |
                       |         |          +-------+         +------+
                       |         |             /
             __________|         |____________/
            |          +---------+   Loopback Channel
            |
            |          +---------+
            |          |         |
Speakers <-------------|   DAC   |<------- Audio Output
                       |         |
                       +---------+
```

To use `ec_hw`:

1. Run `./ec_hw -h` to show its command line options
2. Run `arecord -L` to get audio input devices' name
3. Select the audio input device `./ec_hw -i {input device name} -c {input channels} -l {loopback channel} -m {mic channel list}`

    ```
    # terminal #1, run ec
    ./ec_hw -h
    ./ec -i plughw:1 -c 8 -l 7 -m 0,1,2,3

    # terminal #3, record
    cat /tmp/ec.output > 16k_s16le_4_channels.raw
    ```
    `ec_hw` uses channel 7 as playback audio, remove the playback from channels 0,1,2,3 and writes processed audio to the FIFO `/tmp/ec.output`

### License
GPL V3

### Credits
+ The ring buffer implementation is from PortAudio
+ [SpeexDSP](https://github.com/xiph/speexdsp) provides the excellent open source AEC algorithm
+ Using named pipe for I/O is inspired by [snapcast](https://github.com/badaix/snapcast)
