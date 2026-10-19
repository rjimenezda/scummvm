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

#include "common/util.h"

#include "scumm/he/learn_exercise_he.h"

namespace Scumm {

// Sentences shorter than this are not turned into exercises
static const int kMinSentenceWords = 3;
static const uint kMinBlankLength = 3;

static const char *const kFillerWords[] = {
	"the", "and", "but", "for", "nor", "yet", "you", "your", "yours", "she", "her", "him", "his",
	"its", "our", "ours", "they", "them", "their", "this", "that", "these", "those", "are", "was",
	"were", "been", "being", "have", "has", "had", "does", "did", "doing", "will", "would", "can",
	"could", "should", "shall", "may", "might", "must", "not", "with", "from", "into", "onto",
	"about", "than", "then", "there", "here", "what", "who", "whom", "which", "when", "where",
	"why", "how", "all", "any", "some", "just", "very", "too", "also", "out", "off", "over",
	"let", "lets", "let's", "gonna", "okay", "yes", "yeah", "hey", "wow", "gee", "huh", "uh-oh",
	"i'm", "i'll", "i've", "i'd", "you're", "you'll", "we're", "we'll", "it's", "that's",
	"there's", "he's", "she's", "they're", "don't", "can't", "won't", "didn't", "isn't", "oh"
};

// Used when not enough words have been seen yet to pick decoys
static const char *const kFallbackWords[] = {
	"ocean", "shell", "friend", "treasure", "castle", "bottle", "garden", "jelly",
	"rock", "cave", "seed", "door", "music", "school", "find", "swim", "help", "look"
};

static bool isWordChar(byte c) {
	return Common::isAlpha(c) || c == '\'' || c == '-' || c >= 0xC0;
}

static uint32 mixSeed(uint32 seed, uint32 salt) {
	uint32 h = seed * 2654435761u ^ (salt + 0x9E3779B9u + (seed << 6) + (seed >> 2));
	h ^= h >> 15;
	h *= 2246822519u;
	h ^= h >> 13;
	return h;
}

static bool isBlankable(const Common::String &word, bool sentenceStart) {
	if (word.size() < kMinBlankLength)
		return false;

	// Skip proper nouns (capitalized in the middle of a sentence)
	if (!sentenceStart && Common::isUpper(word[0]))
		return false;

	Common::String lower = word;
	lower.toLowercase();
	for (int i = 0; i < ARRAYSIZE(kFillerWords); i++) {
		if (lower == kFillerWords[i])
			return false;
	}
	return true;
}

struct WordSpan {
	uint start;
	uint length;
	bool blankable;
	bool name;
};

static Common::Array<WordSpan> splitWords(const Common::String &sentence) {
	Common::Array<WordSpan> words;
	bool sentenceStart = true;

	for (uint i = 0; i < sentence.size();) {
		byte c = sentence[i];
		if (!isWordChar(c)) {
			if (c == '.' || c == '!' || c == '?')
				sentenceStart = true;
			i++;
			continue;
		}

		uint start = i;
		while (i < sentence.size() && isWordChar(sentence[i]))
			i++;

		// Trailing dashes and quotes are punctuation, not part of the word
		uint end = i;
		while (end > start && (sentence[end - 1] == '-' || sentence[end - 1] == '\''))
			end--;
		if (end == start)
			continue;

		Common::String word(sentence.c_str() + start, end - start);
		WordSpan span = { start, end - start, isBlankable(word, sentenceStart), !sentenceStart && Common::isUpper(word[0]) };
		words.push_back(span);
		sentenceStart = false;
	}

	return words;
}

void LearnWordPool::addSentence(const Common::String &sentence) {
	Common::Array<WordSpan> words = splitWords(sentence);
	for (uint i = 0; i < words.size(); i++) {
		Common::String word(sentence.c_str() + words[i].start, words[i].length);
		if (words[i].name)
			_names[word] = true;
		else if (Common::isLower(word[0]))
			_lowercase[word] = true;
		if (!words[i].blankable || isName(word))
			continue;

		word.toLowercase();
		if (!_known.contains(word)) {
			_known[word] = true;
			_words.push_back(word);
		}
	}
}

bool LearnWordPool::pickDecoys(const Common::String &answer, uint32 seed, Common::String *decoys, int count) const {
	Common::Array<Common::String> candidates;
	Common::Array<Common::String> similar;

	for (uint i = 0; i < _words.size(); i++) {
		if (!isName(_words[i]))
			candidates.push_back(_words[i]);
	}
	if ((int)candidates.size() < count + 1) {
		for (int i = 0; i < ARRAYSIZE(kFallbackWords); i++)
			candidates.push_back(kFallbackWords[i]);
	}

	// Prefer words of a similar length so the answer doesn't stand out
	for (uint i = 0; i < candidates.size(); i++) {
		if (!candidates[i].equalsIgnoreCase(answer) && ABS<int>((int)candidates[i].size() - (int)answer.size()) <= 2)
			similar.push_back(candidates[i]);
	}
	const Common::Array<Common::String> &source = (int)similar.size() >= count ? similar : candidates;

	int found = 0;
	for (uint attempt = 0; found < count && attempt < source.size() * 4; attempt++) {
		const Common::String &word = source[mixSeed(seed, attempt + 1) % source.size()];
		if (word.equalsIgnoreCase(answer))
			continue;

		bool duplicate = false;
		for (int i = 0; i < found; i++)
			duplicate |= decoys[i].equalsIgnoreCase(word);
		if (!duplicate)
			decoys[found++] = word;
	}

	return found == count;
}

LearnTalkText extractLearnText(const byte *msg, int heversion, bool localized) {
	// Argument sizes of the 0xFF control codes in SCUMM v6 and HE60-71 strings
	static const int classicArgs[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 14, 0, 2, 2, 3, 0 };
	const byte modernCode = heversion >= 80 ? 127 : 64;
	LearnTalkText result;
	result.voiceOffset = 0;

	uint pos = 0;
	while (msg[pos]) {
		byte c = msg[pos];
		if (heversion >= 72 && c == modernCode) {
			byte code = msg[pos + 1];
			if (!code)
				break;
			pos += 2;
			if (code == 'T' || code == 't') {
				if (code == 'T')
					result.voiceOffset = (uint32)atol((const char *)msg + pos);
				while (msg[pos] && msg[pos] != modernCode)
					pos++;
				if (msg[pos])
					pos++;
			} else if (code == 'n') {
				result.text += ' ';
				result.positions.push_back(pos - 2);
			}
		} else if (heversion < 72 && (c == 0xFF || c == 0xFE)) {
			byte code = msg[pos + 1];
			if (!code)
				break;
			// New line, or wait before the next part of the line
			if (code == 1 || code == 3) {
				result.text += ' ';
				result.positions.push_back(pos);
			} else if (code == 10 && !result.voiceOffset) {
				const byte *b = msg + pos + 2;
				result.voiceOffset = b[0] | (b[1] << 8) | (b[4] << 16) | (b[5] << 24);
			}
			// The arguments are binary and may contain zero bytes
			pos += 2 + (code < ARRAYSIZE(classicArgs) ? classicArgs[code] : 0);
		} else if (localized && c == '[') {
			while (msg[pos] && msg[pos] != ']')
				pos++;
			if (msg[pos])
				pos++;
		} else {
			if (c >= 32 || c == '\t') {
				result.text += c == '\t' ? ' ' : (char)c;
				result.positions.push_back(pos);
			}
			pos++;
		}
	}

	// Trim, keeping positions in sync
	uint first = 0, last = result.text.size();
	while (first < last && Common::isSpace(result.text[first]))
		first++;
	while (last > first && Common::isSpace(result.text[last - 1]))
		last--;
	if (first > 0 || last < result.text.size()) {
		Common::Array<uint> positions;
		for (uint i = first; i < last; i++)
			positions.push_back(result.positions[i]);
		result.text = Common::String(result.text.c_str() + first, last - first);
		result.positions = positions;
	}

	return result;
}

Common::String learnDisplayText(const Common::String &text) {
	Common::String result;
	for (uint i = 0; i < text.size(); i++) {
		if (text[i] != '^') {
			result += text[i];
			continue;
		}

		while (i + 1 < text.size() && text[i + 1] == '^')
			i++;
		result += "...";
		if (i + 1 < text.size() && !Common::isSpace(text[i + 1]))
			result += ' ';
	}
	return result;
}

bool buildLearnExercise(const Common::String &sentence, uint32 seed, const LearnWordPool &pool, LearnExercise &exercise) {
	Common::Array<WordSpan> words = splitWords(sentence);
	if ((int)words.size() < kMinSentenceWords)
		return false;

	Common::Array<uint> blankable;
	for (uint i = 0; i < words.size(); i++) {
		if (words[i].blankable && !pool.isName(Common::String(sentence.c_str() + words[i].start, words[i].length)))
			blankable.push_back(i);
	}
	if (blankable.empty())
		return false;

	const WordSpan &blank = words[blankable[mixSeed(seed, 0) % blankable.size()]];
	exercise.before = Common::String(sentence.c_str(), blank.start);
	exercise.blankStart = blank.start;
	exercise.answer = Common::String(sentence.c_str() + blank.start, blank.length);
	exercise.after = Common::String(sentence.c_str() + blank.start + blank.length);

	Common::String decoys[LearnExercise::kNumChoices - 1];
	if (!pool.pickDecoys(exercise.answer, seed, decoys, ARRAYSIZE(decoys)))
		return false;

	// Decoys come from the pool in lower case; match the answer's capitalization
	const bool capitalized = Common::isUpper(exercise.answer[0]);
	exercise.correctChoice = mixSeed(seed, 99) % LearnExercise::kNumChoices;
	for (int i = 0, d = 0; i < LearnExercise::kNumChoices; i++) {
		if (i == exercise.correctChoice) {
			exercise.choices[i] = exercise.answer;
		} else {
			exercise.choices[i] = decoys[d++];
			if (capitalized)
				exercise.choices[i].setChar(toupper(exercise.choices[i][0]), 0);
		}
	}

	return true;
}

} // End of namespace Scumm
