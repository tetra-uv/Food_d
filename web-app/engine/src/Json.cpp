#include "Json.h"

#include <cmath>
#include <cstdlib>
#include <sstream>

namespace cbjson
{

JVal::JVal() : type_(T_NULL), b_(false), i_(0), d_(0.0) {}

JVal JVal::null() { return JVal(); }
JVal JVal::boolean(bool b) { JVal v; v.type_ = T_BOOL; v.b_ = b; return v; }
JVal JVal::integer(long long n) { JVal v; v.type_ = T_INT; v.i_ = n; return v; }
JVal JVal::number(double d) { JVal v; v.type_ = T_DBL; v.d_ = d; return v; }
JVal JVal::str(const std::string& s) { JVal v; v.type_ = T_STR; v.s_ = s; return v; }
JVal JVal::array() { JVal v; v.type_ = T_ARR; return v; }
JVal JVal::object() { JVal v; v.type_ = T_OBJ; return v; }

JVal& JVal::push(const JVal& v)
{
    if (type_ != T_ARR) { type_ = T_ARR; arr_.clear(); }
    arr_.push_back(v);
    return *this;
}

size_t JVal::size() const
{
    if (type_ == T_ARR) return arr_.size();
    if (type_ == T_OBJ) return obj_.size();
    return 0;
}

const JVal& JVal::at(size_t i) const { return arr_.at(i); }

JVal& JVal::set(const std::string& key, const JVal& v)
{
    if (type_ != T_OBJ) { type_ = T_OBJ; obj_.clear(); }
    for (size_t k = 0; k < obj_.size(); k++)
    {
        if (obj_[k].first == key) { obj_[k].second = v; return *this; }
    }
    obj_.push_back(std::make_pair(key, v));
    return *this;
}

const JVal* JVal::get(const std::string& key) const
{
    if (type_ != T_OBJ) return NULL;
    for (size_t k = 0; k < obj_.size(); k++)
    {
        if (obj_[k].first == key) return &obj_[k].second;
    }
    return NULL;
}

bool JVal::operator==(const JVal& o) const
{
    if (type_ != o.type_)
    {
        if (isNumber() && o.isNumber()) return asDouble() == o.asDouble();
        return false;
    }
    switch (type_)
    {
    case T_NULL: return true;
    case T_BOOL: return b_ == o.b_;
    case T_INT: return i_ == o.i_;
    case T_DBL: return d_ == o.d_;
    case T_STR: return s_ == o.s_;
    case T_ARR: return arr_ == o.arr_;
    case T_OBJ:
        if (obj_.size() != o.obj_.size()) return false;
        for (size_t k = 0; k < obj_.size(); k++)
        {
            const JVal* other = o.get(obj_[k].first);
            if (other == NULL || !(*other == obj_[k].second)) return false;
        }
        return true;
    }
    return false;
}

// ---------------------------------------------------------------- writer

static const char HEX[] = "0123456789abcdef";

// Length of a valid UTF-8 sequence at s[pos], or 0 when the bytes there are not valid UTF-8.
static int utf8len(const std::string& s, size_t pos)
{
    unsigned char c = (unsigned char)s[pos];
    size_t left = s.size() - pos;

    if (c < 0x80) return 1;
    if (c >= 0xC2 && c <= 0xDF)
    {
        if (left >= 2 && ((unsigned char)s[pos + 1] & 0xC0) == 0x80) return 2;
        return 0;
    }
    if (c >= 0xE0 && c <= 0xEF)
    {
        if (left < 3) return 0;
        unsigned char c1 = (unsigned char)s[pos + 1], c2 = (unsigned char)s[pos + 2];
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80) return 0;
        if (c == 0xE0 && c1 < 0xA0) return 0;          // overlong
        if (c == 0xED && c1 >= 0xA0) return 0;         // surrogates
        return 3;
    }
    if (c >= 0xF0 && c <= 0xF4)
    {
        if (left < 4) return 0;
        unsigned char c1 = (unsigned char)s[pos + 1], c2 = (unsigned char)s[pos + 2], c3 = (unsigned char)s[pos + 3];
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) return 0;
        if (c == 0xF0 && c1 < 0x90) return 0;          // overlong
        if (c == 0xF4 && c1 >= 0x90) return 0;         // above U+10FFFF
        return 4;
    }
    return 0;
}

