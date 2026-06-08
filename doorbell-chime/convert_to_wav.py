from __future__ import annotations

import subprocess
import sys
import wave
from pathlib import Path

import imageio_ffmpeg


SAMPLE_RATE = 44100
CHANNELS = 1
SAMPLE_WIDTH = 2  # 16-bit


def convert_audio(input_path: Path, output_path: Path, start: float, duration: float) -> None:
    ffmpeg = imageio_ffmpeg.get_ffmpeg_exe()

    command = [
        ffmpeg,
        "-hide_banner",
        "-loglevel", "error",
        "-i", str(input_path),
        "-ss", str(start),
        "-t", str(duration),
        "-vn",
        "-ac", str(CHANNELS),
        "-ar", str(SAMPLE_RATE),
        "-acodec", "pcm_s16le",
        "-f", "s16le",
        "pipe:1",
    ]

    try:
        result = subprocess.run(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=True,
        )
    except subprocess.CalledProcessError as error:
        message = error.stderr.decode("utf-8", errors="replace")
        raise RuntimeError(f"Audio conversion failed:\n{message}") from error

    pcm = result.stdout

    if not pcm:
        raise RuntimeError("Decoder produced no audio data")

    if len(pcm) % SAMPLE_WIDTH != 0:
        raise RuntimeError("Decoder produced incomplete PCM samples")

    with wave.open(str(output_path), "wb") as output:
        output.setnchannels(CHANNELS)
        output.setsampwidth(SAMPLE_WIDTH)
        output.setframerate(SAMPLE_RATE)
        output.writeframes(pcm)


def main() -> int:
    if len(sys.argv) != 5:
        print(
            f"Usage: {Path(sys.argv[0]).name} input_path output_path start_second duration",
            file=sys.stderr,
        )
        return 2

    try:
        convert_audio(Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3], sys.argv[4])
    except Exception as error:
        print(error, file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())