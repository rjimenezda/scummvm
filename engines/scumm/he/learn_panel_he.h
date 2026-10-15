/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef SCUMM_HE_LEARN_PANEL_HE_H
#define SCUMM_HE_LEARN_PANEL_HE_H

#include "common/array.h"
#include "common/rect.h"
#include "common/ustr.h"
#include "graphics/surface.h"

#include "scumm/he/learn_exercise_he.h"

class OSystem;

namespace Graphics {
class Font;
}

namespace Scumm {

// On-screen panel shown while learn mode is paused after a voice line:
// a replay button, the line's translation, an optional fill-in-the-blank
// exercise and a continue button.
class LearnPanel {
public:
	enum Button {
		kButtonNone = -1,
		kButtonReplay = -2,
		kButtonContinue = -3
		// Values >= 0 are exercise choices
	};

	LearnPanel(OSystem *system, const Graphics::Font *font, const Graphics::Font *hintFont);
	~LearnPanel();

	void show(const Common::U32String &translation, const LearnExercise *exercise);
	void hide();

	int buttonAt(const Common::Point &pos) const;
	void setHovered(int button);

	bool hasExercise() const { return _hasExercise; }
	// Returns true if the choice is the right answer
	bool choose(int choice);

private:
	struct Colors {
		uint32 panel, replay, replayHover, cont, contHover, choice, choiceHover, wrong, right, icon, text;
	};

	OSystem *_system;
	const Graphics::Font *_font;
	const Graphics::Font *_hintFont;

	Common::U32String _translation;
	LearnExercise _exercise;
	bool _hasExercise = false;
	bool _solved = false;
	bool _wrong[LearnExercise::kNumChoices];

	Common::Array<Common::U32String> _sentenceLines;
	Common::Array<Common::U32String> _translationLines;
	Common::Rect _box, _textArea, _replayButton, _continueButton;
	Common::Rect _choiceButtons[LearnExercise::kNumChoices];
	int _sentenceTop = 0, _translationTop = 0;

	Graphics::Surface _backup;
	int _hovered = kButtonNone;
	bool _visible = false;
	bool _cursorWasVisible = false;

	void showCursor();
	void restoreCursor();
	Common::U32String sentenceWithBlank() const;
	const Graphics::Font *translationFont() const;
	void layout(int screenWidth, int screenHeight);
	Colors getColors(const Graphics::Surface &screen) const;
	void draw();
	void drawSpeaker(Graphics::Surface &screen, const Common::Rect &r, uint32 color) const;
	void drawPlay(Graphics::Surface &screen, const Common::Rect &r, uint32 color) const;
	void drawHint(Graphics::Surface &screen, const Common::Rect &button, const char *hint, uint32 color) const;
};

} // End of namespace Scumm

#endif
