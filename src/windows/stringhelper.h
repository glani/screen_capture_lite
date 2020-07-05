//
// Created by denis on 04/07/2020.
//

#ifndef SIMPLE_SCREENSHOT_MAKER_STRINGHELPER_H
#define SIMPLE_SCREENSHOT_MAKER_STRINGHELPER_H


#include <string>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <cstdarg>
#include <vector>
#include <codecvt>
#include <locale>
#include <string>
#include <iostream>

using convert_type = std::codecvt_utf8<wchar_t>;
class StringHelper
{
public:

    static std::wstring_convert<convert_type, wchar_t> converter;

    static std::string toLower(std::string source)
    {
        std::transform(source.begin(), source.end(), source.begin(), (int (*)(int))std::tolower);
        return source;
    }

    static std::string toUpper(std::string source)
    {
        std::transform(source.begin(), source.end(), source.begin(), (int (*)(int))std::toupper);
        return source;
    }

    static std::string trimStart(std::string source, const std::string &trimChars = " \t\n\r\v\f")
    {
        return source.erase(0, source.find_first_not_of(trimChars));
    }

    static std::string trimEnd(std::string source, const std::string &trimChars = " \t\n\r\v\f")
    {
        return source.erase(source.find_last_not_of(trimChars) + 1);
    }

    static std::string trim(std::string source, const std::string &trimChars = " \t\n\r\v\f")
    {
        return trimStart(trimEnd(source, trimChars), trimChars);
    }

    static std::string replace(std::string source, const std::string &find, const std::string &replace)
    {
        std::size_t pos = 0;
        while ((pos = source.find(find, pos)) != std::string::npos)
        {
            source.replace(pos, find.length(), replace);
            pos += replace.length();
        }
        return source;
    }

    static bool startsWith(const std::string &source, const std::string &value)
    {
        if (source.length() < value.length())
            return false;
        else
            return source.compare(0, value.length(), value) == 0;
    }

    static bool endsWith(const std::string &source, const std::string &value)
    {
        if (source.length() < value.length())
            return false;
        else
            return source.compare(source.length() - value.length(), value.length(), value) == 0;
    }

    static std::vector<std::string> split(const std::string &source, char delimiter)
    {
        std::vector<std::string> output;
        std::istringstream ss(source);
        std::string nextItem;

        while (std::getline(ss, nextItem, delimiter))
        {
            output.push_back(nextItem);
        }

        return output;
    }

    static std::string format(const char *fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        std::vector<char> v(1024);
        while (true)
        {
            va_list args2;
            va_copy(args2, args);
            int res = vsnprintf(v.data(), v.size(), fmt, args2);
            if ((res >= 0) && (res < static_cast<int>(v.size())))
            {
                va_end(args);
                va_end(args2);
                return std::string(v.data());
            }
            size_t size;
            if (res < 0)
                size = v.size() * 2;
            else
                size = static_cast<size_t>(res) + 1;
            v.clear();
            v.resize(size);
            va_end(args2);
        }
    }

    template<typename T>
    static std::string toString(const T &subject)
    {
        std::ostringstream ss;
        ss << subject;
        return ss.str();
    }

    template<typename T>
    static T fromString(const std::string &subject)
    {
        std::istringstream ss(subject);
        T target;
        ss >> target;
        return target;
    }

    static bool isEmptyOrWhiteSpace(const std::string &source)
    {
        if (source.length() == 0)
            return true;
        else
        {
            for (std::size_t index = 0; index < source.length(); index++)
            {
                if (!std::isspace(source[index]))
                    return false;
            }

            return true;
        }
    }

    template<typename T>
    static std::string formatSimple(const std::string &input, T arg1)
    {
        std::ostringstream ss;
        std::size_t lastFormatChar = std::string::npos;
        std::size_t percent = std::string::npos;
        while ((percent = input.find(L'%', percent + 1)) != std::string::npos)
        {
            if (percent + 1 < input.length())
            {
                if (input[percent + 1] == L'%')
                {
                    percent++;
                    continue;
                }

                std::size_t formatEnd = std::string::npos;
                std::string index;
                for (std::size_t i = percent + 1; i < input.length(); i++)
                {
                    if (input[i] == 's')
                    {
                        index = "1";
                        formatEnd = i;
                        break;
                    }
                    else if (input[i] == '$' && i + 1 < input.length() && input[i + 1] == 's')
                    {
                        index = input.substr(percent + 1, i - percent - 1);
                        formatEnd = i + 1;
                        break;
                    }
                    else if (!std::isdigit(input[i]))
                        break;
                }

                if (formatEnd != std::string::npos)
                {
                    ss << input.substr(lastFormatChar + 1, percent - lastFormatChar - 1);
                    lastFormatChar = formatEnd;

                    if (index == "1")
                        ss << arg1;
                    else
                        throw std::runtime_error("Only simple positional format specifiers are handled by the 'formatSimple' helper method.");
                }
            }
        }

        if (lastFormatChar + 1 < input.length())
            ss << input.substr(lastFormatChar + 1);

        return ss.str();
    }

