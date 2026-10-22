#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <libsn.h>

#include "Forms.h" /* (MemorySys.h before the STL) */
#include <list>
#include "Format.h"

/* (in Format.cpp) */
string ToString(int value);
string ToString(Fixed value);

/* The letters: the values forms share. */

/* What a letter does with what its kind of form does not have: it stops the game. */
class FormRep : public Form {
public:
    FormRep() : Form(LETTER) {}
    virtual FormType Type();
    virtual bool Equals(const Form& other) const;
    virtual void Print(Port& port) const;
    virtual void Write(Port& port) const;
    virtual string String() const;
    virtual bool IsTrue() const;
    virtual int Integer() const;
    virtual Fixed FixedPoint() const;
    virtual int Length() const;
    virtual Form Element(int i) const;
};

class NullForm : public FormRep {
public:
    NullForm();
    virtual FormType Type() const;
    virtual bool Equals(const Form& other) const;
    virtual void Print(Port& port) const;
    virtual void Write(Port& port) const;
};

class SymbolForm : public FormRep {
public:
    SymbolForm(string name);
    virtual FormType Type() const;
    virtual bool Equals(const Form& other) const;
    virtual void Print(Port& port) const;
    virtual void Write(Port& port) const;
    virtual string String() const;

private:
    string m_name;
};

class StringForm : public FormRep {
public:
    StringForm(string text);
    virtual FormType Type() const;
    virtual bool Equals(const Form& other) const;
    virtual void Print(Port& port) const;
    virtual void Write(Port& port) const;
    virtual string String() const;

private:
    string m_text;
};

class BooleanForm : public FormRep {
public:
    BooleanForm(bool value);
    virtual FormType Type() const;
    virtual bool Equals(const Form& other) const;
    virtual void Print(Port& port) const;
    virtual void Write(Port& port) const;
    virtual bool IsTrue() const;

private:
    bool m_value;
};

class IntegerForm : public FormRep {
public:
    IntegerForm(int value);
    virtual FormType Type() const;
    virtual bool Equals(const Form& other) const;
    virtual void Print(Port& port) const;
    virtual void Write(Port& port) const;
    virtual int Integer() const;

private:
    int m_value;
};

class FixedForm : public FormRep {
public:
    FixedForm(Fixed value);
    virtual FormType Type() const;
    virtual bool Equals(const Form& other) const;
    virtual void Print(Port& port) const;
    virtual void Write(Port& port) const;
    virtual Fixed FixedPoint() const;

private:
    Fixed m_value;
};

class ListForm : public FormRep {
public:
    ListForm(int count, Form* forms, bool copy);
    virtual ~ListForm();

    virtual FormType Type() const;
    virtual bool Equals(const Form& other) const;
    virtual void Print(Port& port) const;
    virtual void Write(Port& port) const;
    virtual int Length() const;
    virtual Form Element(int i) const;

private:
    int m_count;
    Form* m_forms;
};

/* The handle. */

Form::Form() : m_rep(new NullForm)
{
}

Form::Form(string text, bool symbol)
{
    if (symbol) {
        m_rep = new SymbolForm(text);
    } else {
        m_rep = new StringForm(text);
    }
}

Form::Form(int count, Form* forms, bool copy) : m_rep(new ListForm(count, forms, copy))
{
}

Form::Form(bool value) : m_rep(new BooleanForm(value))
{
}

Form::Form(int value) : m_rep(new IntegerForm(value))
{
}

Form::Form(Fixed value) : m_rep(new FixedForm(value))
{
}

Form::Form(const Form& other)
{
    ++other.m_rep->m_refs;
    m_rep = other.m_rep;
}

Form Form::operator=(const Form& other)
{
    ++other.m_rep->m_refs;
    --m_rep->m_refs;
    if (m_rep->m_refs == 0) {
        delete m_rep;
    }
    m_rep = other.m_rep;
    return *this;
}

