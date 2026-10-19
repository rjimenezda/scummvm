#!/usr/bin/env python3
"""List every voiced line of a classic SCUMM v6 talkie (Day of the Tentacle, Sam & Max).

Scans the room/script file (e.g. TENTACLE.001) for talkie codes and pairs each
one with the text of its line.

Usage:
    scumm_lines.py GAME_DIR [-o lines.csv] [--tsv learn_translation.tsv]
    scumm_lines.py GAME_DIR --translate OTHER_GAME_DIR --tsv learn_translation.tsv

--tsv writes a translation template for ScummVM's learn mode: tab separated
offset, text, translation. With --translate, the translation column is filled
with the text another language version of the game uses for the same recording
(both versions must share the speech file, as the Spanish and English DOTT do).

CSV columns: offset, length, uses, text
  offset  byte offset of the line in the original MONSTER.SOU (the line's stable
          ID, also used by compressed .so3/.sog/.sof files)
  length  size of the line's mouth sync data
  uses    how many times scripts reference this recording
  text    subtitle text; variants are joined with " || " when scripts use
          different text for the same recording
"""

import argparse
import csv
import sys
from collections import OrderedDict
from pathlib import Path

XOR_KEY = 0x69

# Argument sizes of the 0xFF/0xFE control codes in SCUMM v6 strings
CODE_ARGS = {4: 2, 5: 2, 6: 2, 7: 2, 9: 2, 10: 14, 12: 2, 13: 2, 14: 3}


def find_data_file(game_dir):
    for path in sorted(game_dir.iterdir()):
        if path.suffix == '.001' and path.is_file():
            return path
    sys.exit(f'No .001 file in {game_dir}')


def read_line(data, pos):
    """Reads a talk string from just after its talkie code. Returns its text."""
    text = bytearray()
    while pos < len(data) and data[pos]:
        c = data[pos]
        if c in (0xFF, 0xFE) and pos + 1 < len(data):
            code = data[pos + 1]
            if code in (1, 3):  # New line, wait for the next part of the line
                text += b' '
            elif code == 10:  # Another talkie code: a new line starts
                break
            pos += 2 + CODE_ARGS.get(code, 0)
            continue
        if c >= 32:
            text.append(c)
        pos += 1

    # DOS games use code page 850; the font draws '^' as an ellipsis and '`' as a quotation mark
    return ' '.join(text.decode('cp850').replace('^', '...').replace('`', '"').split())


def scan(game_dir):
    data = bytes(b ^ XOR_KEY for b in find_data_file(game_dir).read_bytes())
    lines = OrderedDict()
    pos = data.find(b'\xff\x0a')
    while pos >= 0:
        a = data[pos + 2:pos + 16]
        # The 32-bit offset and length are split in 16-bit halves, separated by 0xFF 0x0A
        if len(a) == 14 and a[2:4] == a[6:8] == a[10:12] == b'\xff\x0a':
            offset = a[0] | a[1] << 8 | a[4] << 16 | a[5] << 24
            length = a[8] | a[9] << 8 | a[12] << 16 | a[13] << 24
            text = read_line(data, pos + 16)
            entry = lines.setdefault(offset, {'length': length, 'uses': 0, 'texts': []})
            entry['uses'] += 1
            if text and text not in entry['texts']:
                entry['texts'].append(text)
            pos += 16
        else:
            pos += 2
        pos = data.find(b'\xff\x0a', pos)
    return OrderedDict(sorted(lines.items()))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('game_dir', type=Path)
    parser.add_argument('-o', '--output', type=Path, help='CSV file (default: stdout)')
    parser.add_argument('--tsv', type=Path, help='learn mode translation file to write')
    parser.add_argument('--translate', type=Path, metavar='OTHER_GAME_DIR',
                        help='fill the translations with the text of another language version')
    args = parser.parse_args()

    lines = scan(args.game_dir)
    print(f'{len(lines)} recordings in {args.game_dir}', file=sys.stderr)

    out = open(args.output, 'w', newline='', encoding='utf-8') if args.output else sys.stdout
    writer = csv.writer(out)
    writer.writerow(['offset', 'length', 'uses', 'text'])
    for offset, entry in lines.items():
        writer.writerow([offset, entry['length'], entry['uses'], ' || '.join(entry['texts'])])
    if args.output:
        out.close()

    if args.tsv:
        other = scan(args.translate) if args.translate else {}
        matched = 0
        with open(args.tsv, 'w', encoding='utf-8') as tsv:
            for offset, entry in lines.items():
                text = ' || '.join(entry['texts'])
                translation = ' || '.join(other[offset]['texts']) if offset in other else ''
                if translation:
                    matched += 1
                tsv.write(f'{offset}\t{text}\t{translation}\n')
        if args.translate:
            missing = [o for o, e in lines.items() if e['texts'] and o not in other]
            extra = [o for o in other if o not in lines]
            print(f'{matched} lines translated, {len(missing)} without a match, '
                  f'{len(extra)} only in {args.translate}', file=sys.stderr)


if __name__ == '__main__':
    main()
