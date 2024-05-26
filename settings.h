#ifndef SETTINGS_H
#define SETTINGS_H
#include "common.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h" // for stringify JSON
#include "json_parser/simdjson.h"

class Tsettings : TNonCopyable
{
    FILE* m_sf = nullptr;
    bool m_isError = false;
    TSmartPtr<char> m_rawJSON;
    unsigned long m_lng;
    rapidjson::Document m_document;
    simdjson::ondemand::parser m_parser;
    simdjson::ondemand::document m_doc;
    simdjson::padded_string m_simdstr;
    simdjson::ondemand::object m_root_obj;
    simdjson::ondemand::object m_app_obj;
public:
    Tsettings();
    bool is_error() const;
    bool getValue(const char * _key, unsigned long &_val);
    bool getValue(const char * _key, TSmartPtr<char> &_val);
    bool containValue(const char * _key, const char* _val);
};

#endif // SETTINGS_H
