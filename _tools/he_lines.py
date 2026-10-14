#!/usr/bin/env python3
"""List every voiced line of a Humongous (SCUMM HE) game.

Scans the room/script file (.he1) for talkie codes, pairs each one with the
sentence that follows it, and optionally exports the matching audio from the
speech file (.he2) as WAV.

Usage:
    he_lines.py GAME_DIR [-o lines.csv] [--wav OUT_DIR]

CSV columns: offset, length, uses, text
  offset  byte offset of the TALK block in the .he2 file (the line's stable ID,
          same value ScummVM uses for audio_override files)
  length  size of the TALK block
  uses    how many times scripts reference this recording
  text    subtitle text; variants are joined with " || " when scripts use
          different text for the same recording
"""

import argparse
import csv
import re
import struct
import sys
import wave
from collections import OrderedDict
from pathlib import Path

XOR_KEY = 0x69

# Argument sizes of 0xFF control codes in HE60-71 strings
CLASSIC_CODE_ARGS = {9: 2, 10: 14, 12: 2, 13: 2, 14: 3}

MODERN_TALKIE = re.compile(rb'([@\x7f])T(\d+),(\d+)\1')


def find_file(game_dir, suffix):
    matches = [p for p in game_dir.iterdir() if p.suffix.lower() == suffix]
    if not matches:
        sys.exit(f'No *{suffix} file in {game_dir}')
    return matches[0]


def load(path, magic):
    data = path.read_bytes()
    if data[:4] == magic:
        return data, False
    decoded = bytes(b ^ XOR_KEY for b in data)
    if decoded[:4] != magic:
        sys.exit(f'{path.name}: unexpected header {data[:4]!r}')
    return decoded, True


def read_text(data, pos, classic, code_char=None):
    out = bytearray()
    while pos < len(data) and data[pos] != 0:
        c = data[pos]
        if classic and c == 0xFF:
            code = data[pos + 1]
            if code == 1:
                out += b' '
            pos += 2 + CLASSIC_CODE_ARGS.get(code, 0)
            continue
        if not classic and c == code_char:
            code = data[pos + 1]
            if code == ord('n'):
                out += b' '
            pos += 2
            continue
        out.append(c)
        pos += 1
    return out.decode('latin-1').strip()


def scan_classic(data):
    pos = 0
    while (pos := data.find(b'\xff\x0a', pos)) >= 0:
        b = data[pos + 2:pos + 16]
        offset = b[0] | b[1] << 8 | b[4] << 16 | b[5] << 24
        length = b[8] | b[9] << 8 | b[12] << 16 | b[13] << 24
        yield offset, length, read_text(data, pos + 16, True)
        pos += 2


def scan_modern(data):
    for m in MODERN_TALKIE.finditer(data):
        yield int(m[2]), int(m[3]), read_text(data, m.end(), False, m[1][0])


def is_talk_block(speech, offset):
    return speech[offset:offset + 4] == b'TALK'


def count_talk_blocks(speech):
    count, pos = 0, 8
    while is_talk_block(speech, pos):
        count += 1
        pos += struct.unpack('>I', speech[pos + 4:pos + 8])[0]
    return count


def export_wav(speech, offset, path):
    size = struct.unpack('>I', speech[offset + 4:offset + 8])[0]
    block = speech[offset:offset + size]
    rate = struct.unpack('<H', block[22:24])[0]
    pos = 8
    while pos < len(block):
        tag = block[pos:pos + 4]
        n = struct.unpack('>I', block[pos + 4:pos + 8])[0]
        if tag == b'SDAT':
            with wave.open(str(path), 'wb') as w:
                w.setnchannels(1)
                w.setsampwidth(1)
                w.setframerate(rate)
                w.writeframes(block[pos + 8:pos + n])
            return rate, (n - 8) / rate
        pos += n
    return rate, 0.0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('game_dir', type=Path)
    ap.add_argument('-o', '--output', type=Path, help='CSV file (default: stdout)')
    ap.add_argument('--wav', type=Path, help='directory to export <offset>.wav files into')
    args = ap.parse_args()

    rooms, _ = load(find_file(args.game_dir, '.he1'), b'LECF')
    speech, _ = load(find_file(args.game_dir, '.he2'), b'TLKB')

    found = list(scan_classic(rooms)) + list(scan_modern(rooms))
    lines = OrderedDict()
    skipped = 0
    for offset, length, text in found:
        if not is_talk_block(speech, offset):
            skipped += 1
            continue
        entry = lines.setdefault(offset, {'length': length, 'uses': 0, 'texts': []})
        entry['uses'] += 1
        if text and text not in entry['texts']:
            entry['texts'].append(text)

    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
    out = open(args.output, 'w', newline='', encoding='utf-8') if args.output else sys.stdout
    writer = csv.writer(out)
    writer.writerow(['offset', 'length', 'uses', 'text'])
    for offset, e in sorted(lines.items()):
        writer.writerow([offset, e['length'], e['uses'], ' || '.join(e['texts'])])
    if args.output:
        out.close()

    if args.wav:
        args.wav.mkdir(parents=True, exist_ok=True)
        for offset in lines:
            export_wav(speech, offset, args.wav / f'{offset}.wav')

    no_text = sum(1 for e in lines.values() if not e['texts'])
    print(f'{len(found) - skipped} talkie references, {len(lines)} of {count_talk_blocks(speech)} '
          f'recordings referenced, {no_text} without text', file=sys.stderr)


if __name__ == '__main__':
    main()
