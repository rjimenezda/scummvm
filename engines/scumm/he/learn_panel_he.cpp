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

#include "common/system.h"
#include "graphics/cursorman.h"
#include "graphics/font.h"
#include "graphics/palette.h"
#include "graphics/paletteman.h"

#include "scumm/he/learn_panel_he.h"

namespace Scumm {

static const int kMargin = 8;
static const int kPadding = 8;
static const int kButtonSize = 48;
static const int kButtonGap = 12;
static const int kHintGap = 2;
static const int kToggleSize = 26;
static const int kToggleGap = 6;
static const int kChoiceGap = 10;
static const int kChoiceHeight = 36;
static const int kChoicePadding = 18;
static const int kChoiceMinWidth = 90;

// Arrow cursor shown while paused: 0 transparent, 1 black, 2 white
static const int kCursorWidth = 12;
static const int kCursorHeight = 19;
static const char *const kCursorRows[kCursorHeight] = {
	"1           ",
	"11          ",
	"121         ",
	"1221        ",
	"12221       ",
	"122221      ",
	"1222221     ",
	"12222221    ",
	"122222221   ",
	"1222222221  ",
	"12222222221 ",
	"122222211111",
	"1222121     ",
	"122112 1    ",
	"121  121    ",
	"11   121    ",
	"1     121   ",
	"      121   ",
	"       11   "
};
static const byte kCursorPalette[] = { 255, 0, 255, 0, 0, 0, 255, 255, 255 };

LearnPanel::LearnPanel(OSystem *system, const Graphics::Font *font, const Graphics::Font *hintFont) :
	_system(system), _font(font), _hintFont(hintFont) {
	for (int i = 0; i < LearnExercise::kNumChoices; i++)
		_wrong[i] = false;
}

LearnPanel::~LearnPanel() {
	_backup.free();
}

void LearnPanel::show(const Common::String &sentence, const Common::U32String &translation, const LearnExercise *exercise) {
	Graphics::Surface *screen = _system->lockScreen();
	if (!screen)
		return;

	_screenWidth = screen->w;
	_sentence = sentence;
	_translation = translation;
	_showTranslation = true;
	_hasExercise = exercise != nullptr;
	_solved = false;
	for (int i = 0; i < LearnExercise::kNumChoices; i++)
		_wrong[i] = false;
	if (exercise)
		_exercise = *exercise;

	layout(screen->w, screen->h);
	_backup.copyFrom(screen->getSubArea(_box));
	_system->unlockScreen();

	_hovered = kButtonNone;
	_visible = true;
	draw();
	showCursor();
}

int LearnPanel::px(int size) const {
	// Sizes are designed for 640 pixel wide games
	return MAX<int>(1, size * _screenWidth / 640);
}

void LearnPanel::hide() {
	if (!_visible)
		return;

	_system->copyRectToScreen(_backup.getPixels(), _backup.pitch, _box.left, _box.top, _box.width(), _box.height());
	_backup.free();
	_visible = false;
	restoreCursor();
}

void LearnPanel::showCursor() {
	// The game often hides the cursor while someone talks, so use our own
	byte cursor[kCursorWidth * kCursorHeight];
	for (int y = 0; y < kCursorHeight; y++)
		for (int x = 0; x < kCursorWidth; x++)
			cursor[y * kCursorWidth + x] = kCursorRows[y][x] == ' ' ? 0 : kCursorRows[y][x] - '0';

	Graphics::PixelFormat format = Graphics::PixelFormat::createFormatCLUT8();
	CursorMan.pushCursorPalette(kCursorPalette, 0, 3);
	CursorMan.pushCursor(cursor, kCursorWidth, kCursorHeight, 0, 0, 0, &format);
	_cursorWasVisible = CursorMan.showMouse(true);
}

void LearnPanel::restoreCursor() {
	CursorMan.showMouse(_cursorWasVisible);
	CursorMan.popCursor();
	CursorMan.popCursorPalette();
}

int LearnPanel::buttonAt(const Common::Point &pos) const {
	if (!_translation.empty() && _translationButton.contains(pos))
		return kButtonTranslation;
	if (_replayButton.contains(pos))
		return kButtonReplay;
	if (_continueButton.contains(pos))
		return kButtonContinue;
	if (_hasExercise && !_solved) {
		for (int i = 0; i < LearnExercise::kNumChoices; i++) {
			if (_choiceButtons[i].contains(pos))
				return i;
		}
	}
	return kButtonNone;
}

void LearnPanel::setHovered(int button) {
	if (button == _hovered)
		return;

	_hovered = button;
	draw();
}

bool LearnPanel::choose(int choice) {
	if (!_hasExercise || _solved || choice < 0 || choice >= LearnExercise::kNumChoices)
		return false;

	if (choice == _exercise.correctChoice) {
		_solved = true;
		_sentenceLines.clear();
		_font->wordWrapText(displaySentence(), _textArea.width(), _sentenceLines);
	} else {
		_wrong[choice] = true;
	}

	draw();
	return _solved;
}

void LearnPanel::toggleTranslation() {
	if (_translation.empty())
		return;

	_showTranslation = !_showTranslation;
	relayout();
}

void LearnPanel::relayout() {
	// The panel changes size, so put back what was under it first
	_system->copyRectToScreen(_backup.getPixels(), _backup.pitch, _box.left, _box.top, _box.width(), _box.height());

	Graphics::Surface *screen = _system->lockScreen();
	if (!screen)
		return;

	layout(screen->w, screen->h);
	_backup.free();
	_backup.copyFrom(screen->getSubArea(_box));
	_system->unlockScreen();

	draw();
}

Common::U32String LearnPanel::displaySentence() const {
	if (!_hasExercise)
		return Common::U32String(learnDisplayText(_sentence), Common::kISO8859_1);

	Common::String blank = _solved ? _exercise.answer : Common::String(MAX<uint>(4, _exercise.answer.size()), '_');
	return Common::U32String(learnDisplayText(_exercise.before + blank + _exercise.after), Common::kISO8859_1);
}

const Graphics::Font *LearnPanel::translationFont() const {
	// Under the sentence, the translation is a hint
	return _sentence.empty() ? _font : _hintFont;
}

void LearnPanel::layout(int screenWidth, int screenHeight) {
	const int textWidth = screenWidth - 2 * (px(kMargin) + px(kPadding) + px(kButtonSize) + px(kButtonGap));
	const int lineHeight = _font->getFontHeight() + 2;
	const Graphics::Font *trFont = translationFont();
	const int trLineHeight = trFont->getFontHeight() + 2;

	_sentenceLines.clear();
	_translationLines.clear();
	if (_hasExercise || !_sentence.empty())
		_font->wordWrapText(displaySentence(), textWidth, _sentenceLines);
	if (!_translation.empty() && _showTranslation)
		trFont->wordWrapText(_translation, textWidth, _translationLines);

	int textHeight = lineHeight * _sentenceLines.size() + trLineHeight * _translationLines.size();
	if (_hasExercise)
		textHeight += px(kChoiceGap) + px(kChoiceHeight);

	// The translation toggle sits above the continue button, in its own space
	const int toggleHeight = _translation.empty() ? 0 : px(kToggleSize) + px(kToggleGap);
	const int buttonColumnHeight = toggleHeight + px(kButtonSize) + px(kHintGap) + _hintFont->getFontHeight();
	const int contentHeight = MAX<int>(buttonColumnHeight, textHeight);

	_box = Common::Rect(px(kMargin), MAX<int>(0, screenHeight - px(kMargin) - contentHeight - 2 * px(kPadding)),
		screenWidth - px(kMargin), screenHeight - px(kMargin));

	const int buttonTop = _box.top + (_box.height() - buttonColumnHeight) / 2 + toggleHeight;
	_replayButton = Common::Rect(_box.left + px(kPadding), buttonTop, _box.left + px(kPadding) + px(kButtonSize), buttonTop + px(kButtonSize));
	_continueButton = Common::Rect(_box.right - px(kPadding) - px(kButtonSize), buttonTop, _box.right - px(kPadding), buttonTop + px(kButtonSize));

	_translationButton = Common::Rect(_continueButton.right - px(kToggleSize), buttonTop - toggleHeight,
		_continueButton.right, buttonTop - toggleHeight + px(kToggleSize));

	const int textTop = _box.top + (_box.height() - textHeight) / 2;
	_textArea = Common::Rect(_replayButton.right + px(kButtonGap), textTop, _continueButton.left - px(kButtonGap), _box.bottom - px(kPadding));
	_sentenceTop = textTop;
	_translationTop = _sentenceTop + lineHeight * _sentenceLines.size();

	if (_hasExercise) {
		int widths[LearnExercise::kNumChoices];
		int total = px(kChoiceGap) * (LearnExercise::kNumChoices - 1);
		for (int i = 0; i < LearnExercise::kNumChoices; i++) {
			widths[i] = MAX<int>(px(kChoiceMinWidth), _font->getStringWidth(Common::U32String(_exercise.choices[i], Common::kISO8859_1)) + 2 * px(kChoicePadding));
			total += widths[i];
		}

		const int choiceTop = _translationTop + trLineHeight * _translationLines.size() + px(kChoiceGap);
		int x = _textArea.left + (_textArea.width() - total) / 2;
		for (int i = 0; i < LearnExercise::kNumChoices; i++) {
			_choiceButtons[i] = Common::Rect(x, choiceTop, x + widths[i], choiceTop + px(kChoiceHeight));
			x += widths[i] + px(kChoiceGap);
		}
	}
}

LearnPanel::Colors LearnPanel::getColors(const Graphics::Surface &screen) const {
	static const byte rgb[][3] = {
		{  20,  24,  48 }, // panel
		{  40, 110, 210 }, // replay
		{  90, 160, 255 }, // replay hover
		{  40, 160,  70 }, // continue
		{  90, 210, 110 }, // continue hover
		{  70,  70, 110 }, // choice
		{ 120, 120, 180 }, // choice hover
		{ 200,  50,  50 }, // wrong choice
		{  40, 160,  70 }, // right choice
		{ 255, 255, 255 }, // icon
		{ 255, 255, 255 }, // text
		{ 220, 160,  40 }, // translation toggle on
		{  80,  80,  90 }  // translation toggle off
	};
	uint32 c[ARRAYSIZE(rgb)];

	if (screen.format.isCLUT8()) {
		byte palette[256 * 3];
		_system->getPaletteManager()->grabPalette(palette, 0, 256);
		Graphics::PaletteLookup lookup(palette, 256);
		for (int i = 0; i < ARRAYSIZE(rgb); i++)
			c[i] = lookup.findBestColor(rgb[i][0], rgb[i][1], rgb[i][2]);
	} else {
		for (int i = 0; i < ARRAYSIZE(rgb); i++)
			c[i] = screen.format.RGBToColor(rgb[i][0], rgb[i][1], rgb[i][2]);
	}

	Colors colors = { c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8], c[9], c[10], c[11], c[12] };
	return colors;
}