void writeString(std::string& out, const std::string& s)
{
    out += '"';
    size_t pos = 0;
    while (pos < s.size())
    {
        unsigned char c = (unsigned char)s[pos];
        if (c == '"') { out += "\\\""; pos++; }
        else if (c == '\\') { out += "\\\\"; pos++; }
        else if (c == '\n') { out += "\\n"; pos++; }
        else if (c == '\r') { out += "\\r"; pos++; }
        else if (c == '\t') { out += "\\t"; pos++; }
        else if (c == '\b') { out += "\\b"; pos++; }
        else if (c == '\f') { out += "\\f"; pos++; }
        else if (c < 0x20 || c == 0x7F)
        {
            out += "\\u00";
            out += HEX[c >> 4];
            out += HEX[c & 15];
            pos++;
        }
        else if (c < 0x80) { out += (char)c; pos++; }
        else
        {
            int n = utf8len(s, pos);
            if (n == 0) { out += "\xEF\xBF\xBD"; pos++; }   // U+FFFD for an invalid byte
            else { out.append(s, pos, n); pos += n; }
        }
    }
    out += '"';
}

static void writeDouble(std::string& out, double d)
{
    if (!(d == d) || d > 1.7e308 || d < -1.7e308) { out += "null"; return; }
    std::stringstream text;
    text.precision(15);
    text << d;
    std::string s = text.str();
    if (std::strtod(s.c_str(), NULL) != d)
    {
        std::stringstream longer;
        longer.precision(17);
        longer << d;
        s = longer.str();
    }
    out += s;
}

void JVal::dumpTo(std::string& out) const
{
    switch (type_)
    {
    case T_NULL: out += "null"; break;
    case T_BOOL: out += b_ ? "true" : "false"; break;
    case T_INT:
    {
        std::stringstream text;
        text << i_;
        out += text.str();
        break;
    }
    case T_DBL: writeDouble(out, d_); break;
    case T_STR: writeString(out, s_); break;
    case T_ARR:
        out += '[';
        for (size_t k = 0; k < arr_.size(); k++)
        {
            if (k > 0) out += ',';
            arr_[k].dumpTo(out);
        }
        out += ']';
        break;
    case T_OBJ:
        out += '{';
        for (size_t k = 0; k < obj_.size(); k++)
        {
            if (k > 0) out += ',';
            writeString(out, obj_[k].first);
            out += ':';
            obj_[k].second.dumpTo(out);
        }
        out += '}';
        break;
    }
}

std::string JVal::dump() const
{
    std::string out;
    dumpTo(out);
    return out;
}

// ---------------------------------------------------------------- parser

namespace
{

class Parser
{
public:
    Parser(const std::string& text, int maxDepth) : s_(text), pos_(0), maxDepth_(maxDepth) {}

    bool run(JVal& out, std::string& error)
    {
        skipWs();
        if (!value(out, 0)) { error = err_; return false; }
        skipWs();
        if (pos_ != s_.size()) { fail("unexpected data after the value"); error = err_; return false; }
        return true;
    }

private:
    const std::string& s_;
    size_t pos_;
    int maxDepth_;
    std::string err_;

    bool fail(const std::string& what)
    {
        std::stringstream text;
        text << what << " at byte " << pos_;
        err_ = text.str();
        return false;
    }

    void skipWs()
    {
        while (pos_ < s_.size())
        {
            char c = s_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') pos_++;
            else break;
        }
    }

    bool lit(const char* word)
    {
        size_t n = 0;
        while (word[n] != 0) n++;
        if (s_.compare(pos_, n, word) == 0) { pos_ += n; return true; }
        return false;
    }

    bool value(JVal& out, int depth)
    {
        if (depth > maxDepth_) return fail("nesting is too deep");
        if (pos_ >= s_.size()) return fail("unexpected end of text");
        char c = s_[pos_];
        if (c == '{') return object(out, depth);
        if (c == '[') return array(out, depth);
        if (c == '"')
        {
            std::string text;
            if (!string(text)) return false;
            out = JVal::str(text);
            return true;
        }
        if (c == 't') { if (lit("true")) { out = JVal::boolean(true); return true; } return fail("bad literal"); }
        if (c == 'f') { if (lit("false")) { out = JVal::boolean(false); return true; } return fail("bad literal"); }
        if (c == 'n') { if (lit("null")) { out = JVal::null(); return true; } return fail("bad literal"); }
        if (c == '-' || (c >= '0' && c <= '9')) return number(out);
        return fail("unexpected character");
    }

