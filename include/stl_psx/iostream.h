/*
 * There is no iostream library on the PlayStation. The STL headers only use
 * these classes in stream helpers (operator<< / >> / getline for strings)
 * that the game never calls, so declarations are enough.
 */
#ifndef STL_PSX_IOSTREAM_H
#define STL_PSX_IOSTREAM_H
class streambuf {
public:
    int sputc(int);
    int sputn(const char*, int);
    int sbumpc();
    int sputbackc(char);
};
class ios {
public:
    enum { skipws = 1, left = 2 };
    enum { goodbit = 0, eofbit = 1, failbit = 2, badbit = 4 };
    streambuf* rdbuf() const;
    int  flags() const;
    int  width() const;
    int  width(int);
    char fill() const;
    int  rdstate() const;
    void clear(int = 0);
    operator void*() const;
    int  operator!() const;
};
class ostream : public ios {};
class istream : public ios {
public:
    istream& get(char&);
    istream& putback(char);
};
#ifndef isspace /* <ctype.h> may define it as a macro */
extern "C" int isspace(int);
#endif
#endif