/*
 * Also run for a letter going away, whose count (then 0) is taken for a
 * pointer: it decrements the word at address 0.
 */
Form::~Form()
{
    --m_rep->m_refs;
    if (m_rep->m_refs == 0) {
        delete m_rep;
    }
}

Form::FormType Form::Type() const
{
    return m_rep->Type();
}

bool Form::IsBoolean() const
{
    return m_rep->Type() == FORM_BOOLEAN;
}

bool Form::Equals(const Form& other) const
{
    if (m_rep->Type() != other.m_rep->Type()) {
        return false;
    }
    return m_rep->Equals(other);
}

void Form::Print(Port& port) const
{
    m_rep->Print(port);
}

void Form::Write(Port& port) const
{
    m_rep->Write(port);
}

string Form::String() const
{
    return m_rep->String();
}

bool Form::IsTrue() const
{
    return m_rep->IsTrue();
}

int Form::Integer() const
{
    return m_rep->Integer();
}

Fixed Form::FixedPoint() const
{
    Fixed value = m_rep->FixedPoint();
    return value;
}

int Form::Length() const
{
    return m_rep->Length();
}

Form Form::Element(int i) const
{
    return m_rep->Element(i);
}

/*
 * Not Form::Type(), which every letter has: a different function as it is
 * not const.
 */
Form::FormType FormRep::Type()
{
#line 252 "C:/psj/dev/locale/game/Forms.cpp"
    assert(0);
#line 253 "src/game/Forms.cpp"
}

bool FormRep::Equals(const Form& other) const
{
#line 258 "C:/psj/dev/locale/game/Forms.cpp"
    assert(0);
#line 260 "src/game/Forms.cpp"
}

void FormRep::Print(Port& port) const
{
#line 265 "C:/psj/dev/locale/game/Forms.cpp"
    assert(0);
#line 267 "src/game/Forms.cpp"
}

void FormRep::Write(Port& port) const
{
#line 270 "C:/psj/dev/locale/game/Forms.cpp"
    assert(0);
#line 274 "src/game/Forms.cpp"
}

string FormRep::String() const
{
#line 275 "C:/psj/dev/locale/game/Forms.cpp"
    assert(0);
#line 281 "src/game/Forms.cpp"
}

bool FormRep::IsTrue() const
{
    return true;
}

int FormRep::Integer() const
{
#line 286 "C:/psj/dev/locale/game/Forms.cpp"
    assert(0);
#line 293 "src/game/Forms.cpp"
}

Fixed FormRep::FixedPoint() const
{
#line 292 "C:/psj/dev/locale/game/Forms.cpp"
    assert(0);
#line 300 "src/game/Forms.cpp"
}

int FormRep::Length() const
{
#line 298 "C:/psj/dev/locale/game/Forms.cpp"
    assert(0);
#line 307 "src/game/Forms.cpp"
}

Form FormRep::Element(int i) const
{
#line 304 "C:/psj/dev/locale/game/Forms.cpp"
    assert(0);
#line 314 "src/game/Forms.cpp"
}

NullForm::NullForm()
{
}

Form::FormType NullForm::Type() const
{
    return FORM_NIL;
}

bool NullForm::Equals(const Form& other) const
{
    return true;
}

void NullForm::Print(Port& port) const
{
    printf("Cannot write null form.\n");
#line 318 "C:/psj/dev/locale/game/Forms.cpp"
    assert(0);
#line 336 "src/game/Forms.cpp"
}

void NullForm::Write(Port& port) const
{
    printf("Cannot write null form.\n");
#line 324 "C:/psj/dev/locale/game/Forms.cpp"
    assert(0);
#line 344 "src/game/Forms.cpp"
}

SymbolForm::SymbolForm(string name) : m_name(name)
{
}

Form::FormType SymbolForm::Type() const
{
    return FORM_SYMBOL;
}

bool SymbolForm::Equals(const Form& other) const
{
    return m_name == other.String();
}

