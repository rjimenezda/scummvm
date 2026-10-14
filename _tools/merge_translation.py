#!/usr/bin/env python3
"""Merge translated chunks into a learn_translation.tsv file.

Usage:
    merge_translation.py TEMPLATE.tsv OUTPUT.tsv CHUNK.tsv [CHUNK.tsv ...]

TEMPLATE is the file written by `he_lines.py --tsv` (offset, text, translation).
Each CHUNK holds `offset<TAB>translation` lines. Offsets missing from the chunks
keep an empty translation; offsets not in the template are reported and dropped.
"""

import sys


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    template, output, chunks = sys.argv[1], sys.argv[2], sys.argv[3:]

    rows = open(template, encoding='utf-8').read().splitlines()
    header, rows = rows[0], [r.split('\t') for r in rows[1:]]
    known = {r[0] for r in rows}

    translations, unknown = {}, []
    for chunk in chunks:
        for line in open(chunk, encoding='utf-8').read().splitlines():
            if not line.strip():
                continue
            offset, _, text = line.partition('\t')
            offset = offset.strip()
            if offset not in known:
                unknown.append(f'{chunk}: {line[:60]}')
                continue
            translations[offset] = text.replace('\t', ' ').strip()

    with open(output, 'w', encoding='utf-8') as f:
        f.write(header + '\n')
        for offset, text, _ in rows:
            f.write(f'{offset}\t{text}\t{translations.get(offset, "")}\n')

    missing = [r[0] for r in rows if not translations.get(r[0])]
    print(f'{len(rows) - len(missing)} of {len(rows)} lines translated', file=sys.stderr)
    for offset in missing:
        print(f'  missing: {offset}', file=sys.stderr)
    for line in unknown:
        print(f'  unknown offset: {line}', file=sys.stderr)


if __name__ == '__main__':
    main()
