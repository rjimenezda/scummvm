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

class OSystem;

namespace Graphics {
class Font;
}

namespace Scumm {

// On-screen panel shown while learn mode is paused after a voice line:
// a replay button, the line's translation and a continue button.
class LearnPanel {
public:
	enum Button {
		kButtonNone,
		kButtonReplay,
		kButtonContinue
	};

	LearnPanel(OSystem *system, const Graphics::Font *font, const Graphics::Font *hintFont);
	~LearnPanel();

	void show(const Common::U32String &text);
	void hide();

	Button buttonAt(const Common::Point &pos) const;
	void setHovered(Button button);

private:
	struct Colors {
		uint32 panel, replay, replayHover, cont, contHover, icon, text;
	};

	OSystem *_system;
	const Graphics::Font *_font;
	const Graphics::Font *_hintFont;
	Common::Array<Common::U32String> _lines;
	Common::Rect _box, _textArea, _replayButton, _continueButton;
	Graphics::Surface _backup;
	Button _hovered = kButtonNone;
	bool _visible = false;
	bool _cursorWasVisible = false;

	void showCursor();
	void restoreCursor();
	void layout(int screenWidth, int screenHeight, const Common::U32String &text);
	Colors getColors(const Graphics::Surface &screen) const;
	void draw();
	void drawSpeaker(Graphics::Surface &screen, const Common::Rect &r, uint32 color) const;
	void drawPlay(Graphics::Surface &screen, const Common::Rect &r, uint32 color) const;
	void drawHint(Graphics::Surface &screen, const Common::Rect &button, const char *hint, uint32 color) const;
};

} // End of namespace Scumm

#endif
