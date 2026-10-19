#!/usr/bin/env python3
"""List every voiced line of a classic SCUMM v5/v6 talkie (Indiana Jones and the
Fate of Atlantis, Day of the Tentacle, Sam & Max).

Scans the room/script file (e.g. TENTACLE.001) for talkie codes and pairs each
one with the text of its line.

Usage:
    scumm_lines.py GAME_DIR [-o lines.csv] [--tsv learn_translation.tsv]
    scumm_lines.py GAME_DIR --translate OTHER_GAME_DIR --tsv learn_translation.tsv

--tsv writes a translation template for ScummVM's learn mode: tab separated
offset, text, translation. With --translate, the translation column is filled
with the text another language version of the game uses for the same recording
(both versions must share the speech file, as the Spanish and English DOTT do).
When the other version has no speech at all (e.g. the Spanish floppy Indy4), its
lines are matched by their place in the game's scripts instead.

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
import math
import re
import struct
import sys
from collections import Counter, OrderedDict, defaultdict
from pathlib import Path

XOR_KEY = 0x69

# Argument sizes of the 0xFF/0xFE control codes in SCUMM v5/v6 strings
CODE_ARGS = {4: 2, 5: 2, 6: 2, 7: 2, 9: 2, 10: 14, 12: 2, 13: 2, 14: 3}

# Indy4 CD separates many words with 0x10, drawn as a space
WORD_SEPARATOR = 0x10


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
        elif c == WORD_SEPARATOR:
            text += b' '
        pos += 1
    return clean_text(bytes(text))


def clean_text(text):
    # DOS games use code page 850; the font draws '^' as an ellipsis and '`' as a quotation mark
    return ' '.join(text.decode('cp850').replace('^', '...').replace('`', '"').split())


def load(game_dir):
    return bytes(b ^ XOR_KEY for b in find_data_file(game_dir).read_bytes())


def scan(game_dir):
    data = load(game_dir)
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


# Matching by script position, for a version of the game without speech

CONTAINERS = {b'LECF', b'LFLF', b'ROOM', b'OBCD'}


def walk_blocks(data, pos, end, out):
    while pos + 8 <= end:
        tag = data[pos:pos + 4]
        size = struct.unpack('>I', data[pos + 4:pos + 8])[0]
        if size < 8 or pos + size > end:
            return
        out.append((tag, pos, size))
        if tag in CONTAINERS:
            walk_blocks(data, pos + 8, pos + size, out)
        pos += size


def parse_string(data, pos, end):
    """Parses a script string. Returns its parts and its end, or None if it is no text.

    Each part is (talkie offset or None, text). Parts start at a talkie code
    (speech versions) or after a wait code (versions without speech, which wait
    at the same places)."""
    parts = [[None, bytearray()]]
    letters = 0
    while pos < end:
        c = data[pos]
        if c == 0:
            break
        if c in (0xFF, 0xFE):
            code = data[pos + 1] if pos + 1 < end else 0
            if code not in (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14):
                return None
            if code == 10:
                a = data[pos + 2:pos + 16]
                if len(a) < 14 or a[2:4] != b'\xff\x0a':
                    return None
                offset = a[0] | a[1] << 8 | a[4] << 16 | a[5] << 24
                # A talkie code right after a wait code starts the part the wait began
                if parts[-1][0] is None and not parts[-1][1].strip():
                    parts[-1][0] = offset
                else:
                    parts.append([offset, bytearray()])
            elif code == 3:
                parts.append([None, bytearray()])
            elif code == 1:
                parts[-1][1] += b' '
            pos += 2 + CODE_ARGS.get(code, 0)
            continue
        if c < 32 and c != WORD_SEPARATOR:
            return None
        if chr(c).isalpha() or c >= 0x80:
            letters += 1
        parts[-1][1].append(32 if c < 32 else c)
        pos += 1
    else:
        return None
    if letters < 2:
        return None
    return [(o, clean_text(bytes(t))) for o, t in parts if o is not None or t.strip()], pos


def is_string_start(data, pos, start):
    # print/printEgo text, or stringOps loadString with a byte or a variable (dialog choices)
    return ((pos - 1 >= start and data[pos - 1] == 0x0F) or
            (pos - 3 >= start and data[pos - 3] == 0x27 and data[pos - 2] == 0x01) or
            (pos - 4 >= start and data[pos - 4] == 0x27 and data[pos - 3] == 0x81))


def script_strings(game_dir):
    """Returns the strings of each script, keyed by room and script/object number."""
    data = load(game_dir)
    tree = []
    walk_blocks(data, 0, len(data), tree)
    result = OrderedDict()
    room, global_script, obj = -1, 0, None
    for tag, pos, size in tree:
        if tag == b'LFLF':
            room += 1
            global_script = 0
        elif tag == b'CDHD':
            obj = struct.unpack('<H', data[pos + 8:pos + 10])[0]
        if tag == b'SCRP':
            key = (room, 'SCRP', global_script)
            global_script += 1
        elif tag == b'LSCR':
            key = (room, 'LSCR', data[pos + 8])
        elif tag in (b'ENCD', b'EXCD'):
            key = (room, tag.decode(), 0)
        elif tag == b'VERB':
            key = (room, 'VERB', obj)
        else:
            continue
        strings = []
        p, end = pos + 8, pos + size
        while p < end:
            parsed = is_string_start(data, p, pos + 8) and parse_string(data, p, end)
            if parsed:
                strings.append(parsed[0])
                p = parsed[1] + 1
            else:
                p += 1
        result[key] = strings
    return result


def joined(parts):
    return ' '.join(t for _, t in parts)


def words(text):
    return set(re.findall(r'[^\W\d_]+|\d+', text.lower()))


class Lexicon:
    """Word associations (Dice coefficient) learnt from string pairs known to match."""

    def __init__(self, pairs):
        mine, theirs, both = Counter(), Counter(), Counter()
        for a, b in pairs:
            wa, wb = words(joined(a)), words(joined(b))
            mine.update(wa)
            theirs.update(wb)
            both.update((x, y) for x in wa for y in wb)
        self.dice = defaultdict(dict)
        for (x, y), n in both.items():
            if n >= 2:
                self.dice[x][y] = 2 * n / (mine[x] + theirs[y])

    def score(self, a, b):
        """0..1: how well the words of a are explained by the words of b."""
        wa, wb = words(joined(a)), words(joined(b))
        if not wa or not wb:
            return 0.0
        return sum(max(self.dice[x].get(y, 1.0 if x == y else 0.0) for y in wb) for x in wa) / len(wa)

    def similarity(self, a, b):
        """How likely two strings of different languages are the same line."""
        ta, tb = joined(a), joined(b)
        if ta == tb:
            return 5.0
        score = 6 * self.score(a, b) - 1.0
        score += 0.5 if len(a) == len(b) else -1.0
        score += max(-1.5, 1.0 - abs(math.log(max(len(tb), 1) / max(len(ta), 1) / 1.1)) * 2)
        return score


def align(a, b, similarity, gap=-0.5):
    """Needleman-Wunsch alignment of two string lists. Returns (index in a, index in b or None)."""
    n, m = len(a), len(b)
    score = [[0.0] * (m + 1) for _ in range(n + 1)]
    move = [[''] * (m + 1) for _ in range(n + 1)]
    for i in range(1, n + 1):
        score[i][0], move[i][0] = i * gap, 'a'
    for j in range(1, m + 1):
        score[0][j], move[0][j] = j * gap, 'b'
    for i in range(1, n + 1):
        for j in range(1, m + 1):
            score[i][j], move[i][j] = max((score[i - 1][j - 1] + similarity(a[i - 1], b[j - 1]), 'd'),
                                          (score[i - 1][j] + gap, 'a'), (score[i][j - 1] + gap, 'b'))
    pairs, i, j = [], n, m
    while i or j:
        if move[i][j] == 'd':
            pairs.append((i - 1, j - 1))
            i, j = i - 1, j - 1
        elif move[i][j] == 'a':
            pairs.append((i - 1, None))
            i -= 1
        else:
            j -= 1
    return pairs[::-1]


def translate_by_position(game_dir, other_dir):
    """Maps talkie offsets to the text the other version shows at the same place."""
    mine, other = script_strings(game_dir), script_strings(other_dir)
    # Scripts with as many strings in both versions hardly ever differ: they teach
    # the lexicon used to align the others
    same = [(k, v) for k, v in mine.items() if len(v) == len(other.get(k, []))]
    lexicon = Lexicon(pair for k, v in same for pair in zip(v, other.get(k, [])))
    found = defaultdict(Counter)
    for key, strings in mine.items():
        theirs = other.get(key, [])
        if len(strings) == len(theirs):
            pairs = [(i, i) for i in range(len(strings))]
        else:
            pairs = align(strings, theirs, lexicon.similarity)
        for i, j in pairs:
            if j is None:
                continue
            a, b = strings[i], theirs[j]
            texts = defaultdict(list)
            if len(a) == len(b):
                # A recording goes on over wait codes until the next talkie code
                offset = None
                for (part_offset, _), (_, text) in zip(a, b):
                    offset = part_offset if part_offset is not None else offset
                    if offset is not None and text:
                        texts[offset].append(text)
            else:
                # Without the same parts, every recording of the string gets all of its text
                for offset, _ in a:
                    if offset is not None:
                        texts[offset] = [joined(b)]
            for offset, parts in texts.items():
                found[offset][' '.join(parts)] += 1
    return {offset: [texts.most_common(1)[0][0]] for offset, texts in found.items()}


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
        if args.translate and not other:
            print(f'No speech in {args.translate}, matching lines by script position', file=sys.stderr)
            other = {o: {'texts': t} for o, t in translate_by_position(args.game_dir, args.translate).items()}
        translations = {o: ' || '.join(other[o]['texts']) for o in lines if o in other}
        # Other recordings of the same sentence share its translation
        by_text = {' || '.join(lines[o]['texts']): t for o, t in translations.items() if t}
        reused = 0
        with open(args.tsv, 'w', encoding='utf-8') as tsv:
            for offset, entry in lines.items():
                text = ' || '.join(entry['texts'])
                translation = translations.get(offset, '')
                if not translation and text in by_text:
                    translation = translations[offset] = by_text[text]
                    reused += 1
                tsv.write(f'{offset}\t{text}\t{translation}\n')
        if args.translate:
            matched = sum(1 for t in translations.values() if t)
            missing = [o for o, e in lines.items() if e['texts'] and not translations.get(o)]
            extra = [o for o in other if o not in lines]
            print(f'{matched} lines translated ({reused} from another recording of the same text), '
                  f'{len(missing)} without a match, {len(extra)} only in {args.translate}', file=sys.stderr)

if __name__ == '__main__':
    main()