void SymbolForm::Print(Port& port) const
{
    port.PutString(m_name);
}

void SymbolForm::Write(Port& port) const
{
    port.Put(FORM_SYMBOL);
    port.PutInt(m_name.size());
    port.PutString(m_name);
}

string SymbolForm::String() const
{
    return m_name;
}

StringForm::StringForm(string text) : m_text(text)
{
}

Form::FormType StringForm::Type() const
{
    return FORM_STRING;
}

bool StringForm::Equals(const Form& other) const
{
    return m_text == other.String();
}

void StringForm::Print(Port& port) const
{
    port.Put('"');
    for (const char* p = m_text.begin(); p < m_text.end(); p++) {
        char c = *p;
        if (c == '"' || c == '\\') {
            port.Put('\\');
        }
        port.Put(c);
    }
    port.Put('"');
}

void StringForm::Write(Port& port) const
{
    port.Put(FORM_STRING);
    port.PutInt(m_text.size());
    port.PutString(m_text);
}

string StringForm::String() const
{
    return m_text;
}

BooleanForm::BooleanForm(bool value) : m_value(value)
{
}

Form::FormType BooleanForm::Type() const
{
    return FORM_BOOLEAN;
}

bool BooleanForm::Equals(const Form& other) const
{
    return other.IsBoolean() && m_value == other.IsTrue();
}

void BooleanForm::Print(Port& port) const
{
    port.Put('#');
    if (m_value) {
        port.Put('t');
    } else {
        port.Put('f');
    }
}

void BooleanForm::Write(Port& port) const
{
    port.Put(FORM_BOOLEAN);
    port.Put(m_value);
}

bool BooleanForm::IsTrue() const
{
    return m_value;
}

IntegerForm::IntegerForm(int value) : m_value(value)
{
}

Form::FormType IntegerForm::Type() const
{
    return FORM_INTEGER;
}

/* (the values are not compared: any two integers are equal) */
bool IntegerForm::Equals(const Form& other) const
{
    return true;
}

void IntegerForm::Print(Port& port) const
{
    port.PutString(ToString(m_value));
}

void IntegerForm::Write(Port& port) const
{
    port.Put(FORM_INTEGER);
    port.PutInt(m_value);
}

int IntegerForm::Integer() const
{
    return m_value;
}

FixedForm::FixedForm(Fixed value) : m_value(value)
{
}

Form::FormType FixedForm::Type() const
{
    return FORM_FIXED;
}

/* (any two fixed-point numbers are equal too) */
bool FixedForm::Equals(const Form& other) const
{
    return true;
}

void FixedForm::Print(Port& port) const
{
    port.PutString(ToString(m_value));
}

void FixedForm::Write(Port& port) const
{
    port.Put(FORM_FIXED);
    port.PutInt(m_value.Raw());
}

Fixed FixedForm::FixedPoint() const
{
    return m_value;
}

ListForm::ListForm(int count, Form* forms, bool copy) : m_count(count)
{
    if (copy) {
        m_forms = new Form[count];
        for (int i = 0; i < m_count; i++) {
            m_forms[i] = forms[i];
        }
    } else {
        m_forms = forms;
    }
}

ListForm::~ListForm()
{
    delete[] m_forms;
}

Form::FormType ListForm::Type() const
{
    return FORM_LIST;
}

bool ListForm::Equals(const Form& other) const
{
    if (m_count != other.Length()) {
        return false;
    }
    for (int i = 0; i < m_count; i++) {
        if (!m_forms[i].Equals(other.Element(i))) {
            return false;
        }
    }
    return true;
}

void ListForm::Print(Port& port) const
{
    port.Put('(');
    if (m_count > 0) {
        m_forms[0].Print(port);
        if (m_count > 1) {
            for (int i = 1; i < m_count; i++) {
                port.Put(' ');
                m_forms[i].Print(port);
            }
        }
    }
    port.Put(')');
}

