// Copyright 2013-present Facebook. All Rights Reserved.

#include <plist/XMLParser.h>

using plist::XMLParser;
using plist::Object;

XMLParser::XMLParser() :
    BaseXMLParser(),
    _root        (nullptr)
{
}

Object *XMLParser::
parse(std::string const &path, error_function const &error)
{
    if (_root != nullptr)
        return nullptr;

    if (!BaseXMLParser::parse(path, error))
        return nullptr;

    return _root;
}

Object *XMLParser::
parse(std::FILE *fp, error_function const &error)
{
    if (_root != nullptr)
        return nullptr;

    if (!BaseXMLParser::parse(fp, error))
        return nullptr;

    return _root;
}

void XMLParser::
onBeginParse()
{
    _root             = nullptr;
    _state.current    = nullptr;
    _state.key.valid  = false;
    _state.key.active = false;
}

void XMLParser::
onEndParse(bool success)
{
    if (!success) {
        if (_state.current != nullptr && _state.current != _root) {
            _state.current->release();
        }
        for (auto s : _stack) {
            if (s.current != _state.current && s.current != _root) {
                s.current->release();
            }
        }
        _stack.clear();
        if (_root != nullptr) {
            _root->release();
            _root = nullptr;
        }
    }
    _state.current    = nullptr;
    _state.key.valid  = false;
    _state.key.active = false;
    _cdata.clear();
}

void XMLParser::
onStartElement(std::string const &name, string_map const &attrs, size_t depth)
{
    Dictionary *dict = new Dictionary;

    if (depth == 0) {
        if (name != "plist") {
            error("expecting 'plist', found '%s'", name.c_str());
        }
        return;
    }
    
    //
    // If we have a root, and depth == 1 there's an extra
    // entry after the first element, bail out.
    //
    if (depth == 1 && _root != nullptr) {
        error("unexpected element '%s' after root element", name.c_str());
        return;
    }

    if (!beginObject(name)) {
        stop();
        return;
    }
}

void XMLParser::
onEndElement(std::string const &name, size_t)
{
    if (!endObject(name)) {
        stop();
    }
}

void XMLParser::
onCharacterData(std::string const &cdata, size_t)
{
    if (!isExpectingCDATA()) {
        for (size_t n = 0; n < cdata.size(); n++) {
            if (!isspace(cdata[n])) {
                error("unexpected cdata");
                stop();
            }
        }
        return;
    }

    _cdata += cdata;
}

inline bool XMLParser::
inContainer() const
{
    return (depth() == 1 || inDictionary() || inArray());
}

inline bool XMLParser::
inArray() const
{
    return (CastTo <Array> (_state.current) != nullptr);
}

inline bool XMLParser::
inDictionary() const
{
    return (CastTo <Dictionary> (_state.current) != nullptr);
}

inline bool XMLParser::
isExpectingKey() const
{
    return (inDictionary() && !_state.key.valid);
}

inline bool XMLParser::
isExpectingCDATA() const
{
    return (CastTo <Integer> (_state.current) != nullptr ||
            CastTo <Real> (_state.current) != nullptr ||
            CastTo <String> (_state.current) != nullptr ||
            (inDictionary() && _state.key.active));
}

bool XMLParser::
beginObject(std::string const &name)
{
    if (inDictionary()) {
        if (name == "key") {
            if (!isExpectingKey()) {
                error("unexpected 'key' when expecting value "
                        "in dictionary definition");
                return false;
            }
            
            return beginKey();
        } else if (isExpectingKey()) {
            error("unexpected element '%s' when a key "
                    "was expected in dictionary definition",
                    name.c_str());
            return false;
        }
    }

    if (!inContainer()) {
        error("unexpected '%s' element in a non-container element.",
                name.c_str());
        return false;
    }


    if (name == "array") {
        return beginArray();
    } else if (name == "dict") {
        return beginDictionary();
    } else if (name == "string") {
        return beginString();
    } else if (name == "integer") {
        return beginInteger();
    } else if (name == "real") {
        return beginReal();
    } else if (name == "true") {
        return beginBoolean(true);
    } else if (name == "false") {
        return beginBoolean(false);
    } else if (name == "null") {
        return beginNull();
    } else if (name == "data") {
        return beginData();
    } else if (name == "date") {
        return beginDate();
    }

    error("unexpected element '%s'", name.c_str());
    return false;
}

