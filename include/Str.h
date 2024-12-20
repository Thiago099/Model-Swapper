#pragma once

namespace Str {


    inline std::string removePrefix(std::string prefix, std::string text) {
        std::string lowerPrefix = prefix;
        std::string lowerText = text.substr(0, prefix.length()); 

        std::transform(lowerPrefix.begin(), lowerPrefix.end(), lowerPrefix.begin(), ::tolower);
        std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);

        if (lowerText == lowerPrefix) {
            text.erase(0, prefix.length());
        }
        return text;
    }

    inline char* CopySaveFileNameWithoutExtension(const char* input) {
        size_t len = std::strlen(input);
        if (len >= 4 && std::strcmp(input + len - 4, ".ess") == 0) {
            char* result = new char[len - 3];
            std::strncpy(result, input, len - 4);
            result[len - 4] = '\0';
            return result;
        }
        char* result = new char[len + 1];
        std::strcpy(result, input);
        return result;
    }

    inline bool Equal(const char* a1, const char* a2) {
        return strcmp(a1,a2) == 0;
    }
	

    inline std::string processString(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return std::tolower(c); });
        str = std::regex_replace(str, std::regex("[\\\\/]+"), "-");
        str = removePrefix("meshes-", str);
        return str;
    }


    inline bool compare(std::string a, std::string b) {

        if (a.size() != b.size()) {
            return false;
        }

        return processString(a) == processString(b);
    }


}