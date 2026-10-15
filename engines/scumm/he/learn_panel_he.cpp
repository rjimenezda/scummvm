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

void LearnPanel::show(const Common::U32String &translation, const LearnExercise *exercise) {
	Graphics::Surface *screen = _system->lockScreen();
	if (!screen)
		return;

	_translation = translation;
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
		_font->wordWrapText(sentenceWithBlank(), _textArea.width(), _sentenceLines);
	} else {
		_wrong[choice] = true;
	}

	draw();
	return _solved;
}

Common::U32String LearnPanel::sentenceWithBlank() const {
	Common::String blank = _solved ? _exercise.answer : Common::String(MAX<uint>(4, _exercise.answer.size()), '_');
	return Common::U32String(_exercise.before + blank + _exercise.after, Common::kISO8859_1);
}

const Graphics::Font *LearnPanel::translationFont() const {
	// With an exercise, the sentence is the main content and the translation a hint
	return _hasExercise ? _hintFont : _font;
}

void LearnPanel::layout(int screenWidth, int screenHeight) {
	const int textWidth = screenWidth - 2 * (kMargin + kPadding + kButtonSize + kButtonGap);
	const int lineHeight = _font->getFontHeight() + 2;
	const Graphics::Font *trFont = translationFont();
	const int trLineHeight = trFont->getFontHeight() + 2;

	_sentenceLines.clear();
	_translationLines.clear();
	if (_hasExercise)
		_font->wordWrapText(sentenceWithBlank(), textWidth, _sentenceLines);
	if (!_translation.empty())
		trFont->wordWrapText(_translation, textWidth, _translationLines);

	int textHeight = lineHeight * _sentenceLines.size() + trLineHeight * _translationLines.size();
	if (_hasExercise)
		textHeight += kChoiceGap + kChoiceHeight;

	const int buttonColumnHeight = kButtonSize + kHintGap + _hintFont->getFontHeight();
	const int contentHeight = MAX<int>(buttonColumnHeight, textHeight);

	_box = Common::Rect(kMargin, MAX<int>(0, screenHeight - kMargin - contentHeight - 2 * kPadding),
		screenWidth - kMargin, screenHeight - kMargin);

	const int buttonTop = _box.top + (_box.height() - buttonColumnHeight) / 2;
	_replayButton = Common::Rect(_box.left + kPadding, buttonTop, _box.left + kPadding + kButtonSize, buttonTop + kButtonSize);
	_continueButton = Common::Rect(_box.right - kPadding - kButtonSize, buttonTop, _box.right - kPadding, buttonTop + kButtonSize);

	const int textTop = _box.top + (_box.height() - textHeight) / 2;
	_textArea = Common::Rect(_replayButton.right + kButtonGap, textTop, _continueButton.left - kButtonGap, _box.bottom - kPadding);
	_sentenceTop = textTop;
	_translationTop = _sentenceTop + lineHeight * _sentenceLines.size();

	if (_hasExercise) {
		int widths[LearnExercise::kNumChoices];
		int total = kChoiceGap * (LearnExercise::kNumChoices - 1);
		for (int i = 0; i < LearnExercise::kNumChoices; i++) {
			widths[i] = MAX<int>(kChoiceMinWidth, _font->getStringWidth(Common::U32String(_exercise.choices[i], Common::kISO8859_1)) + 2 * kChoicePadding);
			total += widths[i];
		}

		const int choiceTop = _translationTop + trLineHeight * _translationLines.size() + kChoiceGap;
		int x = _textArea.left + (_textArea.width() - total) / 2;
		for (int i = 0; i < LearnExercise::kNumChoices; i++) {
			_choiceButtons[i] = Common::Rect(x, choiceTop, x + widths[i], choiceTop + kChoiceHeight);
			x += widths[i] + kChoiceGap;
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
		{ 255, 255, 255 }  // text
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

	Colors colors = { c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8], c[9], c[10] };
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
	screen->drawRoundRect(_box, 12, colors.panel, true);

	screen->drawRoundRect(_replayButton, 10, _hovered == kButtonReplay ? colors.replayHover : colors.replay, true);
	drawSpeaker(*screen, _replayButton, colors.icon);
	drawHint(*screen, _replayButton, "R", colors.text);

	screen->drawRoundRect(_continueButton, 10, _hovered == kButtonContinue ? colors.contHover : colors.cont, true);
	drawPlay(*screen, _continueButton, colors.icon);
	drawHint(*screen, _continueButton, "Space", colors.text);

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
			screen->drawRoundRect(r, 8, color, true);
			_font->drawString(screen, Common::U32String(_exercise.choices[i], Common::kISO8859_1),
				r.left, r.top + (r.height() - _font->getFontHeight()) / 2, r.width(), colors.text, Graphics::kTextAlignCenter);

			Common::String key = Common::String::format("%d", i + 1);
			_hintFont->drawString(screen, key, r.left + 4, r.top + 2, 10, colors.text);
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
		screen.drawThickLine(x0, y0, x1, y + s / 2, 3, 3, color);
		screen.drawThickLine(x1, y + s / 2, x0, y1, 3, 3, color);
	}
}

void LearnPanel::drawHint(Graphics::Surface &screen, const Common::Rect &button, const char *hint, uint32 color) const {
	const int width = kButtonSize + 2 * kButtonGap;
	_hintFont->drawString(&screen, hint, button.left - kButtonGap, button.bottom + kHintGap, width, color, Graphics::kTextAlignCenter);
}

void LearnPanel::drawPlay(Graphics::Surface &screen, const Common::Rect &r, uint32 color) const {
	const int s = r.width();
	const int x = r.left, y = r.top;

	const int triX[] = { x + s * 36 / 100, x + s * 76 / 100, x + s * 36 / 100 };
	const int triY[] = { y + s * 24 / 100, y + s / 2, y + s * 76 / 100 };
	screen.drawPolygonScan(triX, triY, 3, Common::Rect(triX[0], triY[0], triX[1] + 1, triY[2] + 1), color);
}

} // End of namespace Scumm
