
import sys
import wave
import numpy as np


if len(sys.argv) != 3:
    print('Usage: {} near.wav far.wav'.format(sys.argv[0]))
    sys.exit(1)


near = wave.open(sys.argv[1], 'rb')
far = wave.open(sys.argv[2], 'rb')
rate = near.getframerate()

channels = near.getnchannels()

N = rate


def gcc_phat(sig, refsig, fs=1, max_tau=None, interp=1):
    '''
    This function computes the offset between the signal sig and the reference signal refsig
    using the Generalized Cross Correlation - Phase Transform (GCC-PHAT)method.
    '''
    
    # make sure the length for the FFT is larger or equal than len(sig) + len(refsig)
    n = sig.shape[0] + refsig.shape[0]

    # Generalized Cross Correlation Phase Transform
    SIG = np.fft.rfft(sig, n=n)
    REFSIG = np.fft.rfft(refsig, n=n)
    R = SIG * np.conj(REFSIG)

    cc = np.fft.irfft(R / np.abs(R), n=(interp * n))

    max_shift = int(interp * n / 2)
    if max_tau:
        max_shift = np.minimum(int(interp * fs * max_tau), max_shift)

    cc = np.concatenate((cc[-max_shift:], cc[:max_shift+1]))

    # find max cross correlation index
    shift = np.argmax(np.abs(cc)) - max_shift

    tau = shift / float(interp * fs)
    
    return tau, cc

while True:
    sig = near.readframes(N)
    if len(sig) != 2 * N * channels:
        break

    ref = far.readframes(N)
    ref_buf = np.fromstring(ref, dtype='int16')
    data = np.fromstring(sig, dtype='int16')

    offsets = []
    for ch in range(channels):
        sig_buf = data[ch::channels]
        tau, _ = gcc_phat(sig_buf, ref_buf, fs=1, max_tau=N/2, interp=1)
        # tau, _ = gcc_phat(sig_buf, ref_buf, fs=rate, max_tau=1)

        offsets.append(tau)
    print(offsets)