void LearnPanel::draw() {
	if (!_visible)
		return;

	Graphics::Surface *screen = _system->lockScreen();
	if (!screen)
		return;

	const Colors colors = getColors(*screen);

	screen->copyRectToSurface(_backup, _box.left, _box.top, Common::Rect(_box.width(), _box.height()));
	screen->drawRoundRect(_box, px(12), colors.panel, true);

	screen->drawRoundRect(_replayButton, px(10), _hovered == kButtonReplay ? colors.replayHover : colors.replay, true);
	drawSpeaker(*screen, _replayButton, colors.icon);
	drawHint(*screen, _replayButton, "R", colors.text);

	screen->drawRoundRect(_continueButton, px(10), _hovered == kButtonContinue ? colors.contHover : colors.cont, true);
	drawPlay(*screen, _continueButton, colors.icon);
	drawHint(*screen, _continueButton, "Space", colors.text);

	if (!_translation.empty()) {
		uint32 color = _showTranslation ? colors.toggleOn : colors.toggleOff;
		if (_hovered == kButtonTranslation)
			color = colors.choiceHover;
		screen->drawRoundRect(_translationButton, px(6), color, true);
		drawBubble(*screen, _translationButton, colors.icon);

		const int hintWidth = _hintFont->getStringWidth("T");
		_hintFont->drawString(screen, "T", _translationButton.left - px(kHintGap) - hintWidth,
			_translationButton.top + (_translationButton.height() - _hintFont->getFontHeight()) / 2, hintWidth, colors.text);
	}

	const int lineHeight = _font->getFontHeight() + 2;
	for (uint i = 0; i < _sentenceLines.size(); i++)
		_font->drawString(screen, _sentenceLines[i], _textArea.left, _sentenceTop + i * lineHeight,
			_textArea.width(), colors.text, Graphics::kTextAlignCenter);

	const Graphics::Font *trFont = translationFont();
	const int trLineHeight = trFont->getFontHeight() + 2;
	for (uint i = 0; i < _translationLines.size(); i++)
		trFont->drawString(screen, _translationLines[i], _textArea.left, _translationTop + i * trLineHeight,
			_textArea.width(), colors.text, Graphics::kTextAlignCenter);

	if (_hasExercise) {
		for (int i = 0; i < LearnExercise::kNumChoices; i++) {
			uint32 color = colors.choice;
			if (_solved && i == _exercise.correctChoice)
				color = colors.right;
			else if (_wrong[i])
				color = colors.wrong;
			else if (!_solved && _hovered == i)
				color = colors.choiceHover;

			const Common::Rect &r = _choiceButtons[i];
			screen->drawRoundRect(r, px(8), color, true);
			_font->drawString(screen, Common::U32String(_exercise.choices[i], Common::kISO8859_1),
				r.left, r.top + (r.height() - _font->getFontHeight()) / 2, r.width(), colors.text, Graphics::kTextAlignCenter);

			Common::String key = Common::String::format("%d", i + 1);
			_hintFont->drawString(screen, key, r.left + px(4), r.top + px(2), px(10), colors.text);
		}
	}

	_system->unlockScreen();
}

