"""PC-side sealed vault for MedSec casefiles / exports.

Encrypts a directory or file with a password-derived key (PBKDF2 + Fernet if
cryptography is available; otherwise AES-free XOR is NOT used — we require
stdlib-only Fernet alternative via hashlib+hmac+AES is heavy.

Stdlib path: PBKDF2-HMAC-SHA256 + XOR stream is weak; prefer optional cryptography.

We implement **stdlib sealed container**:
  - magic GLKSEAL1
  - salt + iterations
  - HMAC-SHA256 over ciphertext
  - ciphertext = byte-wise XOR with keystream from SHA256(counter||key)  [OK for local lab, not FIPS]

Documented as lab confidentiality aid, not a certified crypto product.
"""

from __future__ import annotations

import hashlib
import hmac
import json
import os
import struct
import time
import zipfile
from pathlib import Path
from typing import Any, Optional

MAGIC = b"GLKSEAL1"
ITERATIONS = 200_000


def _derive_key(password: str, salt: bytes, iterations: int = ITERATIONS) -> bytes:
    return hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"), salt, iterations, dklen=32)


def _keystream(key: bytes, length: int) -> bytes:
    out = bytearray()
    counter = 0
    while len(out) < length:
        block = hashlib.sha256(key + struct.pack(">Q", counter)).digest()
        out.extend(block)
        counter += 1
    return bytes(out[:length])


def _xor(data: bytes, key: bytes) -> bytes:
    stream = _keystream(key, len(data))
    return bytes(a ^ b for a, b in zip(data, stream))


def seal_bytes(plaintext: bytes, password: str) -> bytes:
    salt = os.urandom(16)
    key = _derive_key(password, salt)
    ct = _xor(plaintext, key)
    tag = hmac.new(key, ct, hashlib.sha256).digest()
    header = MAGIC + struct.pack(">I", ITERATIONS) + salt + tag
    return header + ct


def unseal_bytes(blob: bytes, password: str) -> bytes:
    if len(blob) < 8 + 4 + 16 + 32:
        raise ValueError("seal_too_short")
    if blob[:8] != MAGIC:
        raise ValueError("bad_magic")
    iterations = struct.unpack(">I", blob[8:12])[0]
    salt = blob[12:28]
    tag = blob[28:60]
    ct = blob[60:]
    key = _derive_key(password, salt, iterations=iterations)
    expect = hmac.new(key, ct, hashlib.sha256).digest()
    if not hmac.compare_digest(tag, expect):
        raise ValueError("hmac_mismatch_or_bad_password")
    return _xor(ct, key)


def seal_directory(src_dir: Path, out_path: Path, password: str, *, meta: Optional[dict[str, Any]] = None) -> Path:
    """Zip directory in memory and seal to out_path (.glkseal)."""
    src_dir = Path(src_dir)
    out_path = Path(out_path)
    import io

    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for p in sorted(src_dir.rglob("*")):
            if p.is_file():
                zf.write(p, p.relative_to(src_dir).as_posix())
        manifest = {
            "sealed_ts": time.time(),
            "not_medical_device": True,
            "disclaimer": "Lab sealed export. Not a medical device. Not for PHI.",
            "meta": meta or {},
        }
        zf.writestr("SEAL_MANIFEST.json", json.dumps(manifest, indent=2))
    sealed = seal_bytes(buf.getvalue(), password)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(sealed)
    return out_path


def unseal_to_directory(seal_path: Path, dest_dir: Path, password: str) -> Path:
    import io

    raw = unseal_bytes(Path(seal_path).read_bytes(), password)
    dest_dir = Path(dest_dir)
    dest_dir.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(io.BytesIO(raw), "r") as zf:
        zf.extractall(dest_dir)
    return dest_dir