void ListForm::Write(Port& port) const
{
    port.Put(FORM_LIST);
    port.PutInt(m_count);
    for (int i = 0; i < m_count; i++) {
        m_forms[i].Write(port);
    }
}

int ListForm::Length() const
{
    return m_count;
}

Form ListForm::Element(int i) const
{
#line 541 "C:/psj/dev/locale/game/Forms.cpp"
    assert(i >= 0 && i < m_count);
#line 582 "src/game/Forms.cpp"
    return m_forms[i];
}

/* Reading the text. */

static bool IsDigit(char c)
{
    return c >= '0' && c <= '9';
}

static bool IsSpace(char c)
{
    switch (c) {
    case '\t':
    case '\n':
    case '\v':
    case '\f':
    case '\r':
    case ' ':
        return true;
    }
    return false;
}

/* past white space and comments (from ';' to the end of the line) */
static void SkipSpace(Port& port)
{
    while (!port.AtEof()) {
        char c = port.Peek();
        if (IsSpace(c)) {
            port.Get();
        } else if (c == ';') {
            while (!port.AtEof() && port.Peek() != '\n') {
                port.Get();
            }
        } else {
            return;
        }
    }
}

void Port::PutString(string text)
{
    for (const char* p = text.begin(); p < text.end(); p++) {
        Put(*p);
    }
}

void Port::Print(const Form& form)
{
    form.Print(*this);
}

void Port::Write(const Form& form)
{
    form.Write(*this);
}

/* a symbol, starting with `token` */
static Form ReadToken(Port& port, string token)
{
    while (!port.AtEof()) {
        char c = port.Peek();
        if (IsSpace(c)) {
            break;
        }
        switch (c) {
        case '"':
        case '#':
        case '(':
        case ')':
            goto end;
        case '\\':
            printf("Unexpected '\\' in token form.\n");
#line 617 "C:/psj/dev/locale/game/Forms.cpp"
            assert(0);
#line 659 "src/game/Forms.cpp"
        }
        token += port.Get();
    }
end:
    if (token.size() == 0) {
        printf("Unexpected eof while reading a token form.\n");
#line 627 "C:/psj/dev/locale/game/Forms.cpp"
        assert(0);
#line 668 "src/game/Forms.cpp"
    }
    return Form(token, true);
}

static Form ReadString(Port& port)
{
    if (port.AtEof()) {
        printf("Unexpected eof reading string form.\n");
#line 638 "C:/psj/dev/locale/game/Forms.cpp"
        assert(0);
#line 679 "src/game/Forms.cpp"
    }
    if (port.Get() != '"') {
        printf("String form is missing its leading '\"'.\n");
#line 643 "C:/psj/dev/locale/game/Forms.cpp"
        assert(0);
#line 685 "src/game/Forms.cpp"
    }
    string text = "";
    while (1) {
        if (port.AtEof()) {
            printf("String form is missing its trailing '\"'.\n");
#line 651 "C:/psj/dev/locale/game/Forms.cpp"
            assert(0);
#line 693 "src/game/Forms.cpp"
        }
        char c = port.Get();
        switch (c) {
        case '"':
            return Form(text, false);
        case '\\':
            if (port.AtEof()) {
                printf("String form is missing a character after '\\'.\n");
#line 664 "C:/psj/dev/locale/game/Forms.cpp"
                assert(0);
#line 704 "src/game/Forms.cpp"
            }
            text += port.Get();
            break;
        default:
            text += c;
            break;
        }
    }
}

/* a symbol's name, a string's text (a function of its own: String() called in place compiles differently) */
static inline string Text(const Form& form)
{
    return form.String();
}