void LearnPanel::drawSpeaker(Graphics::Surface &screen, const Common::Rect &r, uint32 color) const {
	const int s = r.width();
	const int x = r.left, y = r.top;

	screen.fillRect(Common::Rect(x + s * 22 / 100, y + s * 40 / 100, x + s * 38 / 100, y + s * 60 / 100), color);

	const int coneX[] = { x + s * 38 / 100, x + s * 56 / 100, x + s * 56 / 100, x + s * 38 / 100 };
	const int coneY[] = { y + s * 40 / 100, y + s * 22 / 100, y + s * 78 / 100, y + s * 60 / 100 };
	screen.drawPolygonScan(coneX, coneY, 4, Common::Rect(coneX[0], coneY[1], coneX[1] + 1, coneY[2] + 1), color);

	// Sound waves
	for (int i = 0; i < 2; i++) {
		const int x0 = x + s * (64 + i * 10) / 100;
		const int x1 = x + s * (70 + i * 12) / 100;
		const int y0 = y + s * (36 - i * 10) / 100;
		const int y1 = y + s * (64 + i * 10) / 100;
		screen.drawThickLine(x0, y0, x1, y + s / 2, px(3), px(3), color);
		screen.drawThickLine(x1, y + s / 2, x0, y1, px(3), px(3), color);
	}
}

