#include "common/common.hpp"
#include "sha_engine.hpp"

const static uint32_t k_1[4] =
{
   0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6
};

const static uint32_t k_256[64] =
{
   0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
   0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
   0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
   0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
   0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
   0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
   0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
   0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

void SHA_Engine::reset_hash()
{
    switch (SHA_CNT.mode)
    {
        case 0:
            //SHA-256
            hash[0] = 0x6a09e667;
            hash[1] = 0xbb67ae85;
            hash[2] = 0x3c6ef372;
            hash[3] = 0xa54ff53a;
            hash[4] = 0x510e527f;
            hash[5] = 0x9b05688c;
            hash[6] = 0x1f83d9ab;
            hash[7] = 0x5be0cd19;
            break;
        case 2:
            //SHA-1
            hash[0] = 0x67452301;
            hash[1] = 0xEFCDAB89;
            hash[2] = 0x98BADCFE;
            hash[3] = 0x10325476;
            hash[4] = 0xC3D2E1F0;

            //Unused
            hash[5] = 0;
            hash[6] = 0;
            hash[7] = 0;
            break;
        default:
            EmuException::die("[SHA] Unrecognized mode %d\n", SHA_CNT.mode);
    }

    message_len = 0;
}

void SHA_Engine::do_hash(bool final_round)
{
    switch (SHA_CNT.mode)
    {
        case 0x0:
            do_sha256(final_round);
            break;
        case 0x2:
        case 0x3:
            do_sha1(final_round);
            break;
        default:
            EmuException::die("[SHA] Unrecognized hash mode %d\n", SHA_CNT.mode);
    }
}

void SHA_Engine::do_sha256(bool final_round)
{
    if (final_round)
    {
        int round_size = in_fifo.size();
        for (int i = 0; i < round_size; i++)
        {
            messages[i] = bswp32(in_fifo.front());
            in_fifo.pop();
        }

        //Clear to zero
        for (int i = round_size; i < 16; i++)
            messages[i] = 0;

        //Append '1' to the end of the user message
        messages[round_size] = bswp32(0x80);

        //Convert to bits
        message_len *= 4 * 8;
        uint32_t len_lo = message_len & 0xFFFFFFFF;

        if (round_size >= 14)
        {
            //EmuException::die("[SHA] 14 or above\n");
            _sha256();

            //Not enough space to store the message variable. We need to do another round
            for (int i = 0; i < 16; i++)
                messages[i] = 0;

            messages[15] = len_lo;

            _sha256();
        }
        else
        {
            messages[15] = len_lo;
            _sha256();
        }

        /*if (SHA_CNT.out_big_endian)
        {
            for (int i = 0; i < 8; i++)
                hash[i] = bswp32(hash[i]);
        }*/
    }
    else
    {
        for (int i = 0; i < 16; i++)
        {
            messages[i] = bswp32(in_fifo.front());
            in_fifo.pop();
        }
        _sha256();
    }
}

void SHA_Engine::do_sha1(bool final_round)
{
    if (final_round)
    {
        int round_size = in_fifo.size();
        for (int i = 0; i < round_size; i++)
        {
            messages[i] = bswp32(in_fifo.front());
            in_fifo.pop();
        }

        //Clear to zero
        for (int i = round_size; i < 16; i++)
            messages[i] = 0;

        //Append '1' to the end of the user message
        messages[round_size] = bswp32(0x80);

        //Convert to bits
        message_len *= 4 * 8;
        uint32_t len_lo = message_len & 0xFFFFFFFF;

        if (round_size >= 14)
        {
            EmuException::die("[SHA] 14 or above\n");
            _sha1();

            //Not enough space to store the message variable. We need to do another round
            for (int i = 0; i < 16; i++)
                messages[i] = 0;

            messages[15] = len_lo;

            _sha1();
        }
        else
        {
            messages[15] = len_lo;
            _sha1();
        }
    }
    else
    {
        for (int i = 0; i < 16; i++)
        {
            messages[i] = bswp32(in_fifo.front());
            in_fifo.pop();
        }
        _sha1();
    }
}

void SHA_Engine::_sha256()
{
    for (int i = 16; i < 64; i++)
    {
        uint32_t msg0 = messages[i - 15];
        uint32_t msg1 = messages[i - 2];
        uint32_t s0 = rotr32(msg0, 7) ^ rotr32(msg0, 18) ^ (msg0 >> 3);
        uint32_t s1 = rotr32(msg1, 17) ^ rotr32(msg1, 19) ^ (msg1 >> 10);
        messages[i] = messages[i - 16] + messages[i - 7] + s0 + s1;
    }

    uint32_t a, b, c, d, e, f, g, h;
    a = hash[0];
    b = hash[1];
    c = hash[2];
    d = hash[3];
    e = hash[4];
    f = hash[5];
    g = hash[6];
    h = hash[7];

    for (int i = 0; i < 64; i++)
    {
        uint32_t S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + S1 + ch + k_256[i] + messages[i];
        uint32_t S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    hash[0] += a;
    hash[1] += b;
    hash[2] += c;
    hash[3] += d;
    hash[4] += e;
    hash[5] += f;
    hash[6] += g;
    hash[7] += h;
}

void SHA_Engine::_sha1()
{
    for (int i = 16; i < 80; i++)
    {
        messages[i] = messages[i - 3] ^ messages[i - 8] ^ messages[i - 14] ^ messages[i - 16];
        messages[i] = rotl32(messages[i], 1);
    }

    uint32_t a, b, c, d, e, f;
    a = hash[0];
    b = hash[1];
    c = hash[2];
    d = hash[3];
    e = hash[4];

    for (int i = 0; i < 80; i++)
    {
        int index = i / 20;
        uint32_t k = k_1[index];
        switch (index)
        {
            case 0:
                f = (b & c) | ((~b) & d);
                break;
            case 1:
                f = b ^ c ^ d;
                break;
            case 2:
                f = (b & c) | (b & d) | (c & d);
                break;
            case 3:
                f = b ^ c ^ d;
                break;
        }

        uint32_t temp = rotl32(a, 5) + f + e + k + messages[i];
        e = d;
        d = c;
        c = rotl32(b, 30);
        b = a;
        a = temp;
    }

    hash[0] += a;
    hash[1] += b;
    hash[2] += c;
    hash[3] += d;
    hash[4] += e;
}
