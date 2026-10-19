# Learn mode for Humongous (SCUMM HE) games — handoff notes

Private fork of ScummVM, branch `he-language-learning` (6 commits on top of `master`, nothing pushed).
Goal: turn Humongous kids' adventures (Freddi Fish, Putt-Putt, Pajama Sam, Spy Fox…) into
English-listening practice for Spanish-speaking kids.

## Working conventions
- macOS arm64, Xcode clang, Homebrew deps. Build: `./configure --enable-debug && make -j8` (already configured; just `make -j8`).
- Run from repo root (game paths in the config are relative):
  `./scummvm --extrapath=dists/engine-data -d1 --subtitles <target> 2>&1 | grep Sound`
  (`--extrapath` is needed for the NotoSans TTF in `fonts.dat`; `-d1` shows the `Sound:`/`SoundHE:` debug traces.)
- The game can't be run headless here (no display), so in-game testing is done by the user.
- Commits: ScummVM `AI-GUIDELINES.md` → use `Assisted-by: Claude Code:claude-opus-5-5`, never an AI co-author line.
  Prefix messages `SCUMM: HE: …`. Commit only when the user asks.
- Game data lives in `_games/` (git-ignored). Exports/translations in `_games/_out/<game>/`.
- Translations: Castilian Spanish (Spain, "vosotros"), for kids. Done with 8 parallel background agents
  (each reads one chunk and writes one `chunkN.es.tsv`), then merged with `_tools/merge_translation.py`.
  Chunks live in `_games/_out/<game>/es/` (`chunkN.tsv` source, `chunkN.es.tsv` output). The merged
  template is `_games/_out/<game>/learn_translation.tsv`; merge output goes to `learn_translation.es.tsv`,
  then copy into the game folder. Pajama's old lorem ipsum file is `_games/_out/pajama1/lorem_backup.tsv`.
  Agents sometimes leave a line without a tab (offset only); check with `awk -F'\t' 'NF!=2'` and retranslate.
  Lines with no source text stay empty; the merge reports them as "missing", which is expected.

## Games set up (`~/Library/Preferences/ScummVM Preferences`)
| Target | Folder | Engine | Screen | Translation file |
|---|---|---|---|---|
| `freddi-win` | `_games/freddi1` | HE 71 | 640×480 | Spanish (1240 lines) |
| `puttputt-win` | `_games/putt1` | HE 70 | 320×200 | Spanish (1215 lines) |
| `pajama-win` | `_games/pajama1` | HE 100 ("Updated" re-release) | 640×480 | Spanish (2717 of 2730 lines; the 13 empty ones have no subtitle text) |
| `spyfox` (add via launcher) | `_games/spyfox1` | HE 90 | 640×480 | Spanish (5087 lines, untested in game) |
| `tentacle` (add via launcher) | `_games/dott_en/dott` | SCUMM v6 (not HE), English CD, FLAC `monster.sof` | 320×200 | Spanish, taken from `dott_es` (4439 lines) |
| (source only) | `_games/dott_es` | SCUMM v6, Spanish CD (Spanish text, English voices) | 320×200 | — |

## What's built
- **Pause after each voiced line** (`learn_pause`): when the talkie voice ends (`HSND_SOUND_ENDED` on
  `HSND_TALKIE_SLOT` in `SoundHE::digitalSoundCallback`), `stopTalk()` is deferred and
  `ScummEngine_v60he::runLearnPause()` runs from the `go()` loop after `feedMixer()`. It uses `pauseEngine()`.
  Lines without text (yawns…) don't pause. HE95+ early callbacks: the remaining audio is let play out first.
- **Replay**: re-reads the TALK block from the .he2 file and plays its SDAT on a separate mixer handle.
- **Panel** (`he/learn_panel_he.{h,cpp}`): bottom of screen; 🔊 replay (R), sentence, translation hint,
  ▶ continue (Space/Enter), small speech-bubble toggle (T) that hides the translation for that line only.
  Own arrow cursor forced visible. Clicks outside buttons ignored. Sizes scale with screen width (`px()`,
  designed for 640); fonts: NotoSans-Bold 20/11 px, hints GUI font / console font at 320 px.
- **Translations** (`learn_translate`): `learn_translation.tsv` in the game folder (or `learn_translation_file`),
  tab-separated `offset<TAB>original<TAB>translation`, keyed by the voice's byte offset in the .he2 file.
- **Fill-in-the-blank exercise** (`learn_exercise`, `learn_exercise_mode=choice`; typing mode not built yet):
  `he/learn_exercise_he.{h,cpp}`. In `ScummEngine::actorTalk` → `SoundHE::prepareTalk()` extracts the text and
  voice offset from the talk string (`extractLearnText`, handles HE60-71 binary 0xFF codes and HE72+ `@`/0x7F
  codes), picks one word seeded by the voice offset (skips filler words, <3 letters, names, lines <3 words),
  blanks it with spaces in the game's own subtitle, and keeps the exercise for the panel: 3 word buttons
  (keys 1-3), decoys from a pool of words seen in other lines/the TSV. Wrong = red, right = green + fills gap.
  `learn_exercise_mode=choice_required` ("Multiple choice (answer to continue)") greys out ▶ and ignores
  Space/Enter until the right word is picked (`LearnPanel::canContinue()`); lines without an exercise aren't locked.
