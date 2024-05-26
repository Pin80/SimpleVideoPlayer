#include "common.h"
#include "stdio.h"
#include "stdlib.h"
#include <math.h>
#include <string.h>

void showmsg(const char *_str) noexcept
{
    lmsgr << _str << mend;
}

void showerrmsg(const char * _str) noexcept
{
    elmsgr << _str << mend;
}

const TMessage& msgr = TMessage();
const TMessageEnd& mend = TMessageEnd();
const TMessageNL& nl = TMessageNL();

// Reverses a string 'str' of length 'len'
char* TMessage::reverse(char* _str, int _len)
{
    int i = 0, j = _len - 1, temp;
    while (i < j)
    {
        temp = _str[i];
        _str[i] = _str[j];
        _str[j] = temp;
        i++;
        j--;
    }
    return _str;
}

// Custom itoa. No c++17 support (std::to_chars)
char* TMessage::litoa(long int _value, char* _buffer, unsigned _base)
{
    if (_base < 2 || _base > 16 || !_buffer)
    {
        return _buffer;
    }

    long int n = abs(_value);
    int i = 0;
    while (n)
    {
        int r = n % _base;

        if (r >= 10)
        {
            _buffer[i++] = 65 + (r - 10); // 65 = 'A'
        }
        else {
            _buffer[i++] = 48 + r; // 48 = '0'
        }

        n = n / _base;
    }

    if (i == 0)
    {
        _buffer[i++] = '0';
    }

    if (_value < 0 && _base == 10)
    {
        _buffer[i++] = '-';
    }

    _buffer[i] = '\0';

    return reverse(_buffer, i);
}

// Custom itoa. No c++17 support (std::to_chars)
char* TMessage::ulitoa(unsigned long int _value, char* _buffer, unsigned _base)
{
    if (_base < 2 || _base > 16 || !_buffer)
    {
        return _buffer;
    }

    unsigned long int n = _value;
    int i = 0;
    while (n)
    {
        int r = n % _base;

        if (r >= 10)
        {
            _buffer[i++] = 65 + (r - 10); // 65 = 'A'
        }
        else {
            _buffer[i++] = 48 + r; // 48 = '0'
        }

        n = n / _base;
    }

    if (i == 0)
    {
        _buffer[i++] = '0';
    }

    if (_value < 0 && _base == 10)
    {
        _buffer[i++] = '-';
    }

    _buffer[i] = '\0';

    return reverse(_buffer, i);
}


void TMessage::setStream(FILE * _stm) const
{
    m_stm = _stm;
}

FILE *TMessage::getStream() const
{
    return m_stm;
}

void TMessage::process_error() const noexcept
{
    exit(CRITICAL_ERROR_CODE);
}

TMessage::TMessage(FILE *_stm) noexcept : m_stm(_stm)
{ }

const TMessage& TMessage::operator << (const char *_str) const noexcept
{
    if (_str)
    {
        if (!m_isError)
        {
            fputs(_str, m_stm);
            m_isError = (ferror(m_stm) != 0);
        }
        else
            process_error();
    }
    return *this;
}

bool TMessage::isError() const
{
    return m_isError;
}

const TMessage& TMessage::operator << (long int _n) const noexcept
{
    char number[20];
    litoa(_n, number, 10); // itoa is not found in stdlib.h
    if (!m_isError)
    {
        fputs(number, m_stm);
        m_isError = (ferror(m_stm) != 0);
    }
    else
        process_error();
    return *this;
}

const TMessage& TMessage::operator << (long unsigned int _n) const noexcept
{
    char number[30];
    ulitoa(_n, number, 10);
    if (!m_isError)
    {
        fputs(number, m_stm);
        m_isError = (ferror(m_stm) != 0);
    }
    else
        process_error();
    return *this;
}

const TMessage& TMessage::operator << (int _n) const noexcept
{
    char number[20];
    litoa(_n, number, 10);
    if (!m_isError)
    {
        fputs(number, m_stm);
        m_isError = (ferror(m_stm) != 0);
    }
    else
        process_error();
    return *this;
}

const TMessage& TMessage::operator << (unsigned int _n) const noexcept
{
    char number[20];
    litoa(_n, number, 10);
    if (!m_isError)
    {
        fputs(number, m_stm);
        m_isError = (ferror(m_stm) != 0);
    }
    else
        process_error();
    return *this;
}

const TMessage& TMessage::operator << (float _n) const noexcept
{
    char number[20];
    gcvt(_n, 12, number);
    if (!m_isError)
    {
        fputs(number, m_stm);
        m_isError = (ferror(m_stm) != 0);
    }
    else
        process_error();
    return *this;
}

const TMessage& TMessage::operator << (double _n) const noexcept
{
    char number[20];
    gcvt(_n, 12, number);
    if (!m_isError)
    {
        fputs(number, m_stm);
        m_isError = (ferror(m_stm) != 0);
    }
    else
        process_error();
    return *this;
}


void TMessage::operator << (const TMessageEnd&) const noexcept
{
    if (!m_isError)
    {
        fputs("\n", m_stm);
        m_isError = (ferror(m_stm) != 0);
    }
    else
        process_error();
}

const TMessage& TMessage::operator << (const TMessageNL&) const noexcept
{
    if (!m_isError)
    {
        fputs("\n", m_stm);
        m_isError = (ferror(m_stm) != 0);
    }
    else
        process_error();
    return *this;
}

const TErrorMessage& emsgr = TErrorMessage();

TErrorMessage::TErrorMessage()
{
    setStream(stderr);
}

void TErrorMessage::operator << (const TMessageEnd&) const noexcept
{
    m_isFirst = true;
    TMessage::operator <<(TMessageEnd());
    return;
}

const TFormatMessage& fmsgr = TFormatMessage();

void TFormatMessage::process_error() const noexcept
{
    return;
}

TFormatMessage::TFormatMessage() noexcept
{
    FILE* stm = fopen(TEMPORARY_FILE, "w+");
    setStream(stm);
}

const char *TFormatMessage::getString() const noexcept
{
    FILE* stm = getStream();
    if (!stm)
        return nullptr;
    unsigned long lng = 0;
    if(m_string)
        free(m_string);
    if (isError())
        return nullptr;
    fwrite("\0", 1, 1, stm);
    if (ferror(stm) != 0)
    {
        fclose(getStream());
        setStream(nullptr);
        return nullptr;
    }
    fflush(stm);
    fseek(stm, 0, SEEK_SET);
    fseek(stm, 0, SEEK_END);
    lng = ftell(stm);
    fseek(stm, 0, SEEK_SET);
    if (ferror(stm) != 0)
    {
        fclose(getStream());
        setStream(nullptr);
        return nullptr;
    }
    m_string = (char *)malloc(lng);
    memset(m_string, 0, lng);
    fread(m_string, lng, 1, stm);
    fseek(stm, 0, SEEK_SET);
    if (ferror(stm) != 0)
    {
        fclose(getStream());
        setStream(nullptr);
        return nullptr;
    }
    return m_string;
}

TFormatMessage::~TFormatMessage()
{
    if (getStream())
        fclose(getStream());
    if (m_string)
        free(m_string);
}

const TSyncLogMessage<>& lmsgr{LOG_FILE_NAME};
const TSyncLogMessage<TErrorMessage>& elmsgr{ERRLOG_FILE_NAME};

