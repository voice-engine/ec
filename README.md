EC - Echo Canceller
===================

The `ec` is an Acoustic Echo Cancellation (AEC) deamon.
It is a part of [voice-engine project](https://github.com/voice-engine).
The goal is to make an open source smart speaker for daily use.

It will read audio data from a named pipe and play it. In the mean time, PortAudio and remove output audio from input audio.
Meanwhile it will record audio and remove the playing audio from the recording, and then output it to another named pipe.

It uses PortAudio library to read and write audio, uses SpeexDSP's AEC algorithm.

### Build
```
sudo apt-get -y install libasound2-dev libspeexdsp-dev
git clone https://github.com/voice-engine/ec.git
cd ec
make
```

### Configuration
+ ALSA, use [File plugin](https://www.alsa-project.org/alsa-doc/alsa-lib/pcm_plugins.html)

+ PulseAudio, use [module-pipe-sink](https://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/User/Modules/#index1h3) and [module-pipe-source](https://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/User/Modules/#index2h3)

### Usage
1. Run `ec -h` to show its command line options
2. Run `ec -l` to get audio devices' index
3. Select audio input/output devices `ec -i {input device index} -o {output device index} -c {input channels}`

```
ec -h
ec -l
ec -i 0 -o 0 -s
```

### Hardware
+ ReSpeaker 2 Mic Hat for Raspberry Pi

  The system delay between ALSA `playback` device and ALSA `capture` device is about 5900 / 16000 second

### License
GPL V3

### Credits
+ PortAudio
+ [SpeexDSP](https://github.com/xiph/speexdsp) provides the excellent open source AEC algorithm
+ Using named pipe for I/O is inspired by [snapcast](https://github.com/badaix/snapcast)
+ `install_portaudio.sh`, `portaudio.patch` and `Makefile` are from [Snowboy](https://github.com/Kitt-AI/snowboy)
