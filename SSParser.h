#pragma once

#include <string_view>
#include <cctype>

class SSParser {
    std::string_view m_str;
    const char * m_original_pointer;
public:
    SSParser() = default;

    SSParser(std::string_view str)
        : m_str(str)
        , m_original_pointer(str.data())
    {}

    SSParser(std::string_view str, const char* original_pointer)
        : m_str(str)
        , m_original_pointer(original_pointer)
    {}

    size_t size() const {
        return m_str.size();
    }

    std::string_view to_string_view() const {
        return m_str;
    }

    std::string to_string() const {
        return std::string{m_str.data(), m_str.size()};
    }

    // check what next and move pointer after expected
    bool expect(std::string_view expected) {
        if (m_str.starts_with(expected)) {
            m_str.remove_prefix(expected.size());
            return true;
        }

        return false;
    }

    // just check what next
    bool peek_is(std::string_view expected) const {
        return m_str.starts_with(expected);
    }

    // find expected string and move pointer after expected
    bool to(std::string_view expected) {
        const size_t pos = m_str.find(expected);
        if (pos == std::string_view::npos) {
            return false;
        }

        m_str.remove_prefix(pos + expected.size());
        return true;
    }

    SSParser extract(std::string_view from, std::string_view to) {
        auto startIt = m_str.find(from);
        if (startIt == std::string::npos) {
            return {};
        }
        startIt += from.size();

        const auto endIt = m_str.find(to);
        return SSParser(m_str.substr(startIt, endIt), m_original_pointer);
    }

    SSParser extract(std::string_view to) {
        const auto endIt = m_str.find(to);
        return SSParser(m_str.substr(0, endIt), m_original_pointer);
    }

    SSParser extract(size_t size) const {
        return SSParser(m_str.substr(0, size), m_original_pointer);
    }

    void skip(size_t length) {
        m_str.remove_prefix(length);
    }

    uint64_t take_uint64() {
        char* end = nullptr;
        const auto result = std::strtoull(m_str.data(), &end, 10);
        m_str.remove_prefix(end - m_str.data());
        return result;
    }

    int64_t take_int64() {
        char* end = nullptr;
        const auto result = std::strtoll(m_str.data(), &end, 10);
        m_str.remove_prefix(end - m_str.data());
        return result;
    }

    void skip_white_spaces() {
        while (m_str.size() && std::isspace(static_cast<unsigned char>(m_str[0]))) {
            m_str.remove_prefix(1);
        }
    }

    size_t get_original_position() {
        return m_str.data() - m_original_pointer;
    }

    friend std::ostream& operator<<(std::ostream& oss, const SSParser& parser) {
        return oss << parser.to_string_view();
    }
};
