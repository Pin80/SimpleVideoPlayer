#ifndef TCOMMON_H
#define TCOMMON_H

#include <cstdio>
#include <pthread.h>
#include <cassert>
#include <stdlib.h>
#include <type_traits>
#include "glad/glad.h"

#ifndef CRITICAL_ERROR_CODE
    #define CRITICAL_ERROR_CODE -1
#endif

#ifndef LOG_FILE_NAME
    #define LOG_FILE_NAME "def_videoplayer.log"
#endif

#ifndef ERRLOG_FILE_NAME
    #define ERRLOG_FILE_NAME "def_videoplayer_error.log"
#endif

class TNonCopyable
{
    TNonCopyable(const TNonCopyable&) = delete;
    TNonCopyable(TNonCopyable&&) = delete;
    constexpr TNonCopyable& operator=(const TNonCopyable&) = delete;
    TNonCopyable&& operator=(TNonCopyable&&) = delete;
public:
    TNonCopyable() noexcept
    {}
};

// printf is unsafe
void showmsg(const char * _str) noexcept;
void showerrmsg(const char * _str) noexcept;

class TMessageEnd {};
class TMessageNL {};

class TMessage : TNonCopyable
{
    mutable FILE* m_stm = nullptr;
    mutable bool m_isError = false;
protected:
    void setStream(FILE* m_stm) const;
    FILE* getStream() const;
    virtual void process_error() const noexcept;
public:
    TMessage(FILE * _stm = stdout) noexcept;
    const TMessage& operator << (const char *_str) const noexcept;
    const TMessage& operator << (long int _n) const noexcept;
    const TMessage& operator << (long unsigned int _n) const noexcept;
    const TMessage& operator << (int _n) const noexcept;
    const TMessage& operator << (unsigned int _n) const noexcept;
    const TMessage& operator << (float _n) const noexcept;
    const TMessage& operator << (double _n) const noexcept;
    void operator <<(const TMessageEnd&) const noexcept;
    const TMessage& operator <<(const TMessageNL&) const noexcept;
    bool isError() const;
    static char* reverse(char* _str, int _len);
    static char* litoa(long int _value, char* _buffer, unsigned _base);
    static char* ulitoa(unsigned long int _value, char* _buffer, unsigned _base);
    virtual ~TMessage() = default;
};

class TFormatMessage: public TMessage
{
    mutable char* m_string = nullptr;
    virtual void process_error() const  noexcept override;
public:
    TFormatMessage() noexcept;
    const char* getString() const noexcept;
    ~TFormatMessage();
};

class TErrorMessage: public TMessage
{
    mutable bool m_isFirst = true;
public:
    TErrorMessage();
    template <typename T>
    const TMessage& operator << (const T& _obj) const noexcept
    {
        const char* error_prefix = "Error: ";
        if (m_isFirst)
        {
            m_isFirst = false;
            TMessage::operator << (error_prefix);
        }
        TMessage::operator << (_obj);
        return *this;
    }
    void operator << (const TMessageEnd&) const noexcept;
};

template <typename TMSG = TMessage>
class TLogMessage : public TMSG
{
    mutable TMSG m_msg;
    mutable pthread_mutex_t m_mutex;
    mutable bool m_isError = false;
protected:
    virtual void unlock() const = 0;
public:
    TLogMessage(const char *_fname)
    {
        if (pthread_mutex_init(&m_mutex, nullptr) != 0)
        {
            m_isError = true;
            return;
        }
        FILE* stm = fopen(_fname, "w+");
        m_isError = (stm == nullptr);
        if (m_isError)
        {
            pthread_mutex_destroy(&m_mutex);
            return;
        }
        TMSG::setStream(stm);
    }
    ~TLogMessage()
    {

        pthread_mutex_destroy(&m_mutex);
        fclose(TMSG::getStream());
    }
    template <typename T>
    const TLogMessage& operator << (const T& _obj) const noexcept
    {
        if (m_isError)
            return *this;
        pthread_mutex_lock(&m_mutex);
        if ((TMSG::getStream()) && (!TMSG::isError()))
            TMSG::operator <<(_obj);
        m_msg << _obj;
        pthread_mutex_unlock(&m_mutex);
        return *this;
    }
    void operator << (const TMessageEnd&) const noexcept
    {
        if (m_isError)
            return;
        pthread_mutex_lock(&m_mutex);
        if (TMSG::getStream())
            TMSG::operator <<(TMessageEnd());
        m_msg << TMessageEnd();
        pthread_mutex_unlock(&m_mutex);
        unlock();
    }
};

template <typename TMSG = TMessage>
class TSyncLogMessage : public TLogMessage<TMSG>
{
    mutable pthread_mutex_t m_line_mutex;
    mutable bool m_line_sync = false;
protected:
    virtual void unlock() const
    {
        if (!m_line_sync)
            return;
        pthread_mutex_unlock(&m_line_mutex);
    }
public:
    TSyncLogMessage(const char *_fname) : TLogMessage<TMSG>(_fname)
    {
        if (pthread_mutex_init(&m_line_mutex, nullptr) == 0)
            m_line_sync = true;
    }
    ~TSyncLogMessage()
    {
        pthread_mutex_destroy(&m_line_mutex);
    }
    template <typename T>
    const TLogMessage<TMSG>& operator << (const T& _obj) const noexcept
    {
        if (!m_line_sync)
            return *this;
        pthread_mutex_lock(&m_line_mutex);
        TLogMessage<TMSG>::operator <<(_obj);
        return *this;
    }
};

