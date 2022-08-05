#ifndef __STIRNG_HELPER_H__
#define __STIRNG_HELPER_H__

#include <locale>
#include <codecvt>
#include <type_traits>

#include <string>

/**
 * @brief Use string_helper to convert back and forth between std::string and std::wstring under utf-8
 * for example. string_helper<std::string>(L"asd") or string_helper<std::wstring>("adf");
 * @remark if you not use utf-8, you should add local infomation into function to_wide_string and to_byte_string 
 */
namespace string_helper
{
    struct to_wide_string
    {
        std::wstring operator()(const std::string &input)
        {
            static std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            return converter.from_bytes(input);
        }
    };

    struct to_byte_string
    {
        std::string operator()(const std::wstring &input)
        {
            static std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            return converter.to_bytes(input);
        }
    };

    /**
     * @brief a template to check if the Args can be used in F's constructors.
     *      for example , decltype(check<std::string>::t(3, 'a')) == std::true_type
     */
    template <class F>
    struct check
    {
        template <class... GArgs>
        static auto t(int, GArgs &&...args) -> decltype(F(std::forward<GArgs>(args)...), std::true_type());
        template <class...>
        static auto t(...) -> std::false_type;
    };

    /**
     * @brief can change all str-like to TSTRING under utf-8 coding
     */
    template <typename T, typename... Args>
    T to_tstring(Args &&...args)
    {
        return to_tstring_impl<T>(decltype(check<T>::t(0, std::forward<Args>(args)...))(), std::forward<Args>(args)...);
    }
    template <typename T, typename... Args>
    T to_tstring_impl(std::false_type, Args &&...args)
    {
        return std::conditional<
            std::is_same<T, std::string>::value,
            to_byte_string,
            to_wide_string>::type()(std::forward<Args>(args)...);
    }
    template <typename T, typename... Args>
    T to_tstring_impl(std::true_type, Args &&...args)
    {
        return T(std::forward<Args>(args)...);
    }
}

#endif // __STIRNG_HELPER_H__