bool XMLParser::
endObject(std::string const &name)
{
    if (name == "plist") {
        return true;
    } if (name == "key") {
        return endKey();
    } else if (name == "array") {
        return endArray();
    } else if (name == "dict") {
        return endDictionary();
    } else if (name == "string") {
        return endString();
    } else if (name == "integer") {
        return endInteger();
    } else if (name == "real") {
        return endReal();
    } else if (name == "true" || name == "false") {
        return endBoolean();
    } else if (name == "null") {
        return endNull();
    } else if (name == "data") {
        return endData();
    } else if (name == "date") {
        return endDate();
    }

    error("unexpected element '%s'", name.c_str());
    return false;
}

void XMLParser::
push(Object *object)
{
    if (_state.current != nullptr) {
        _stack.push_back(_state);
    }
    _state.current    = object;
    _state.key.valid  = false;
    _state.key.active = false;
    if (_root == nullptr) {
        _root = _state.current;
    }
}

void XMLParser::
pop()
{
    if (_stack.empty() && _state.current == nullptr) {
        error("stack underflow");
        return;
    }

    if (_state.current != _root) {
        State old = _state;
        _state = _stack.back();
        _stack.pop_back();

        if (auto array = CastTo <Array> (_state.current)) {
            array->append(old.current);
        } else if (auto dict = CastTo <Dictionary> (_state.current)) {
            if (!isExpectingKey()) {
                dict->set(_state.key.value, old.current);
                _state.key.valid  = false;
                _state.key.active = false;
            }
        }
    }

    _cdata.clear();
}

bool XMLParser::
beginArray()
{
    push(Array::New());
    return true;
}

bool XMLParser::
endArray()
{
    pop();
    return true;
}

bool XMLParser::
beginDictionary()
{
    push(Dictionary::New());
    _state.key.valid  = false;
    _state.key.active = false;
    return true;
}

bool XMLParser::
endDictionary()
{
    pop();
    return true;
}

bool XMLParser::
beginString()
{
    push(String::New());
    _cdata.clear();
    return true;
}

bool XMLParser::
endString()
{
    CastTo <String> (_state.current)->setValue(_cdata);
    pop();
    return true;
}

bool XMLParser::
beginInteger()
{
    push(Integer::New());
    _cdata.clear();
    return true;
}

bool XMLParser::
endInteger()
{
    CastTo <Integer> (_state.current)->setValue(std::stoi(_cdata));
    pop();
    return true;
}

bool XMLParser::
beginReal()
{
    push(Real::New());
    _cdata.clear();
    return true;
}

bool XMLParser::
endReal()
{
    CastTo <Integer> (_state.current)->setValue(std::stod(_cdata));
    pop();
    return true;
}

bool XMLParser::
beginNull()
{
    push(Null::New());
    return true;
}

bool XMLParser::
endNull()
{
    pop();
    return true;
}

bool XMLParser::
beginBoolean(bool value)
{
    push(Boolean::New(value));
    return true;
}

bool XMLParser::
endBoolean()
{
    pop();
    return true;
}

bool XMLParser::
beginData()
{
    push(Data::New());
    _cdata.clear();
    return true;
}

bool XMLParser::
endData()
{
    CastTo <Data> (_state.current)->setBase64Value(_cdata);
    pop();
    return true;
}

bool XMLParser::
beginDate()
{
    push(Date::New());
    _cdata.clear();
    return true;
}

bool XMLParser::
endDate()
{
    CastTo <Date> (_state.current)->setStringValue(_cdata);
    pop();
    return true;
}

bool XMLParser::
beginKey()
{
    _state.key.valid = false;
    _state.key.active = true;
    _cdata.clear();
    return true;
}

bool XMLParser::
endKey()
{
    _state.key.active = false;
    _state.key.valid = true;
    _state.key.value = _cdata;
    _cdata.clear();
    return true;
}