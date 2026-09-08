"""Tiny Ed25519 sign/verify for GLKSIG1-ED25519 (host/CI only).

Pure Python RFC 8032. Not used on the STM32 image (crypto budget).
"""

from __future__ import annotations

import hashlib
import os

_P = 2**255 - 19
_L = 2**252 + 27742317777372353535851937790883648493
_D = -121665 * pow(121666, _P - 2, _P) % _P
_I = pow(2, (_P - 1) // 4, _P)


def _inv(x: int) -> int:
    return pow(x, _P - 2, _P)


def _xrecover(y: int) -> int:
    xx = (y * y - 1) * _inv(_D * y * y + 1)
    x = pow(xx, (_P + 3) // 8, _P)
    if (x * x - xx) % _P != 0:
        x = (x * _I) % _P
    if x % 2 != 0:
        x = _P - x
    return x


_BY = 4 * _inv(5)
_BX = _xrecover(_BY)
_B = (_BX % _P, _BY % _P)


def _edwards(p: tuple[int, int], q: tuple[int, int]) -> tuple[int, int]:
    x1, y1 = p
    x2, y2 = q
    x3 = (x1 * y2 + x2 * y1) * _inv(1 + _D * x1 * x2 * y1 * y2)
    y3 = (y1 * y2 + x1 * x2) * _inv(1 - _D * x1 * x2 * y1 * y2)
    return (x3 % _P, y3 % _P)


def _scalarmult(p: tuple[int, int], e: int) -> tuple[int, int]:
    if e == 0:
        return (0, 1)
    q = _scalarmult(p, e // 2)
    q = _edwards(q, q)
    if e & 1:
        q = _edwards(q, p)
    return q


def _encodeint(y: int) -> bytes:
    return y.to_bytes(32, "little")


def _encodepoint(p: tuple[int, int]) -> bytes:
    x, y = p
    bits = bytearray(_encodeint(y))
    bits[-1] |= 0x80 if x & 1 else 0
    return bytes(bits)


def _bit(h: bytes, i: int) -> int:
    return (h[i // 8] >> (i % 8)) & 1


def _hint(m: bytes) -> int:
    h = hashlib.sha512(m).digest()
    return int.from_bytes(h, "little")


def _decodeint(s: bytes) -> int:
    return int.from_bytes(s, "little")


def _decodepoint(s: bytes) -> tuple[int, int]:
    y = _decodeint(s) & ((1 << 255) - 1)
    x = _xrecover(y)
    if x & 1 != _bit(s, 255):
        x = _P - x
    return (x, y)


def publickey(sk: bytes) -> bytes:
    h = hashlib.sha512(sk).digest()
    a = 2 ** (256 - 2) + sum(2**i * _bit(h, i) for i in range(3, 256 - 2))
    return _encodepoint(_scalarmult(_B, a))


def signature(m: bytes, sk: bytes, pk: bytes) -> bytes:
    h = hashlib.sha512(sk).digest()
    a = 2 ** (256 - 2) + sum(2**i * _bit(h, i) for i in range(3, 256 - 2))
    r = _hint(h[32:] + m)
    R = _scalarmult(_B, r)
    S = (r + _hint(_encodepoint(R) + pk + m) * a) % _L
    return _encodepoint(R) + _encodeint(S)


def checkvalid(sig: bytes, m: bytes, pk: bytes) -> bool:
    if len(sig) != 64 or len(pk) != 32:
        return False
    R = _decodepoint(sig[:32])
    A = _decodepoint(pk)
    S = _decodeint(sig[32:])
    if S >= _L:
        return False
    h = _hint(_encodepoint(R) + pk + m)
    return _scalarmult(_B, S) == _edwards(R, _scalarmult(A, h))


def glksig1_ed25519_line(manifest: bytes, sk: bytes) -> str:
    pk = publickey(sk)
    sig = signature(manifest, sk, pk)
    return "GLKSIG1-ED25519 " + sig.hex()


def verify_glksig1_ed25519(manifest: bytes, line: str, pk: bytes) -> bool:
    line = line.strip()
    prefix = "GLKSIG1-ED25519 "
    if not line.startswith(prefix):
        return False
    hexpart = line[len(prefix) :].strip()
    if len(hexpart) != 128:
        return False
    try:
        sig = bytes.fromhex(hexpart)
    except ValueError:
        return False
    return checkvalid(sig, manifest, pk)


def generate_sk() -> bytes:
    return os.urandom(32)
