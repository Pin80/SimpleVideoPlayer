#include "settings.h"
#include <utility>
#include "nostd/string_view.hpp"

Tsettings::Tsettings()
{
    m_sf = fopen(SETTINGS_FILE, "r");
    m_isError = (m_sf == nullptr);
    if (m_isError)
        return;
    fseek(m_sf, 0, SEEK_SET);
    fseek(m_sf, 0, SEEK_END);
    m_lng = ftell(m_sf);
    fseek(m_sf, 0, SEEK_SET);
    if (ferror(m_sf) != 0)
    {
        fclose(m_sf);
        m_isError = true;
        return;
    }
    TSmartPtr<char> rawJSON(m_lng);
    m_rawJSON = std::move(rawJSON);
    if (m_rawJSON)
    {
        fread(const_cast<char*>(m_rawJSON.get()), m_lng, 1, m_sf);
        fclose(m_sf);
        m_isError = false;
    }
    else
    {
        m_isError = true;
        fclose(m_sf);
        return;
    }
    char * buffer = const_cast<char *>(m_rawJSON.get());
    //if (m_document.ParseInsitu(buffer).HasParseError())
    //{
    //    m_isError = true;
    //    return;
    //}
    m_isError = !simdjson::validate_utf8(buffer, m_lng);
    if (m_isError)
        return;
    simdjson::padded_string str(buffer, m_lng);
    m_simdstr = std::move(str);
    simdjson::error_code error = m_parser.iterate(m_simdstr).get(m_doc);
    m_isError = (error != 0);
    if (m_isError)
        return;

    simdjson::ondemand::json_type jt;
    m_isError = (m_doc.type().get(jt) != 0);
    if (m_isError)
        return;
    if (jt != simdjson::ondemand::json_type::object)
        return;

    m_isError = m_doc.get_object().get(m_root_obj);
    if (m_isError)
        return;
    m_isError = m_root_obj["application"].get(m_app_obj);
}

bool Tsettings::is_error() const
{
    return m_isError;
}

bool Tsettings::getValue(const char *_key, TSmartPtr<char> &_val)
{
#undef SIMDJSON_HAS_STRING_VIEW
    using string_view = nonstd::string_view;
    if (m_isError)
        return false;
    string_view str;
    m_isError = (m_app_obj[_key].get(str) != 0);
    if (m_isError)
        return false;
    TSmartPtr<char> sptr = TSmartPtr<char>(str.length());
    char * dest_buff = const_cast<char *>(sptr.get());
    char * src_ptr = const_cast<char *>(str.data());
    memcpy(dest_buff, src_ptr, str.length());
    _val = std::move(sptr);
    return true;
}

bool Tsettings::containValue(const char *_key, const char *_val)
{
#undef SIMDJSON_HAS_STRING_VIEW
    using string_view = nonstd::string_view;
    if (m_isError)
        return false;
    simdjson::ondemand::value val;
    m_isError = (m_app_obj[_key].get(val) != 0);
    if (m_isError)
        return false;
    simdjson::ondemand::json_type jt;
    m_isError = (val.type().get(jt) != 0);
    if (m_isError)
        return false;
    if (jt != simdjson::ondemand::json_type::array)
    {
        m_isError = true;
        return false;
    }
    bool found = false;
    for( auto fmt: val.get_array())
    {
        m_isError = (fmt.type().get(jt) != 0);
        if (m_isError)
            return false;
        if (jt != simdjson::ondemand::json_type::string)
            break;
        string_view vstr;
        m_isError = (fmt.get(vstr) != 0);
        if (m_isError)
            return false;
        const char* cstr = vstr.data();
        TSmartPtr<char> sptr = TSmartPtr<char>(vstr.length() + 1);
        char * dest_buff = const_cast<char *>(sptr.get());
        memcpy(dest_buff, vstr.data(), vstr.length());
        dest_buff[vstr.length()] = 0;
        const char * s = strstr(_val, sptr.get());
        if (s != nullptr)
        {
            found = true;
            break;
        }
    }
    if (!found)
        return false;
    return true;
}

bool Tsettings::getValue(const char *_key, unsigned long& _val)
{
#undef SIMDJSON_HAS_STRING_VIEW
    if (m_isError)
        return false;
    m_isError = (m_app_obj[_key].get(_val) != 0);
    if (m_isError)
        return false;
    return true;
}
