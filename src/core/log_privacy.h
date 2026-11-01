// Removes personal details from log lines, so that logs are safer to share.
//
// Every line passes through this before it's queued, so it also covers text copied from
// the game's own messages. It doesn't remove Steam display names, because those appear
// in ordinary game messages and can't be told apart from other text.
#pragma once

#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

namespace sgfixes::log_privacy {

// Steam IDs for individual accounts are 17 digits long, and all start with these digits.
inline constexpr std::string_view kSteamIdPrefix = "7656119";
inline constexpr size_t kSteamIdLength = 17;

// Folders whose immediate subfolder is named after a user.
inline constexpr std::string_view kUserParentFolders[] = {"home", "users"};

// Checks for either kind of path separator.
//   character: the character to check.
// Returns true for '/' and '\'.
inline bool IsPathSeparator(char character) {
    return character == '/' || character == '\\';
}

// Checks whether a path segment is a folder listed in kUserParentFolders.
//   segment: the text between two path separators.
// Returns true on a match, ignoring case.
inline bool IsUserParentFolder(std::string_view segment) {
    for (std::string_view folder : kUserParentFolders) {
        if (segment.size() != folder.size()) {
            continue;
        }
        bool matches = true;
        for (size_t i = 0; i < segment.size(); i++) {
            auto character = static_cast<unsigned char>(segment[i]);
            matches = matches && std::tolower(character) == folder[i];
        }
        if (matches) {
            return true;
        }
    }
    return false;
}

// Replaces every full Steam ID with a placeholder. A number only counts if the whole run
// of digits has the right length and prefix.
//   text: the text to clean.
// Returns the cleaned text.
inline std::string ReplaceSteamIds(std::string_view text) {
    std::string result;
    size_t position = 0;
    while (position < text.size()) {
        size_t digitsEnd = position;
        while (digitsEnd < text.size()
               && std::isdigit(static_cast<unsigned char>(text[digitsEnd]))) {
            digitsEnd++;
        }
        if (digitsEnd == position) {
            result += text[position++];
            continue;
        }
        std::string_view digits = text.substr(position, digitsEnd - position);
        bool isSteamId = digits.size() == kSteamIdLength && digits.starts_with(kSteamIdPrefix);
        result += isSteamId ? std::string_view("<steam-id>") : digits;
        position = digitsEnd;
    }
    return result;
}

// Replaces the user's folder name in paths like C:\Users\Name\ and /home/name/.
//   text: the text to clean.
// Returns the cleaned text.
inline std::string ReplaceUserFolders(std::string_view text) {
    std::string result;
    size_t position = 0;
    while (position < text.size()) {
        result += text[position];
        if (!IsPathSeparator(text[position])) {
            position++;
            continue;
        }
        // Look at the segment after this separator, and the one after that.
        size_t segmentStart = position + 1;
        size_t segmentEnd = segmentStart;
        while (segmentEnd < text.size() && !IsPathSeparator(text[segmentEnd])) {
            segmentEnd++;
        }
        size_t nameStart = segmentEnd + 1;
        size_t nameEnd = nameStart;
        while (nameEnd < text.size() && !IsPathSeparator(text[nameEnd])) {
            nameEnd++;
        }
        std::string_view segment = text.substr(segmentStart, segmentEnd - segmentStart);
        bool hasUserName = nameEnd < text.size() && nameEnd > nameStart
                           && IsUserParentFolder(segment);
        if (hasUserName) {
            result += text.substr(segmentStart, nameStart - segmentStart);
            result += "<user>";
            position = nameEnd;
        } else {
            position++;
        }
    }
    return result;
}

// Removes every kind of personal detail this file knows about.
//   text: one log line, without its timestamp.
// Returns the cleaned line.
inline std::string RemovePrivateDetails(std::string_view text) {
    return ReplaceUserFolders(ReplaceSteamIds(text));
}

}  // namespace sgfixes::log_privacy
