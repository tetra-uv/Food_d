#ifndef CB_JSON_H
#define CB_JSON_H
// Json.h: a small JSON value, a strict parser and a writer (C++11 style, no external library).
// Strings are UTF-8. The parser rejects invalid UTF-8, trailing commas, comments, duplicate-free is not enforced
// (the last duplicate key wins), depth is capped. The writer replaces invalid UTF-8 bytes with U+FFFD.

#include <string>
#include <utility>
#include <vector>

namespace cbjson
{

class JVal
{
public:
    enum Type { T_NULL, T_BOOL, T_INT, T_DBL, T_STR, T_ARR, T_OBJ };

    JVal();                                  // null
    static JVal null();
    static JVal boolean(bool b);
    static JVal integer(long long n);
    static JVal number(double d);
    static JVal str(const std::string& s);
    static JVal array();
    static JVal object();

    Type type() const { return type_; }
    bool isNull() const { return type_ == T_NULL; }
    bool isBool() const { return type_ == T_BOOL; }
    bool isInt() const { return type_ == T_INT; }
    bool isNumber() const { return type_ == T_INT || type_ == T_DBL; }
    bool isStr() const { return type_ == T_STR; }
    bool isArr() const { return type_ == T_ARR; }
    bool isObj() const { return type_ == T_OBJ; }

    bool asBool() const { return b_; }
    long long asInt() const { return i_; }
    double asDouble() const { return type_ == T_INT ? (double)i_ : d_; }
    const std::string& asStr() const { return s_; }

    // arrays
    JVal& push(const JVal& v);               // returns *this for chaining
    size_t size() const;                     // array or object size
    const JVal& at(size_t i) const;
    const std::vector<JVal>& items() const { return arr_; }

    // objects (insertion ordered)
    JVal& set(const std::string& key, const JVal& v);   // returns *this for chaining
    const JVal* get(const std::string& key) const;      // NULL when absent
    bool has(const std::string& key) const { return get(key) != NULL; }
    const std::vector<std::pair<std::string, JVal> >& members() const { return obj_; }

    // text
    std::string dump() const;
    void dumpTo(std::string& out) const;

    bool operator==(const JVal& other) const;

private:
    Type type_;
    bool b_;
    long long i_;
    double d_;
    std::string s_;
    std::vector<JVal> arr_;
    std::vector<std::pair<std::string, JVal> > obj_;
};

// Parses one complete JSON text. Returns false and fills error (with a byte offset) on any problem.
bool parse(const std::string& text, JVal& out, std::string& error, int maxDepth = 32);

// Appends s as a JSON string literal (with quotes) to out.
void writeString(std::string& out, const std::string& s);

}  // namespace cbjson

#endif