/* #t or #f */
static Form ReadHash(Port& port)
{
    if (port.AtEof()) {
        printf("Unexpected eof reading # form.\n");
#line 681 "C:/psj/dev/locale/game/Forms.cpp"
        assert(0);
#line 728 "src/game/Forms.cpp"
    }
    if (port.Get() != '#') {
        printf("# form is missing its leading '#'.\n");
#line 686 "C:/psj/dev/locale/game/Forms.cpp"
        assert(0);
#line 734 "src/game/Forms.cpp"
    }
    string name = Text(ReadToken(port, ""));
    if (name == "t" || name == "T") {
        return Form(true);
    }
    if (name == "f" || name == "F") {
        return Form(false);
    }
    printf("Unrecognized # form: %s.\n", name.c_str());
#line 696 "C:/psj/dev/locale/game/Forms.cpp"
    assert(0);
#line 746 "src/game/Forms.cpp"
}

/* a list: (form form ...) */
static Form ReadAggregate(Port& port)
{
    if (port.AtEof()) {
        printf("Unexpected eof reading aggregate form.\n");
#line 707 "C:/psj/dev/locale/game/Forms.cpp"
        assert(0);
#line 756 "src/game/Forms.cpp"
    }
    if (port.Get() != '(') {
        printf("Aggregate form is missing its leading '('.\n");
#line 712 "C:/psj/dev/locale/game/Forms.cpp"
        assert(0);
#line 762 "src/game/Forms.cpp"
    }
    list<Form> read;
    while (1) {
        SkipSpace(port);
        if (port.AtEof()) {
            printf("Aggregate form is missing its trailing ')'.\n");
#line 722 "C:/psj/dev/locale/game/Forms.cpp"
            assert(0);
#line 771 "src/game/Forms.cpp"
        }
        if (port.Peek() == ')') {
            port.Get();
            int count = read.size();
            Form* forms = new Form[count];
            int k = 0;
            for (list<Form>::const_iterator i = read.begin(); i != read.end(); ++i, ++k) {
                forms[k] = *i;
            }
            return Form(count, forms, false);
        }
        read.push_back(port.Read());
    }
}

static Form ReadNumber(Port& port, int sign);

/* a number with its sign, or a symbol starting with + or - */
static Form ReadSigned(Port& port)
{
    char sign = port.GetByte();
    /*
     * (a bug: the end is checked the wrong way round, so a sign is read as a
     * symbol, and one at the end of the text stops the game)
     */
    if (port.AtEof() && IsDigit(port.Peek())) {
        switch (sign) {
        case '+':
            return ReadNumber(port, 1);
        case '-':
            return ReadNumber(port, -1);
        }
    }
    return ReadToken(port, string(&sign, &sign + 1));
}

/* an integer, or a fixed-point number with up to 6 decimals */
static Form ReadNumber(Port& port, int sign)
{
    int whole = 0;
    int decimals = 0;
    int fraction = 0;
    while (!port.AtEof()) {
        char c = port.PeekByte();
        if (c == '.') {
            port.Skip();
            while (!port.AtEof()) {
                char d = port.PeekByte();
                if (IsDigit(d)) {
                    decimals++;
                    port.Skip();
                    fraction = fraction * 10 + (d - '0');
                } else {
                    Fixed scaled = Fixed::Int(fraction);
                    Fixed part;
                    switch (decimals) {
                    case 0:
                    case 1:
                        part = scaled / 10;
                        break;
                    case 2:
                        part = scaled / 100;
                        break;
                    case 3:
                        part = scaled / 1000;
                        break;
                    case 4:
                        part = scaled / 10000;
                        break;
                    case 5:
                        part = scaled / 100000;
                        break;
                    case 6:
                        part = scaled / 1000000;
                        break;
                    default:
                        printf("Error reading fixed point number:\n  Too much precision.");
#line 801 "C:/psj/dev/locale/game/Forms.cpp"
                        assert(0);
#line 851 "src/game/Forms.cpp"
                    }
                    return Form((part + Fixed::Int(whole)) * sign);
                }
            }
        } else {
            if (!IsDigit(c)) {
                break;
            }
            port.Skip();
            whole = whole * 10 + (c - '0');
        }
    }
    return Form(sign * whole);
}

