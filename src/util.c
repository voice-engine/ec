// from http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
unsigned power2(unsigned v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v;
}