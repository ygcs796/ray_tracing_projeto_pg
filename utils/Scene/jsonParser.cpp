#pragma once
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

// ------------------------------------------------------------
// JSON VALUE
// ------------------------------------------------------------
class Json {
public:
    using array_t  = std::vector<Json>;
    using object_t = std::map<std::string, Json>;

    enum class Type {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

private:
    std::variant<std::nullptr_t, bool, double, std::string, array_t, object_t> data;

public:
    Json() : data(nullptr) {}
    Json(std::nullptr_t) : data(nullptr) {}
    Json(bool b) : data(b) {}
    Json(double d) : data(d) {}
    Json(int i) : data(static_cast<double>(i)) {}
    Json(long long i) : data(static_cast<double>(i)) {}
    Json(const std::string& s) : data(s) {}
    Json(std::string&& s) : data(std::move(s)) {}
    Json(const char* s) : data(std::string(s)) {}
    Json(const array_t& a) : data(a) {}
    Json(array_t&& a) : data(std::move(a)) {}
    Json(const object_t& o) : data(o) {}
    Json(object_t&& o) : data(std::move(o)) {}

    bool isNull()   const { return std::holds_alternative<std::nullptr_t>(data); }
    bool isBool()   const { return std::holds_alternative<bool>(data); }
    bool isNumber() const { return std::holds_alternative<double>(data); }
    bool isString() const { return std::holds_alternative<std::string>(data); }
    bool isArray()  const { return std::holds_alternative<array_t>(data); }
    bool isObject() const { return std::holds_alternative<object_t>(data); }

    bool asBool() const { return std::get<bool>(data); }
    double asNumber() const { return std::get<double>(data); }
    const std::string& asString() const { return std::get<std::string>(data); }
    const array_t& asArray() const { return std::get<array_t>(data); }
    const object_t& asObject() const { return std::get<object_t>(data); }

    array_t& asArray() { return std::get<array_t>(data); }
    object_t& asObject() { return std::get<object_t>(data); }
    const Json& at(const std::string& key) const { return asObject().at(key); }
    const Json& operator[](const std::string& key) const { return at(key); }
    const Json& operator[](size_t i) const { return asArray().at(i); }

    bool contains(const std::string& key) const {
        if (!isObject()) return false;
        const auto& o = asObject();
        return o.find(key) != o.end();
    }

    size_t size() const {
        if (isArray())  return asArray().size();
        if (isObject()) return asObject().size();
        throw std::runtime_error("Json::size() called on non-array/non-object");
    }
};

class JsonParser {
public:
    Json parseFile(const std::string& filename) {
        std::ifstream f(filename, std::ios::binary);
        if (!f) throw std::runtime_error("Could not open JSON file: " + filename);

        std::stringstream ss;
        ss << f.rdbuf();
        return parseString(ss.str());
    }

    Json parseString(const std::string& text) {
        src = text;
        pos = 0;
        line = 1;
        col = 1;

        skipIgnored();
        Json value = parseValue();
        skipIgnored();

        if (!eof()) error("Unexpected trailing content after root JSON value");
        return value;
    }

private:
    std::string src;
    size_t pos = 0;
    int line = 1;
    int col  = 1;

    bool eof() const { return pos >= src.size(); }

    char peek(size_t off = 0) const {
        size_t p = pos + off;
        if (p >= src.size()) return '\0';
        return src[p];
    }

    char advance() {
        if (eof()) return '\0';
        char c = src[pos++];
        if (c == '\n') {
            line++;
            col = 1;
        } else {
            col++;
        }
        return c;
    }

    [[noreturn]] void error(const std::string& msg) const {
        throw std::runtime_error(
            msg + " at line " + std::to_string(line) + ", column " + std::to_string(col)
        );
    }

    void expect(char c) {
        if (peek() != c) error(std::string("Expected '") + c + "'");
        advance();
    }

    void skipIgnored() {
        while (!eof()) {
            char c = peek();

            // whitespace
            if (std::isspace(static_cast<unsigned char>(c))) {
                advance();
                continue;
            }

            // # comment
            if (c == '#') {
                while (!eof() && peek() != '\n') advance();
                continue;
            }

            // // comment
            if (c == '/' && peek(1) == '/') {
                while (!eof() && peek() != '\n') advance();
                continue;
            }

            // /* block comment */
            if (c == '/' && peek(1) == '*') {
                advance();
                advance();
                while (!eof() && !(peek() == '*' && peek(1) == '/')) {
                    advance();
                }
                if (eof()) error("Unterminated block comment");
                advance();
                advance();
                continue;
            }

            break;
        }
    }

