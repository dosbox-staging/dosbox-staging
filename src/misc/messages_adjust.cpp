// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/messages_adjust.h"

#include "utils/checks.h"
#include "utils/string_utils.h"

CHECK_NARROWING();


void adjust_newlines(const std::string &current,
                     std::string &previous,
                     std::string &translated)
{
	auto count_leading_newlines = [](const std::string& message) {
		const auto position = message.find_first_not_of('\n');
		if (position == std::string::npos) {
			return message.size();
		} else {
			return position;
		}
	};

	auto count_trailing_newlines = [](const std::string& message) {
		const auto position = message.find_last_not_of('\n');
		if (position == std::string::npos) {
			return message.size();
		} else {
			return message.size() - position - 1;
		}
	};

	// Count the number of leading and trailing newlines in the translated
	// message, the current English message, and the English message
	// which was the base for the translation (the previous English)

	const auto num_leading_translated  = count_leading_newlines(translated);
	const auto num_trailing_translated = count_trailing_newlines(translated);

	const auto num_leading_previous  = count_leading_newlines(previous);
	const auto num_trailing_previous = count_trailing_newlines(previous);

	const auto num_leading_current  = count_leading_newlines(current);
	const auto num_trailing_current = count_trailing_newlines(current);

	// Skip auto-adjusting if any of the strings is empty or consists only
	// of newline characters
	if (num_leading_translated == translated.size() || translated.empty() ||
	    num_leading_previous == previous.size() || previous.empty() ||
	    num_leading_current == current.size() || current.empty()) {
		return;
	}

	// Safety check - do not auto-adjust the translation if the translated
	// message has a different
	if (num_leading_translated  != num_leading_previous ||
	    num_trailing_translated != num_trailing_previous) {
		return;
	}

	const auto translated_stripped = translated.substr(
	        num_leading_translated,
	        translated.size() - num_leading_translated - num_trailing_translated);
	const auto previous_stripped = previous.substr(
	        num_leading_previous,
	        previous.size() - num_leading_previous - num_trailing_previous);
	const auto current_stripped = current.substr(
	        num_leading_current,
                current.size() - num_leading_current - num_trailing_current);

	// We can only auto-adjust the translated message if the previous and
	// current English strings only differ by the number of leading/trailing
	// newlines
	if (current_stripped != previous_stripped) {
		return;
	}

	// Override the previous English string, adjust the translation
	previous   = current;
	translated = std::string(num_leading_current, '\n') +
	             translated_stripped +
	             std::string(num_trailing_current, '\n');
}