Form Port::Read()
{
    SkipSpace(*this);
    if (AtEof()) {
        return Form();
    }
    char c = Peek();
    if (IsDigit(c)) {
        return ReadNumber(*this, 1);
    }
    switch (c) {
    case '+':
    case '-':
        return ReadSigned(*this);
    case '(':
        return ReadAggregate(*this);
    case '"':
        return ReadString(*this);
    case '#':
        return ReadHash(*this);
    case ')':
#line 850 "C:/psj/dev/locale/game/Forms.cpp"
        assert(0);
#line 890 "src/game/Forms.cpp"
    default:
        return ReadToken(*this, "");
    }
}

Form ReadBinaryForm(Port& port)
{
    if (port.AtEof()) {
        return Form();
    }
    switch (port.GetByte()) {
    case Form::FORM_SYMBOL: {
        int length = port.GetInt();
        return Form(port.GetString(length), true);
    }
    case Form::FORM_STRING: {
        int length = port.GetInt();
        return Form(port.GetString(length), false);
    }
    case Form::FORM_LIST: {
        int count = port.GetInt();
        Form* forms = new Form[count];
        for (int i = 0; i < count; i++) {
            forms[i] = ReadBinaryForm(port);
        }
        return Form(count, forms, false);
    }
    case Form::FORM_BOOLEAN:
        return Form(port.GetByte() != 0);
    case Form::FORM_INTEGER:
        return Form(port.GetInt());
    case Form::FORM_FIXED:
        return Form(Fixed::FromRaw(port.GetInt()));
    default:
#line 891 "C:/psj/dev/locale/game/Forms.cpp"
        assert(0);
#line 927 "src/game/Forms.cpp"
    }
}

/* The ports. */

/* PCopen()'s mode */
static inline int OpenMode(int read, int write)
{
    if (read) {
        return write ? 2 : 0;
    }
#line 909 "C:/psj/dev/locale/game/Forms.cpp"
    assert(write);
#line 941 "src/game/Forms.cpp"
    return 1;
}

HostPort::~HostPort()
{
    if (m_open) {
        Close();
    }
}

void HostPort::Open(int read, int write)
{
#line 930 "C:/psj/dev/locale/game/Forms.cpp"
    assert(!m_open);
#line 956 "src/game/Forms.cpp"
    m_fd = PCopen(m_name.begin(), OpenMode(read, write), 0);
#line 932 "C:/psj/dev/locale/game/Forms.cpp"
    assert(m_fd >= 0);
#line 960 "src/game/Forms.cpp"
    m_readable = read;
    m_writable = write;
    m_open = 1;
    m_eof = 0;
    m_peeked = 0;
}

void HostPort::Create()
{
#line 942 "C:/psj/dev/locale/game/Forms.cpp"
    assert(!m_open);
#line 972 "src/game/Forms.cpp"
    m_fd = PCcreat(m_name.begin(), 0);
#line 944 "C:/psj/dev/locale/game/Forms.cpp"
    assert(m_fd >= 0);
#line 976 "src/game/Forms.cpp"
    PCclose(m_fd);
}

void HostPort::Close()
{
#line 950 "C:/psj/dev/locale/game/Forms.cpp"
    assert(m_open);
    int result = PCclose(m_fd);
    assert(result <= 0);
#line 986 "src/game/Forms.cpp"
    m_open = 0;
    m_readable = 0;
    m_writable = 0;
}

void HostPort::Put(char c)
{
#line 960 "C:/psj/dev/locale/game/Forms.cpp"
    assert(m_writable);
#line 996 "src/game/Forms.cpp"
    int written = PCwrite(m_fd, &c, 1);
#line 962 "C:/psj/dev/locale/game/Forms.cpp"
    assert(written == 1);
#line 1000 "src/game/Forms.cpp"
}