// the bycycle, which repeats unique_ptr
template <typename T>
class TSmartPtr
{
    T* m_obj = nullptr;
    TSmartPtr(const TSmartPtr&) = delete;
    TSmartPtr& operator=(const TSmartPtr&) = delete;
public:
    explicit TSmartPtr() noexcept
    {   }
    TSmartPtr(T* _obj) noexcept
    {
        if (_obj == nullptr)
            return;
        m_obj = _obj;
    }
    TSmartPtr(TSmartPtr&& _obj) noexcept
    {
        if (_obj.m_obj == m_obj)
            return;
        m_obj = _obj.m_obj;
        _obj.m_obj = nullptr;
    }
    TSmartPtr& operator=(TSmartPtr&& _obj) noexcept
    {
        if (_obj.m_obj == m_obj)
            return *this;
        if (!m_obj)
            delete m_obj;
        m_obj = _obj.m_obj;
        _obj.m_obj = nullptr;
        return *this;
    }
    T* operator ->() const noexcept
    {
        return m_obj;
    }
    T& operator *() const noexcept
    {
        return *m_obj;
    }
    const T* get() const noexcept
    {
        return m_obj;
    }
    void reset(T* _obj)
    {
        if (m_obj == _obj)
            return;
        if (!m_obj) // optional "if"
            delete m_obj;
        m_obj = _obj;
    }
    explicit operator bool() const noexcept
    {
        return m_obj != nullptr;
    }
    virtual ~TSmartPtr()
    {
        if (!m_obj) // [optional "if" as best practice example]
            delete m_obj;
    }
};

// the bycycle, which repeats unique_ptr
template <>
class TSmartPtr<uint8_t>
{
    uint8_t* m_obj = nullptr;
    TSmartPtr(const TSmartPtr&) = delete;
    TSmartPtr& operator=(const TSmartPtr&) = delete;
public:
    explicit TSmartPtr() noexcept
    {     }
    TSmartPtr(std::size_t _size) noexcept
    {
        m_obj = (uint8_t*)malloc(_size);
    }
    TSmartPtr(TSmartPtr&& _obj) noexcept
    {
        if (_obj.m_obj == m_obj)
            return;
        m_obj = _obj.m_obj;
        _obj.m_obj = nullptr;
    }
    TSmartPtr& operator=(TSmartPtr&& _obj) noexcept
    {
        if (_obj.m_obj == m_obj)
            return *this;
        if (!m_obj)
            free(m_obj);
        m_obj = _obj.m_obj;
        _obj.m_obj = nullptr;
        return *this;
    }
    uint8_t* operator ->() const noexcept
    {
        return m_obj;
    }
    uint8_t* operator[] (std::size_t _size)
    {
        return m_obj + _size;
    }
    uint8_t& operator *() const noexcept
    {
        return *m_obj;
    }
    const uint8_t* get() const noexcept
    {
        return m_obj;
    }
    void reset(uint8_t* _obj)
    {
        if (m_obj == _obj)
            return;
        if (!m_obj) // optional "if"
            delete m_obj;
        m_obj = _obj;
    }
    explicit operator bool() const noexcept
    {
        return m_obj != nullptr;
    }
    void* operator new(std::size_t _size) noexcept
    {
        return malloc(_size);
    }
    virtual ~TSmartPtr()
    {
        if (!m_obj) // [optional "if" as best practice example]
            free(m_obj);
    }
};

template <>
class TSmartPtr<char>
{
    char* m_obj = nullptr;
    TSmartPtr(const TSmartPtr&) = delete;
    TSmartPtr& operator=(const TSmartPtr&) = delete;
public:
    explicit TSmartPtr() noexcept
    {     }
    TSmartPtr(std::size_t _size) noexcept
    {
        m_obj = (char*)malloc(_size);
    }
    TSmartPtr(TSmartPtr&& _obj) noexcept
    {
        if (_obj.m_obj == m_obj)
            return;
        m_obj = _obj.m_obj;
        _obj.m_obj = nullptr;
    }
    TSmartPtr& operator=(TSmartPtr&& _obj) noexcept
    {
        if (_obj.m_obj == m_obj)
            return *this;
        if (!m_obj)
            free(m_obj);
        m_obj = _obj.m_obj;
        _obj.m_obj = nullptr;
        return *this;
    }
    char* operator ->() const noexcept
    {
        return m_obj;
    }
    char* operator[] (std::size_t _size)
    {
        return m_obj + _size;
    }
    char& operator *() const noexcept
    {
        return *m_obj;
    }
    const char* get() const noexcept
    {
        return m_obj;
    }
    void reset(char* _obj)
    {
        if (m_obj == _obj)
            return;
        if (!m_obj) // optional "if"
            delete m_obj;
        m_obj = _obj;
    }
    explicit operator bool() const noexcept
    {
        return m_obj != nullptr;
    }
    void* operator new(std::size_t _size) noexcept
    {
        return malloc(_size);
    }
    virtual ~TSmartPtr()
    {
        if (!m_obj) // [optional "if" as best practice example]
            free(m_obj);
    }
};

struct TVariant
{
    enum Type {eInt,eStr};
    enum Type type;
    union {
        int int_;
        char* str_;
    };
};

enum TEvent  {eNoEvent = -1,
              eExit,
              ePause,
              eResume,
              eFullScreen,
              eUnFullScreen,
              eRestart,
              eAddVolume,
              eScreenShot,
              eCroppedShot,
              eMute,
              eDecVolume,
              eBackStep,
              eNextStep};

#define unconst(u) const_cast<uint8_t *>(u)

extern const TMessage& msgr;
extern const TErrorMessage& emsgr;
extern const TSyncLogMessage<TMessage>& lmsgr;
extern const TSyncLogMessage<TErrorMessage>& elmsgr;
extern const TFormatMessage& fmsgr;
extern const TMessageEnd& mend;
extern const TMessageNL& nl;

#endif // TCOMMON_H