    bool number(JVal& out)
    {
        size_t start = pos_;
        bool isInt = true;
        if (s_[pos_] == '-') pos_++;
        if (pos_ >= s_.size()) return fail("bad number");
        if (s_[pos_] == '0') pos_++;
        else if (s_[pos_] >= '1' && s_[pos_] <= '9') { while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') pos_++; }
        else return fail("bad number");
        if (pos_ < s_.size() && s_[pos_] == '.')
        {
            isInt = false;
            pos_++;
            size_t digits = 0;
            while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') { pos_++; digits++; }
            if (digits == 0) return fail("bad number");
        }
        if (pos_ < s_.size() && (s_[pos_] == 'e' || s_[pos_] == 'E'))
        {
            isInt = false;
            pos_++;
            if (pos_ < s_.size() && (s_[pos_] == '+' || s_[pos_] == '-')) pos_++;
            size_t digits = 0;
            while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') { pos_++; digits++; }
            if (digits == 0) return fail("bad number");
        }
        std::string text = s_.substr(start, pos_ - start);
        if (isInt && text.size() <= 18)
        {
            out = JVal::integer(std::atoll(text.c_str()));
            return true;
        }
        double d = std::strtod(text.c_str(), NULL);
        if (isInt && std::fabs(d) < 9.0e18) out = JVal::integer((long long)d);
        else out = JVal::number(d);
        return true;
    }

    static void appendUtf8(std::string& out, unsigned long cp)
    {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000)
        {
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
        else
        {
            out += (char)(0xF0 | (cp >> 18));
            out += (char)(0x80 | ((cp >> 12) & 0x3F));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
    }

    bool hex4(unsigned long& cp)
    {
        if (pos_ + 4 > s_.size()) return fail("bad unicode escape");
        cp = 0;
        for (int k = 0; k < 4; k++)
        {
            char c = s_[pos_ + k];
            cp <<= 4;
            if (c >= '0' && c <= '9') cp |= (unsigned long)(c - '0');
            else if (c >= 'a' && c <= 'f') cp |= (unsigned long)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') cp |= (unsigned long)(c - 'A' + 10);
            else return fail("bad unicode escape");
        }
        pos_ += 4;
        return true;
    }

    bool string(std::string& out)
    {
        pos_++;  // opening quote
        while (true)
        {
            if (pos_ >= s_.size()) return fail("unterminated string");
            unsigned char c = (unsigned char)s_[pos_];
            if (c == '"') { pos_++; return true; }
            if (c < 0x20) return fail("control character in string");
            if (c == '\\')
            {
                pos_++;
                if (pos_ >= s_.size()) return fail("unterminated string");
                char e = s_[pos_++];
                if (e == '"') out += '"';
                else if (e == '\\') out += '\\';
                else if (e == '/') out += '/';
                else if (e == 'b') out += '\b';
                else if (e == 'f') out += '\f';
                else if (e == 'n') out += '\n';
                else if (e == 'r') out += '\r';
                else if (e == 't') out += '\t';
                else if (e == 'u')
                {
                    unsigned long cp = 0;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF)
                    {
                        if (pos_ + 2 <= s_.size() && s_[pos_] == '\\' && s_[pos_ + 1] == 'u')
                        {
                            pos_ += 2;
                            unsigned long low = 0;
                            if (!hex4(low)) return false;
                            if (low < 0xDC00 || low > 0xDFFF) return fail("bad surrogate pair");
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                        }
                        else return fail("lone surrogate");
                    }
                    else if (cp >= 0xDC00 && cp <= 0xDFFF) return fail("lone surrogate");
                    appendUtf8(out, cp);
                }
                else return fail("bad escape");
                continue;
            }
            if (c < 0x80) { out += (char)c; pos_++; continue; }
            int n = utf8len(s_, pos_);
            if (n == 0) return fail("invalid UTF-8");
            out.append(s_, pos_, n);
            pos_ += n;
        }
    }

    bool array(JVal& out, int depth)
    {
        pos_++;
        out = JVal::array();
        skipWs();
        if (pos_ < s_.size() && s_[pos_] == ']') { pos_++; return true; }
        while (true)
        {
            skipWs();
            JVal item;
            if (!value(item, depth + 1)) return false;
            out.push(item);
            skipWs();
            if (pos_ >= s_.size()) return fail("unterminated array");
            if (s_[pos_] == ',') { pos_++; continue; }
            if (s_[pos_] == ']') { pos_++; return true; }
            return fail("expected , or ]");
        }
    }

    bool object(JVal& out, int depth)
    {
        pos_++;
        out = JVal::object();
        skipWs();
        if (pos_ < s_.size() && s_[pos_] == '}') { pos_++; return true; }
        while (true)
        {
            skipWs();
            if (pos_ >= s_.size() || s_[pos_] != '"') return fail("expected a key string");
            std::string key;
            if (!string(key)) return false;
            skipWs();
            if (pos_ >= s_.size() || s_[pos_] != ':') return fail("expected :");
            pos_++;
            skipWs();
            JVal item;
            if (!value(item, depth + 1)) return false;
            out.set(key, item);
            skipWs();
            if (pos_ >= s_.size()) return fail("unterminated object");
            if (s_[pos_] == ',') { pos_++; continue; }
            if (s_[pos_] == '}') { pos_++; return true; }
            return fail("expected , or }");
        }
    }
};

}  // namespace

bool parse(const std::string& text, JVal& out, std::string& error, int maxDepth)
{
    Parser parser(text, maxDepth);
    return parser.run(out, error);
}

}  // namespace cbjson