    template<typename T1, typename T2>
    static std::string formatSimple(const std::string &input, T1 arg1, T2 arg2)
    {
        std::ostringstream ss;
        std::size_t lastFormatChar = std::string::npos;
        std::size_t percent = std::string::npos;
        while ((percent = input.find(L'%', percent + 1)) != std::string::npos)
        {
            if (percent + 1 < input.length())
            {
                if (input[percent + 1] == L'%')
                {
                    percent++;
                    continue;
                }

                std::size_t formatEnd = std::string::npos;
                std::string index;
                for (std::size_t i = percent + 1; i < input.length(); i++)
                {
                    if (input[i] == 's')
                    {
                        index = "1";
                        formatEnd = i;
                        break;
                    }
                    else if (input[i] == '$' && i + 1 < input.length() && input[i + 1] == 's')
                    {
                        index = input.substr(percent + 1, i - percent - 1);
                        formatEnd = i + 1;
                        break;
                    }
                    else if (!std::isdigit(input[i]))
                        break;
                }

                if (formatEnd != std::string::npos)
                {
                    ss << input.substr(lastFormatChar + 1, percent - lastFormatChar - 1);
                    lastFormatChar = formatEnd;

                    if (index == "1")
                        ss << arg1;
                    else if (index == "2")
                        ss << arg2;
                    else
                        throw std::runtime_error("Only simple positional format specifiers are handled by the 'formatSimple' helper method.");
                }
            }
        }

        if (lastFormatChar + 1 < input.length())
            ss << input.substr(lastFormatChar + 1);

        return ss.str();
    }

    template<typename T1, typename T2, typename T3>
    static std::string formatSimple(const std::string &input, T1 arg1, T2 arg2, T3 arg3)
    {
        std::ostringstream ss;
        std::size_t lastFormatChar = std::string::npos;
        std::size_t percent = std::string::npos;
        while ((percent = input.find(L'%', percent + 1)) != std::string::npos)
        {
            if (percent + 1 < input.length())
            {
                if (input[percent + 1] == L'%')
                {
                    percent++;
                    continue;
                }

                std::size_t formatEnd = std::string::npos;
                std::string index;
                for (std::size_t i = percent + 1; i < input.length(); i++)
                {
                    if (input[i] == 's')
                    {
                        index = "1";
                        formatEnd = i;
                        break;
                    }
                    else if (input[i] == '$' && i + 1 < input.length() && input[i + 1] == 's')
                    {
                        index = input.substr(percent + 1, i - percent - 1);
                        formatEnd = i + 1;
                        break;
                    }
                    else if (!std::isdigit(input[i]))
                        break;
                }

                if (formatEnd != std::string::npos)
                {
                    ss << input.substr(lastFormatChar + 1, percent - lastFormatChar - 1);
                    lastFormatChar = formatEnd;

                    if (index == "1")
                        ss << arg1;
                    else if (index == "2")
                        ss << arg2;
                    else if (index == "3")
                        ss << arg3;
                    else
                        throw std::runtime_error("Only simple positional format specifiers are handled by the 'formatSimple' helper method.");
                }
            }
        }

        if (lastFormatChar + 1 < input.length())
            ss << input.substr(lastFormatChar + 1);

        return ss.str();
    }

    static std::wstring s2ws(const char* value)
    {
        return converter.from_bytes(value);
    }

    static std::wstring s2ws(const std::string& str)
    {
        return converter.from_bytes(str);
    }

    static std::string ws2s(const std::wstring& wstr)
    {
        return converter.to_bytes(wstr);
    }
};

#endif //SIMPLE_SCREENSHOT_MAKER_STRINGHELPER_H
