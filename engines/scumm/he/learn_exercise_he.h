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

#ifndef SCUMM_HE_LEARN_EXERCISE_HE_H
#define SCUMM_HE_LEARN_EXERCISE_HE_H

#include "common/array.h"
#include "common/hashmap.h"
#include "common/hash-str.h"
#include "common/str.h"

namespace Scumm {

// Fill-in-the-blank exercise built from a spoken line
struct LearnExercise {
	static const int kNumChoices = 3;

	Common::String before;
	uint blankStart;
	Common::String answer;
	Common::String after;
	Common::String choices[kNumChoices];
	int correctChoice;
};

// Words that can be blanked, collected from every line seen so far.
// Used to pick the wrong choices of an exercise.
class LearnWordPool {
public:
	void addSentence(const Common::String &sentence);
	bool pickDecoys(const Common::String &answer, uint32 seed, Common::String *decoys, int count) const;
	// Words only ever seen capitalized in the middle of a sentence, e.g. character names
	bool isName(const Common::String &word) const { return _names.contains(word) && !_lowercase.contains(word); }

private:
	typedef Common::HashMap<Common::String, bool, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> WordSet;

	Common::Array<Common::String> _words;
	WordSet _known;
	WordSet _names;
	WordSet _lowercase;
};

// Printable text of a talk string, without its control codes.
// heversion is 0 for classic SCUMM v5/v6 games, whose strings can hold several
// voiced lines: the text stops at the second voice code, at index end.
struct LearnTalkText {
	Common::String text;
	Common::Array<uint> positions; // Index in the talk string of each character of text
	uint32 voiceOffset;
	uint end;
};

// Localized games tag their lines with "[ID]", which the engine never displays
LearnTalkText extractLearnText(const byte *msg, int heversion, bool localized);

// Text as it should be shown outside the game, e.g. '^' is drawn as an ellipsis by HE fonts
Common::String learnDisplayText(const Common::String &text);

bool buildLearnExercise(const Common::String &sentence, uint32 seed, const LearnWordPool &pool, LearnExercise &exercise);

} // End of namespace Scumm

#endif
