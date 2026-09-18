from groklink_os.crypto_ed25519 import (
    generate_sk,
    glksig1_ed25519_line,
    publickey,
    verify_glksig1_ed25519,
)


def test_ed25519_roundtrip() -> None:
    sk = generate_sk()
    pk = publickey(sk)
    man = b'{"id":"lab_test","risk":"passive_rx"}\n'
    line = glksig1_ed25519_line(man, sk)
    assert line.startswith("GLKSIG1-ED25519 ")
    assert len(line.split()[1]) == 128
    assert verify_glksig1_ed25519(man, line, pk)
    assert not verify_glksig1_ed25519(man + b"x", line, pk)
