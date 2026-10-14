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
}

LearnPanel::~LearnPanel() {
	_backup.free();
}

void LearnPanel::show(const Common::U32String &text) {
	Graphics::Surface *screen = _system->lockScreen();
	if (!screen)
		return;

	layout(screen->w, screen->h, text);
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

LearnPanel::Button LearnPanel::buttonAt(const Common::Point &pos) const {
	if (_replayButton.contains(pos))
		return kButtonReplay;
	if (_continueButton.contains(pos))
		return kButtonContinue;
	return kButtonNone;
}

void LearnPanel::setHovered(Button button) {
	if (button == _hovered)
		return;

	_hovered = button;
	draw();
}

void LearnPanel::layout(int screenWidth, int screenHeight, const Common::U32String &text) {
	const int textWidth = screenWidth - 2 * (kMargin + kPadding + kButtonSize + kButtonGap);

	_lines.clear();
	if (!text.empty())
		_font->wordWrapText(text, textWidth, _lines);

	const int lineHeight = _font->getFontHeight() + 2;
	const int buttonColumnHeight = kButtonSize + kHintGap + _hintFont->getFontHeight();
	const int contentHeight = MAX<int>(buttonColumnHeight, lineHeight * _lines.size());

	_box = Common::Rect(kMargin, MAX<int>(0, screenHeight - kMargin - contentHeight - 2 * kPadding),
		screenWidth - kMargin, screenHeight - kMargin);

	const int buttonTop = _box.top + (_box.height() - buttonColumnHeight) / 2;
	_replayButton = Common::Rect(_box.left + kPadding, buttonTop, _box.left + kPadding + kButtonSize, buttonTop + kButtonSize);
	_continueButton = Common::Rect(_box.right - kPadding - kButtonSize, buttonTop, _box.right - kPadding, buttonTop + kButtonSize);

	const int textTop = _box.top + (_box.height() - lineHeight * _lines.size()) / 2;
	_textArea = Common::Rect(_replayButton.right + kButtonGap, textTop, _continueButton.left - kButtonGap, _box.bottom - kPadding);
}

LearnPanel::Colors LearnPanel::getColors(const Graphics::Surface &screen) const {
	static const byte rgb[][3] = {
		{  20,  24,  48 }, // panel
		{  40, 110, 210 }, // replay
		{  90, 160, 255 }, // replay hover
		{  40, 160,  70 }, // continue
		{  90, 210, 110 }, // continue hover
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

	Colors colors = { c[0], c[1], c[2], c[3], c[4], c[5], c[6] };
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
	for (uint i = 0; i < _lines.size(); i++)
		_font->drawString(screen, _lines[i], _textArea.left, _textArea.top + i * lineHeight,
			_textArea.width(), colors.text, Graphics::kTextAlignCenter);

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