void HostPort::PutInt(int value)
{
#line 967 "C:/psj/dev/locale/game/Forms.cpp"
    assert(m_writable);
#line 1007 "src/game/Forms.cpp"
    int written = PCwrite(m_fd, (char*)&value, 4);
#line 969 "C:/psj/dev/locale/game/Forms.cpp"
    assert(written == 4);
#line 1011 "src/game/Forms.cpp"
}

void HostPort::PutBytes(char* bytes, int count)
{
#line 974 "C:/psj/dev/locale/game/Forms.cpp"
    assert(m_writable);
#line 1018 "src/game/Forms.cpp"
    int written = PCwrite(m_fd, bytes, count);
#line 976 "C:/psj/dev/locale/game/Forms.cpp"
    assert(written == count);
#line 1022 "src/game/Forms.cpp"
}

bool HostPort::AtEof()
{
#line 981 "C:/psj/dev/locale/game/Forms.cpp"
    assert(m_readable);
#line 1029 "src/game/Forms.cpp"
    if (m_peeked) {
        return false;
    }
    if (m_eof) {
        return true;
    }
    switch (PCread(m_fd, &m_peek, 1)) {
    case 1:
        m_peeked = 1;
        return false;
    case 0:
        m_eof = 1;
        return true;
    default:
#line 994 "C:/psj/dev/locale/game/Forms.cpp"
        assert(0);
#line 1046 "src/game/Forms.cpp"
    }
}

char HostPort::Peek()
{
#line 1001 "C:/psj/dev/locale/game/Forms.cpp"
    assert(!AtEof());
#line 1054 "src/game/Forms.cpp"
    return m_peek;
}

char HostPort::PeekByte()
{
    if (m_peeked) {
        return m_peek;
    }
    return Peek();
}

char HostPort::Get()
{
#line 1014 "C:/psj/dev/locale/game/Forms.cpp"
    assert(!AtEof());
#line 1070 "src/game/Forms.cpp"
    char c = m_peek;
    m_peeked = 0;
    return c;
}

char HostPort::GetByte()
{
    if (m_peeked) {
        char c = m_peek;
        m_peeked = 0;
        return c;
    }
    return Get();
}

void HostPort::Skip()
{
    Get();
}

int HostPort::GetInt()
{
    int value;
    char* bytes = (char*)&value;
    for (u_int i = 0; i < 4; i++) {
        bytes[i] = GetByte();
    }
    return value;
}

string HostPort::GetString(int length)
{
    string text = "";
    for (int i = 0; i < length; i++) {
        text += Get();
    }
    return text;
}

int HostPort::SeekCurrent(int offset)
{
    return PClseek(m_fd, offset, 1);
}

int HostPort::SeekSet(int position)
{
    return PClseek(m_fd, position, 0);
}

int HostPort::SeekEnd(int offset)
{
    return PClseek(m_fd, offset, 2);
}

/*
 * Pads the text with spaces up to the position, which may be past its end.
 * (The position asked for is not used: it pads to, and gives back, the
 * current one.)
 */
int StringPort::Seek(int position)
{
#line 1264 "C:/psj/dev/locale/game/Forms.cpp"
    assert(m_position >= 0);
#line 1134 "src/game/Forms.cpp"
    int pad = m_position - m_text.size();
    if (pad > 0) {
        m_text += string(pad, ' ');
    }
    return m_position;
}

StringPort::~StringPort()
{
}

/* (a character written over the text is also added at its end) */
void StringPort::Put(char c)
{
    if (m_position < m_text.size()) {
        m_text[m_position] = c;
    }
    if (m_position == m_text.size()) {
        m_text += c;
    } else {
        Seek(m_position);
        m_text += c;
    }
    m_position++;
}

void StringPort::PutInt(int value)
{
    char* bytes = (char*)&value;
    for (u_int i = 0; i < 4; i++) {
        Put(bytes[i]);
    }
}

bool StringPort::AtEof()
{
    return m_position == m_text.size();
}