    Json parseValue() {
        skipIgnored();
        if (eof()) error("Unexpected end of input");

        char c = peek();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') return Json(parseStringLiteral());
        if (c == 't' || c == 'f' || c == 'n') return parseLiteral();
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parseNumber();

        error("Invalid JSON value");
        return Json(); // unreachable
    }

    Json parseObject() {
        expect('{');
        skipIgnored();

        Json::object_t obj;

        if (peek() == '}') {
            advance();
            return Json(std::move(obj));
        }

        while (true) {
            skipIgnored();
            if (peek() != '"') {
                error("Expected string key in object");
            }

            std::string key = parseStringLiteral();
            skipIgnored();
            expect(':');
            Json value = parseValue();

            obj.emplace(std::move(key), std::move(value));

            skipIgnored();
            if (peek() == '}') {
                advance();
                break;
            }

            expect(',');
        }

        return Json(std::move(obj));
    }

    Json parseArray() {
        expect('[');
        skipIgnored();

        Json::array_t arr;

        if (peek() == ']') {
            advance();
            return Json(std::move(arr));
        }

        while (true) {
            arr.push_back(parseValue());
            skipIgnored();

            if (peek() == ']') {
                advance();
                break;
            }

            expect(',');
        }

        return Json(std::move(arr));
    }

    Json parseLiteral() {
        if (src.compare(pos, 4, "true") == 0) {
            pos += 4;
            col += 4;
            return Json(true);
        }
        if (src.compare(pos, 5, "false") == 0) {
            pos += 5;
            col += 5;
            return Json(false);
        }
        if (src.compare(pos, 4, "null") == 0) {
            pos += 4;
            col += 4;
            return Json(nullptr);
        }

        error("Invalid literal");
        return Json(); // unreachable
    }

    Json parseNumber() {
        size_t start = pos;

        if (peek() == '-') advance();

        if (!std::isdigit(static_cast<unsigned char>(peek()))) {
            error("Invalid number");
        }

        if (peek() == '0') {
            advance();
        } else {
            while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
        }

        if (peek() == '.') {
            advance();
            if (!std::isdigit(static_cast<unsigned char>(peek()))) {
                error("Invalid fractional part in number");
            }
            while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
        }

        if (peek() == 'e' || peek() == 'E') {
            advance();
            if (peek() == '+' || peek() == '-') advance();
            if (!std::isdigit(static_cast<unsigned char>(peek()))) {
                error("Invalid exponent in number");
            }
            while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
        }

        std::string num = src.substr(start, pos - start);
        try {
            return Json(std::stod(num));
        } catch (...) {
            error("Could not parse number: " + num);
        }

        return Json(); // unreachable
    }

    static int hexValue(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    }

    void appendUtf8(std::string& out, uint32_t cp) {
        if (cp <= 0x7F) {
            out.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    uint32_t parseHex4() {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            if (eof()) error("Unexpected end in unicode escape");
            int h = hexValue(advance());
            if (h < 0) error("Invalid hex digit in unicode escape");
            v = (v << 4) | static_cast<uint32_t>(h);
        }
        return v;
    }

    std::string parseStringLiteral() {
        expect('"');
        std::string out;

        while (!eof()) {
            char c = advance();
            if (c == '"') break;

            if (c == '\\') {
                if (eof()) error("Invalid escape sequence");

                char e = advance();
                switch (e) {
                    case '"':  out.push_back('"');  break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/');  break;
                    case 'b':  out.push_back('\b'); break;
                    case 'f':  out.push_back('\f'); break;
                    case 'n':  out.push_back('\n'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 't':  out.push_back('\t'); break;

                    case 'u': {
                        uint32_t cp = parseHex4();

                        // Handle surrogate pair
                        if (cp >= 0xD800 && cp <= 0xDBFF) {
                            if (peek() == '\\' && peek(1) == 'u') {
                                advance(); // \
                                advance(); // u
                                uint32_t low = parseHex4();
                                if (low < 0xDC00 || low > 0xDFFF) {
                                    error("Invalid UTF-16 surrogate pair");
                                }
                                cp = 0x10000 + (((cp - 0xD800) << 10) | (low - 0xDC00));
                            } else {
                                error("High surrogate without low surrogate");
                            }
                        }

                        appendUtf8(out, cp);
                        break;
                    }

                    default:
                        error("Unknown escape sequence");
                }
            } else {
                out.push_back(c);
            }
        }

        return out;
    }
};