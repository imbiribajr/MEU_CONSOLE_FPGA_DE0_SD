import struct
import wave
from pathlib import Path


def wav_to_c(src_path: Path, out_path: Path, symbol: str) -> None:
    with wave.open(str(src_path), "rb") as wav:
        channels = wav.getnchannels()
        sampwidth = wav.getsampwidth()
        rate = wav.getframerate()
        frames = wav.readframes(wav.getnframes())

    if sampwidth != 2:
        raise SystemExit("Only 16-bit PCM WAV supported")
    if channels not in (1, 2):
        raise SystemExit("Only mono or stereo WAV supported")

    samples = struct.unpack("<" + "h" * (len(frames) // 2), frames)
    if channels == 1:
        left = list(samples)
        right = list(samples)
    else:
        left = list(samples[0::2])
        right = list(samples[1::2])

    with out_path.open("w", encoding="utf-8") as out:
        out.write("#ifndef SFX_SAMPLES_H\n")
        out.write("#define SFX_SAMPLES_H\n\n")
        out.write("#include <stdint.h>\n\n")
        out.write(f"// Source: {src_path.name} ({rate} Hz, {channels} ch)\n")
        out.write(f"static const int16_t {symbol}_L[] = {{\n")
        for i, s in enumerate(left):
            if i % 12 == 0:
                out.write("    ")
            out.write(f"{s}, ")
            if i % 12 == 11:
                out.write("\n")
        if len(left) % 12 != 0:
            out.write("\n")
        out.write("};\n\n")
        out.write(f"static const int16_t {symbol}_R[] = {{\n")
        for i, s in enumerate(right):
            if i % 12 == 0:
                out.write("    ")
            out.write(f"{s}, ")
            if i % 12 == 11:
                out.write("\n")
        if len(right) % 12 != 0:
            out.write("\n")
        out.write("};\n\n")
        out.write(f"#define {symbol}_LEN {len(left)}u\n")
        out.write("\n#endif\n")


if __name__ == "__main__":
    wav = Path("sons/encaixe.wav")
    out = Path("software/app/sfx_samples.h")
    wav_to_c(wav, out, "SFX_LOCK")
