EC - Echo Canceller
===================

The `ec` is an Acoustic Echo Cancellation (AEC) deamon.
It is a part of [voice-engine project](https://github.com/voice-engine).
The goal is to make an open source smart speaker for daily use.

It will read audio data from a named pipe and play it.
Meanwhile it will record audio and remove the playing audio from the recording,
and then output it to another named pipe.

It uses ALSA API to read and write audio, uses SpeexDSP's AEC algorithm.

### Build
```
sudo apt-get -y install libasound2-dev libspeexdsp-dev
git clone https://github.com/voice-engine/ec.git
cd ec
make
```

### Basic usage
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

### Use with ALSA file plugin
ALSA's [file plugin](https://www.alsa-project.org/alsa-doc/alsa-lib/pcm_plugins.html) can be used to configure the FIFO `/tmp/ec.input` as the default ALSA output. Just copy [asound.conf](asound.conf) to `~/.asoundrc` to apply the configuration.

```
pcm.!default pcm.ec

pcm.ec {
    type plug
    slave {
        format S16_LE
        rate 16000
        channels 1
        pcm {
            type file
            slave.pcm null
            file "/tmp/ec.input"
            format "raw"
        }
    }
}
```


### Use with PulseAudio
PulseAudio has [module-pipe-sink](https://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/User/Modules/#index1h3) and [module-pipe-source](https://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/User/Modules/#index2h3) which can be used to configure `/tmp/ec.input` and `/tmp/ec.output` as the default audio output and input. We just need to copy [pulse.default.pa](pulse.default.pa) to `/etc/pulse`

PulseAudio will create `/tmp/ec.input` and `/tmp/ec.output`. If the two FIFO already exist, PulseAudio will fail, we should delete the two FIFO (`rm /tmp/ec.input /tmp/ec.output`) and then launch PulseAudio. It means we must start PulseAudio before we start `ec`.

If you have installed PulseAudio but want to disable it, we need to disable PulseAudio's autospawn feature by adding `autospawn=no` to `~/.config/pulse/client.conf` or `/etc/pulse/client.conf`, and then run `pulseaudio -k`

### Hardware
>The sound from the on-board audio jack of the Raspberry Pi has serious distortion, do not use the on-board audio jack!

+ ReSpeaker 2 Mic Hat for Raspberry Pi

  The delay between playback and recording is about 200. Try `./ec -i plughw:1 -o plughw:1 -d 200`

### License
GPL V3

### Credits
+ The ring buffer implementation is from PortAudio
+ [SpeexDSP](https://github.com/xiph/speexdsp) provides the excellent open source AEC algorithm
+ Using named pipe for I/O is inspired by [snapcast](https://github.com/badaix/snapcast)