void LearnPanel::drawHint(Graphics::Surface &screen, const Common::Rect &button, const char *hint, uint32 color) const {
	const int width = px(kButtonSize) + 2 * px(kButtonGap);
	_hintFont->drawString(&screen, hint, button.left - px(kButtonGap), button.bottom + px(kHintGap), width, color, Graphics::kTextAlignCenter);
}

void LearnPanel::drawBubble(Graphics::Surface &screen, const Common::Rect &r, uint32 color) const {
	// Speech bubble with a tail, standing for "text"
	const int s = r.width();
	const int x = r.left, y = r.top;

	screen.drawRoundRect(Common::Rect(x + s * 18 / 100, y + s * 22 / 100, x + s * 82 / 100, y + s * 64 / 100), px(4), color, true);

	const int tailX[] = { x + s * 32 / 100, x + s * 48 / 100, x + s * 30 / 100 };
	const int tailY[] = { y + s * 62 / 100, y + s * 62 / 100, y + s * 80 / 100 };
	screen.drawPolygonScan(tailX, tailY, 3, Common::Rect(tailX[2], tailY[0], tailX[1] + 1, tailY[2] + 1), color);
}

void LearnPanel::drawPlay(Graphics::Surface &screen, const Common::Rect &r, uint32 color) const {
	const int s = r.width();
	const int x = r.left, y = r.top;

	const int triX[] = { x + s * 36 / 100, x + s * 76 / 100, x + s * 36 / 100 };
	const int triY[] = { y + s * 24 / 100, y + s / 2, y + s * 76 / 100 };
	screen.drawPolygonScan(triX, triY, 3, Common::Rect(triX[0], triY[0], triX[1] + 1, triY[2] + 1), color);
}

} // End of namespace Scumm
