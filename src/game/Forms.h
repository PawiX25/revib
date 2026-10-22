#ifndef FORMS_H
#define FORMS_H

#include "common.h"

#include "MemorySys.h"
#include <string>

#include "Fixed.h"
#include "MemPtr.h"

class Port;

/*
 * A form of a Scheme-like language, as text or in a binary format: nil, a
 * symbol, a string, a list of forms, a boolean, an integer or a fixed-point
 * number. (The game makes none but g_define, and reads none.)
 *
 * A Form is a handle sharing its value: it points to a letter, an object
 * of a class derived from Form that holds the value and counts the handles
 * pointing to it (the envelope/letter idiom). The handle's member
 * functions pass on to the letter's.
 */
class Form {
public:
    enum FormType {
        FORM_NIL,
        FORM_SYMBOL,
        FORM_STRING,
        FORM_LIST,
        FORM_BOOLEAN,
        FORM_INTEGER,
        FORM_FIXED
    };

    Form();
    /* a symbol or a string */
    Form(string text, bool symbol);
    /* a list of `count` forms; `copy`: copy them rather than take the array */
    Form(int count, Form* forms, bool copy);
    Form(bool value);
    Form(int value);
    Form(Fixed value);
    Form(const Form& other);
    Form operator=(const Form& other);
    virtual ~Form();

    virtual FormType Type() const;
    bool IsBoolean() const;
    virtual bool Equals(const Form& other) const;
    /* as text */
    virtual void Print(Port& port) const;
    /* in the binary format */
    virtual void Write(Port& port) const;
    /* a symbol's name, a string's text */
    virtual string String() const;
    /* anything but #f is true */
    virtual bool IsTrue() const;
    virtual int Integer() const;
    virtual Fixed FixedPoint() const;
    /* a list's */
    virtual int Length() const;
    virtual Form Element(int i) const;

protected:
    /* the constructor of a letter */
    enum Letter { LETTER };
    Form(Letter) : m_refs(1) {}

    union {
        Form* m_rep; /* a handle's letter */
        int m_refs;  /* a letter's count of handles */
    };
};

/*
 * Where forms are read from and written to, as characters or as the
 * binary format's bytes and words.
 */
class Port {
public:
    virtual ~Port() {}

    virtual void Put(char c) = 0;
    virtual void PutInt(int value) = 0;
    virtual void PutString(string text);
    virtual void Print(const Form& form);
    virtual void Write(const Form& form);

    virtual bool AtEof() = 0;
    virtual char Peek() = 0;
    /* Peek(), without checking for the end */
    virtual char PeekByte() = 0;
    virtual char Get() = 0;
    /* Get(), without checking for the end */
    virtual char GetByte() = 0;
    virtual void Skip() = 0;
    virtual int GetInt() = 0;
    virtual string GetString(int length) = 0;
    /* the next form of the text */
    virtual Form Read();

    virtual int SeekCurrent(int offset) = 0;
    virtual int SeekSet(int position) = 0;
    virtual int SeekEnd(int offset) = 0;
};

/* A file on the PC the game is run from (PSY-Q's PC file server). (The game makes none.) */
class HostPort : public Port {
public:
    HostPort(string name)
        : m_name(name), m_open(0), m_readable(0), m_writable(0), m_eof(0), m_peeked(0)
    {
    }
    virtual ~HostPort();

    virtual void Put(char c);
    virtual void PutInt(int value);
    virtual bool AtEof();
    virtual char Peek();
    virtual char PeekByte();
    virtual char Get();
    virtual char GetByte();
    virtual void Skip();
    virtual int GetInt();
    virtual string GetString(int length);
    virtual int SeekCurrent(int offset);
    virtual int SeekSet(int position);
    virtual int SeekEnd(int offset);

    virtual void Open(int read, int write);
    /* makes the file (empty) */
    virtual void Create();
    virtual void Close();
    virtual void PutBytes(char* bytes, int count);

private:
    string m_name;
    int m_open;
    int m_readable;
    int m_writable;
    int m_fd;
    int m_eof;
    int m_peeked; /* m_peek holds the next character */
    char m_peek;
};

/* A string, read and written at a position. (The game makes none.) */
class StringPort : public Port {
public:
    StringPort(string text) : m_text(text), m_position(0) {}
    virtual ~StringPort();

    virtual void Put(char c);
    virtual void PutInt(int value);
    virtual bool AtEof();
    virtual char Peek();
    virtual char PeekByte();
    virtual char Get();
    virtual char GetByte();
    virtual void Skip();
    virtual int GetInt();
    virtual string GetString(int length);
    virtual int SeekCurrent(int offset);
    virtual int SeekSet(int position);
    virtual int SeekEnd(int offset);

private:
    int Seek(int position);

    string m_text;
    int m_position;
};

/* A file loaded into memory, read only. (The game makes none.) */
class MemPtrPort : public Port {
public:
    MemPtrPort(const char* name);
    virtual ~MemPtrPort();

    virtual void Put(char c);
    virtual void PutInt(int value);
    virtual bool AtEof();
    virtual char Peek();
    virtual char PeekByte();
    virtual char Get();
    virtual char GetByte();
    virtual void Skip();
    virtual int GetInt();
    virtual string GetString(int length);
    virtual int SeekCurrent(int offset);
    virtual int SeekSet(int position);
    virtual int SeekEnd(int offset);

private:
    MemPtr<char> m_file;
    int m_position;
};

/* a form of the binary format */
Form ReadBinaryForm(Port& port);

/* the symbol `define` */
extern Form g_define;

#endif
