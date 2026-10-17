# ScummVM Learn

**Learn a language by playing classic Humongous Entertainment adventure games, right in your browser.**

▶ **Play it at [rjimenezda.github.io/scummvm-learn](https://rjimenezda.github.io/scummvm-learn/)**

This is a web build of [ScummVM](https://www.scummvm.org/) with an added *learn mode* for the
Humongous games (Freddi Fish, Pajama Sam, Putt-Putt, Spy Fox…). These games are fully voiced and
subtitled, which makes them a good fit for practising a language: short, clear lines, spoken
slowly, in context.

## What learn mode does

- **Pauses after every spoken line.** A panel at the bottom of the screen shows the sentence, with
  buttons to listen to it again or continue.
- **Shows a translation** of the line, which you can hide if you want to test yourself.
- **Fill-in-the-blank exercise.** One word of the line is blanked out in the game's own subtitles,
  and you pick the right word from three choices. You can optionally require the correct answer
  before the game continues.

All of it can be turned on or off per game in ScummVM's game options (Engine tab).

## Demos included

The site comes with four playable demos, in English with Spanish translations:

- Freddi Fish 1: The Case of the Missing Kelp Seeds
- Pajama Sam 1: No Need to Hide When It's Dark Outside
- Putt-Putt Saves the Zoo
- SPY Fox 1: Dry Cereal

Pick one and it starts with learn mode already on.

## Playing your own games

You can also play games you own. Choose **Add a game folder…** and pick the game's folder on your
computer. The files are copied into your browser's storage and **never leave your device**: nothing
is uploaded. ScummVM detects the game and adds it with learn mode turned on, and it's still there
the next time you open the page.

For translations, the game folder needs a `learn_translation.tsv` file. It's a tab-separated list
of every spoken line with its translation:

```
offset	text	translation
8	It's a wooden plank.	Es un tablón de madera.
10888	Pull!	¡Tira!
```

### Making a translation file

The [`_tools`](https://github.com/rjimenezda/scummvm/tree/he-language-learning/_tools) folder of the
source repository has the scripts used to make the demos' translations:

- `he_lines.py GAME_DIR --tsv learn_translation.tsv` lists every voiced line of a game and writes a
  template with an empty translation column. Fill it in and save it in the game folder.
- `merge_translation.py` merges a template with translations done in several chunks, and reports
  missing or unknown lines.

## Source code

This is a modified version of ScummVM. The changes live in a fork:

- **Engine changes (learn mode):**
  [`he-language-learning`](https://github.com/rjimenezda/scummvm/tree/he-language-learning)
  ([compare with ScummVM](https://github.com/scummvm/scummvm/compare/master...rjimenezda:scummvm:he-language-learning))
- **This web build** (the demo page, local game folders and learn mode on by default):
  [`he-web`](https://github.com/rjimenezda/scummvm/tree/he-web)

To build it yourself, check out `he-web` and run:

```sh
./dists/emscripten/build.sh build --disable-all-engines --enable-engine=scumm,he
```

## Credits and license

- [ScummVM](https://www.scummvm.org/) is © the ScummVM Team and is licensed under the
  [GNU GPL v3](https://www.gnu.org/licenses/gpl-3.0.html). This build is too; its full source is in
  the branches above.
- The Humongous Entertainment games and demos are © their respective owners. Only freely
  distributed demos are hosted here.
- Game artwork from [scummvm-icons](https://github.com/scummvm/scummvm-icons).

This is an independent project, not affiliated with or endorsed by the ScummVM Team.