- **Launcher options** (Engine tab, HE games only): pause / translations / exercise + mode dropdown, wired in
  `ScummGameOptionsWidget` (`dialogs.cpp`) with greying when pause is off. Defined in `metaengine.cpp`.

## Classic SCUMM v6 talkies (Day of the Tentacle, Sam & Max)
- The pause loop is `ScummEngine::runLearnPause()` (base engine, not HE-only). The learn state (pending flag,
  talk/voice text and exercise, last voice offset/length, translations, word pool), `prepareTalk()` and
  `getLastVoiceExercise()` live in `Sound`. `startLearnVoice()` turns the prepared line into the paused one:
  called from `SoundHE::playVoice()` and, for v6, `Sound::talkSound()`. `SoundHE` only overrides
  `replayLastVoice` and `getLearnPauseDelay`.
- v6 voice lines end in `Sound::processSfxQueues()`: with `learn_pause`, `stopTalk()` is replaced by setting
  `_learnPausePending` (as soon as the voice ends, without waiting for the subtitle timer).
- Line IDs are offsets into the original MONSTER.SOU (compressed .sof/.so3/.sog keep them via their offset table).
- `prepareTalk()` for v6: CP850 → Latin-1 and '`' → '"' (the LucasArts font draws it as a quote), length-preserving
  so exercise blanking positions stay valid.
- Replay calls `startTalkSound()` on `_voiceReplayHandle` with `_soundsPaused` briefly cleared.
- Launcher: pause, translations and exercise for `tentacle`/`samnmax` (exercise only makes sense when
  text and voice share a language, i.e. the English copy).
- Workflow: the Spanish DOTT CD has the same speech file as the English one, so lines match by offset.
  `scumm_lines.py _games/dott_en/dott -o _games/_out/dott/lines_en.csv --translate _games/dott_es --tsv _games/_out/dott/learn_translation.tsv`,
  then copy the TSV into `_games/dott_en/dott/`. The 91 unmatched offsets have no text in either version.

## Tools (`_tools/`)
- `scumm_lines.py GAME_DIR [-o lines.csv] [--tsv file.tsv] [--translate OTHER_GAME_DIR]`: same for SCUMM v6
  talkies (reads the XOR'd `.001`); `--translate` fills the translations from another language version.
- `he_lines.py GAME_DIR [-o lines.csv] [--wav DIR] [--tsv template.tsv]`: lists every voiced line
  (offset, length, uses, text), exports WAVs, writes a translation template. Handles XOR 0x69, `.he1`/`.(a)`,
  both talkie formats, `^`→`...`, tabs→spaces. Recording variants with different text are joined by ` || `.
- `merge_translation.py TEMPLATE OUTPUT CHUNK...`: merges translated chunks, reports missing/unknown offsets.

## Gotchas learned
- HE60-71 talkie code payload is binary and contains zero bytes: skip it by length, never scan to NUL.
- In HE fonts `_` and `-` are drawn at odd heights (Freddi's `_` sits at the top of the line); use spaces.
- `^` is drawn as an ellipsis by HE fonts.
- HE < 71 runs at 320×200.
- Some recordings are spliced sentence fragments ("...three..."), so their translations may read oddly.

- Spy Fox 3 (`spyozon`) is HE **99** (`GF_HE_995`), so it runs the HE 90 opcodes, not the `o100_*` ones.
  16-bit games don't draw the talk string; SF3 also stores each line in string array `var 313`
  (`o72_arrayOps` `SO_STRING`) and a script draws it char by char (`readArray`) as sprites. The word is
  blanked in that stored copy. Lines carry `[SPYFOX.8BF]` tags (`GF_HE_LOCALIZED`), which `extractLearnText` skips.

## Batch translation status (2026-10-17)
Game data for all games is in `/Users/roman/scummvm/humongous/<Game>/`; each finished `learn_translation.tsv`
is installed there (and in `_games/_out/<game>/learn_translation.es.tsv`).
- Done (all 17 main adventures): Freddi Fish 1–5, Pajama Sam 1–3, Putt-Putt 1, 2, Zoo, 4, 5, 6, Spy Fox 1–3.
- Skipped on purpose: mini-game titles (<1k lines), Pajama Sam GTPOAD (no voiced text), Pajama Sam 4 / Putt-Putt 7
  (non-SCUMM engine).
- Workflow: `_tools/translation_setup.sh <out> <Template_Name>` splits into 8 chunks → 8 background agents (one per
  chunk, Castilian Spanish, drop `(PU1)`/`[NAME.123]` markers, translate stage directions) →
  `_tools/translation_finish.sh <out> "<Humongous Folder>"` validates, strips markers, merges and installs.
- Newer games prefix lines with `[NAME.123]` tags and older ones with `(PU1)`; they stay in the English column.
  Pajama Sam 3 has a raw `\x85` (cp1252 "…") in one line; `merge_translation.py` now splits on `\n` only.

## Open ideas / next steps
- User-test Pajama Sam with the Spanish file (watch for scripts acting behind the panel,
  lines not pausing, lip-sync after resume, whether the translations fit the voice timing).
- Freddi and Putt-Putt translations are merged; Pajama Sam is done (commit: see git log).
- Typing exercise mode (add to the dropdown in `dialogs.cpp`, input field in the panel).
- Spy Fox 3: subtitle blanking works; no translation file yet.
- Remove the `SoundHE:` debug traces before any cleanup/upstream attempt.