char StringPort::Peek()
{
#line 1311 "C:/psj/dev/locale/game/Forms.cpp"
    assert(!AtEof());
#line 1178 "src/game/Forms.cpp"
    return m_text[m_position];
}

char StringPort::PeekByte()
{
    return m_text[m_position];
}

char StringPort::Get()
{
#line 1322 "C:/psj/dev/locale/game/Forms.cpp"
    assert(!AtEof());
#line 1191 "src/game/Forms.cpp"
    char c = m_text[m_position];
    m_position++;
    return c;
}

char StringPort::GetByte()
{
    char c = m_text[m_position];
    m_position++;
    return c;
}

void StringPort::Skip()
{
    m_position++;
}

/* (without moving on) */
int StringPort::GetInt()
{
    return *(int*)(m_text.begin() + m_position);
}

string StringPort::GetString(int length)
{
#line 1347 "C:/psj/dev/locale/game/Forms.cpp"
    assert(m_position + length < m_text.size());
#line 1219 "src/game/Forms.cpp"
    string text(m_text, m_position, length);
    m_position += length;
    return text;
}

int StringPort::SeekCurrent(int offset)
{
    return Seek(offset + m_position);
}

int StringPort::SeekSet(int position)
{
    return Seek(position);
}

int StringPort::SeekEnd(int offset)
{
    return Seek(offset + m_text.size());
}

inline MemPtrPort::MemPtrPort(const char* name) : m_file(name), m_position(0)
{
}

MemPtrPort::~MemPtrPort()
{
}

void MemPtrPort::Put(char c)
{
    printf("MemPtrPort: MemPtrs are not writable.\n");
#line 1384 "C:/psj/dev/locale/game/Forms.cpp"
    assert(0);
#line 1253 "src/game/Forms.cpp"
}

void MemPtrPort::PutInt(int value)
{
    printf("MemPtrPort: MemPtrs are not writable.\n");
#line 1390 "C:/psj/dev/locale/game/Forms.cpp"
    assert(0);
#line 1261 "src/game/Forms.cpp"
}

bool MemPtrPort::AtEof()
{
    return m_position >= m_file.Size();
}

char MemPtrPort::Peek()
{
#line 1400 "C:/psj/dev/locale/game/Forms.cpp"
    assert(!AtEof());
#line 1273 "src/game/Forms.cpp"
    return m_file[m_position];
}

char MemPtrPort::PeekByte()
{
    return m_file[m_position];
}

char MemPtrPort::Get()
{
#line 1411 "C:/psj/dev/locale/game/Forms.cpp"
    assert(!AtEof());
#line 1286 "src/game/Forms.cpp"
    char c = m_file[m_position];
    m_position++;
    return c;
}

char MemPtrPort::GetByte()
{
    char c = m_file[m_position];
    m_position++;
    return c;
}

void MemPtrPort::Skip()
{
    m_position++;
}

int MemPtrPort::GetInt()
{
    int value;
    char* to = (char*)&value;
    char* from = m_file.Data() + m_position;
    for (u_int i = 0; i < 4; i++) {
        to[i] = from[i];
    }
    m_position += 4;
    return value;
}

string MemPtrPort::GetString(int length)
{
    char* start = m_file.Data() + m_position;
    string text(start, start + length);
    m_position += length;
    return text;
}

int MemPtrPort::SeekCurrent(int offset)
{
    m_position += offset;
#line 1453 "C:/psj/dev/locale/game/Forms.cpp"
    assert(m_position >= 0);
#line 1329 "src/game/Forms.cpp"
    return m_position;
}

int MemPtrPort::SeekSet(int position)
{
    m_position = position;
#line 1459 "C:/psj/dev/locale/game/Forms.cpp"
    assert(m_position >= 0);
#line 1338 "src/game/Forms.cpp"
    return m_position;
}

int MemPtrPort::SeekEnd(int offset)
{
    return m_position = m_file.Size() + offset;
}

Form g_define("define", true);
