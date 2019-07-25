/*
 * Copyright (C) 2005-2008 by Dr. Marc Boris Duerner
 * Copyright (C) 2011 by Tommi Maekitalo
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * As a special exception, you may use this file as part of a free
 * software library without restriction. Specifically, if other files
 * instantiate templates or use macros or inline functions from this
 * file, or you compile this file and link it with other files to
 * produce an executable, this file does not by itself cause the
 * resulting executable to be covered by the GNU General Public
 * License. This exception does not however invalidate any other
 * reasons why the executable file might be covered by the GNU Library
 * General Public License.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <cxxtools/serializationinfo.h>
#include <cxxtools/serializationerror.h>
#include <cxxtools/conversionerror.h>
#include <cxxtools/convert.h>
#include <cxxtools/log.h>

#include <stdexcept>
#include <sstream>

log_define("cxxtools.serializationinfo")

namespace cxxtools
{

SerializationInfo::SerializationInfo(const SerializationInfo& si)
: _category(si._category),
  _name(si._name),
  _type(si._type),
  _u(si._u),
  _t(si._t),
  _nodes(0)
{
    switch (_t)
    {
        case t_string:  new (_StringPtr()) String(si._String());
                        break;

        case t_string8: new (_String8Ptr()) std::string(si._String8());
                        break;

        default:
            ;
    }

    if (si._nodes)
        _nodes = new Nodes(*si._nodes);
}


SerializationInfo& SerializationInfo::operator=(const SerializationInfo& si)
{
    if (this == &si)
        return *this;

    assignData(si);
    _name = si._name;

    return *this;
}


#if __cplusplus >= 201103L

SerializationInfo::SerializationInfo(SerializationInfo&& si) noexcept
    : _category(si._category),
      _name(std::move(si._name)),
      _type(std::move(si._type)),
      _u(si._u),
      _t(si._t),
      _nodes(si._nodes)
{
    if (si._t == t_string)
    {
        new (_StringPtr()) String(std::move(*si._StringPtr()));
    }
    else if (si._t == t_string8)
    {
        new (_String8Ptr()) std::string(std::move(*si._String8Ptr()));
    }

    si._nodes = 0;
}


SerializationInfo& SerializationInfo::operator=(SerializationInfo&& si)
{
    _category = si._category;
    _name = std::move(si._name);
    _type = std::move(si._type);
    _nodes = si._nodes;
    si._nodes = 0;

    if (si._t == t_string)
    {
        if (_t != t_string)
        {
            _releaseValue();
            new (_StringPtr()) String();
            _t = t_string;
        }

        _String().swap(si._String());
    }
    else if (si._t == t_string8)
    {
        if (_t != t_string8)
        {
            _releaseValue();
            new (_String8Ptr()) std::string();
            _t = t_string8;
        }

        _String8().swap(si._String8());
    }
    else
    {
        _releaseValue();
        _t = si._t;
        _u = si._u;
    }

    return *this;
}


#endif

SerializationInfo::~SerializationInfo()
{
    _releaseValue();
    delete _nodes;
}

SerializationInfo& SerializationInfo::addMember(const std::string& name)
{
    log_debug("addMember(\"" << name << "\")");

    Nodes& n = nodes();
    n.push_back(SerializationInfo());
    n.back().setName(name);

    // category Array overrides Object
    // This is needed for xmldeserialization. In the xml file the root node of a array
    // has a category attribute. When the serializationinfo of the array is created
    // it is known, that it is of category Array. When the members of the array are read,
    // they should not make an object out of the array.
    if (_category != Array && _category != Object)
        _category = name.empty() ? Array : Object;

    return n.back();
}


const SerializationInfo& SerializationInfo::getMember(const std::string& name) const
{
    log_debug("getMember(\"" << name << "\")");

    const Nodes& n = nodes();

    for (Nodes::const_iterator it = n.begin(); it != n.end(); ++it)
    {
        if( it->name() == name )
            return *it;
    }

    throw SerializationMemberNotFound(name);
}


const SerializationInfo& SerializationInfo::getMember(unsigned idx) const
{
    log_debug("getMember(" << idx << ')');

    const Nodes& n = nodes();

    if (idx >= n.size())
    {
        std::ostringstream msg;
        msg << "requested member index " << idx << " exceeds number of members " << n.size();
        throw std::range_error(msg.str());
    }

    return n[idx];
}


const SerializationInfo* SerializationInfo::findMember(const std::string& name) const
{
    log_debug("findMember(\"" << name << "\")");

    const Nodes& n = nodes();

    for (Nodes::const_iterator it = n.begin(); it != n.end(); ++it)
    {
        if( it->name() == name )
            return &(*it);
    }

    return 0;
}


SerializationInfo* SerializationInfo::findMember(const std::string& name)
{
    log_debug("findMember(\"" << name << "\")");

    Nodes& n = nodes();

    for (Nodes::iterator it = n.begin(); it != n.end(); ++it)
    {
        if( it->name() == name )
            return &(*it);
    }

    return 0;
}

void SerializationInfo::clear()
{
    _category = Void;
    _name.clear();
    _type.clear();
    nodes().clear();
    _releaseValue();
}

void SerializationInfo::swap(SerializationInfo& si)
{
    if (this == &si)
        return;

    std::swap(_category, si._category);
    std::swap(_name, si._name);
    std::swap(_type, si._type);

    if (_t == t_string)
    {
        if (si._t == t_string)
        {
            // this: String, other: String
            _String().swap(si._String());
        }
        else if (si._t == t_string8)
        {
            // this: String, other: std::string
            std::string s;
            si._String8().swap(s);
            si.setValue(String());
            si._String().swap(_String());
            setValue(s);
        }
        else
        {
            // this: String, other: something
            U u = si._u;
            T t = si._t;
            si.setValue(String());
            _String().swap(si._String());
            _releaseValue();
            _u = u;
            _t = t;
        }
    }
    else if (_t == t_string8)
    {
        if (si._t == t_string)
        {
            // this: std::string, other: String
            String s;
            si._String().swap(s);
            si.setValue(std::string());
            _String8().swap(si._String8());
            setValue(s);
        }
        else if (si._t == t_string8)
        {
            // this: std::string, other: std::string
            _String8().swap(si._String8());
        }
        else
        {
            // this: std::string, other: something
            U u = si._u;
            T t = si._t;
            si.setValue(_String8());
            _releaseValue();
            _u = u;
            _t = t;
        }
    }
    else
    {
        if (si._t == t_string)
        {
            // this: something, other: String
            U u = _u;
            T t = _t;
            setValue(si._String());
            si._releaseValue();
            si._u = u;
            si._t = t;
        }
        else if (si._t == t_string8)
        {
            // this: something, other: std::string
            U u = _u;
            T t = _t;
            setValue(si._String8());
            si._releaseValue();
            si._u = u;
            si._t = t;
        }
        else
        {
            // this: something, other: something
            std::swap(_u, si._u);
            std::swap(_t, si._t);
        }
    }

    std::swap(_nodes, si._nodes);
}

void SerializationInfo::dump(std::ostream& out, const std::string& prefix) const
{
    if (!_name.empty())
        out << prefix << "name = \"" << _name << "\"\n";

    if (_t != t_none)
    {
        out << prefix << "type = " << (_t == t_none ? "none" :
                                       _t == t_string ? "string" :
                                       _t == t_string8 ? "string8" :
                                       _t == t_char ? "char" :
                                       _t == t_bool ? "bool" :
                                       _t == t_int ? "int" :
                                       _t == t_uint ? "uint" :
                                       _t == t_ldouble ? "double" : "?") << '\n';

        out << prefix << "value = ";

        switch (_t)
        {
            case t_none:    out << '-'; break;
            case t_string:  out << '"' << _String().narrow() << '"'; break;
            case t_string8: out << '"' << _String8() << '"'; break;
            case t_char:    out << '\'' << _u._c << '\''; break;
            case t_bool:    out << _u._b; break;
            case t_int:     out << _u._i; break;
            case t_uint:    out << _u._u; break;
            case t_float:   out << _u._f; break;
            case t_double:  out << _u._d; break;
            case t_ldouble: out << _u._ld; break;
        }

        out << '\n';
    }

    if (!_type.empty())
        out << prefix << "typeName = " << _type << '\n';
    out << prefix << "category = " << static_cast<unsigned>(_category) << '\n';

    const Nodes& n = nodes();
    if (!n.empty())
    {
        std::string p = prefix + '\t';
        for (Nodes::size_type i = 0; i < n.size(); ++i)
        {
            out << prefix << "node[" << i << "]\n";
            n[i].dump(out, p);
        }
    }
}

void SerializationInfo::_releaseValue()
{
    switch (_t)
    {
        case t_string: _String().~String(); break;
        case t_string8:
        {
            // I don't know how to call the destructor without 'using' so that
            // both gcc and xlc understand it.
            using std::string;
            _String8().~string();
            break;
        }

        default:
            ;
    }
    _t = t_none;
}

void SerializationInfo::setNull()
{
    _releaseValue();
    _t = t_none;
    _category = Value;
}

void SerializationInfo::_setString(const String& value)
{
    if (_t != t_string)
    {
        _releaseValue();
        new (_StringPtr()) String(value);
        _t = t_string;
    }
    else
    {
        _String().assign(value);
    }

    _category = Value;
}

void SerializationInfo::_setString8(const std::string& value)
{
    if (_t != t_string8)
    {
        _releaseValue();
        new (_String8Ptr()) std::string(value);
        _t = t_string8;
    }
    else
    {
        _String8().assign(value);
    }

    _category = Value;
}

void SerializationInfo::_setString8(const char* value)
{
    if (_t != t_string8)
    {
        _releaseValue();
        new (_String8Ptr()) std::string(value);
        _t = t_string8;
    }
    else
    {
        _String8().assign(value);
    }

    _category = Value;
}

void SerializationInfo::_setChar(char value)
{
    if (_t != t_char)
    {
        _releaseValue();
        _t = t_char;
    }

    _u._c = value;

    _category = Value;
}

void SerializationInfo::_setBool(bool value)
{
    if (_t != t_bool)
    {
        _releaseValue();
        _t = t_bool;
    }

    _u._b = value;

    _category = Value;
}

void SerializationInfo::_setInt(int_type value)
{
    if (_t != t_int)
    {
        _releaseValue();
        _t = t_int;
    }

    _u._i = value;

    _category = Value;
}

void SerializationInfo::_setUInt(unsigned_type value)
{
    if (_t != t_uint)
    {
        _releaseValue();
        _t = t_uint;
    }

    _u._u= value;

    _category = Value;
}

void SerializationInfo::_setFloat(float value)
{
    if (_t != t_float)
    {
        _releaseValue();
        _t = t_float;
    }

    _u._f = value;

    _category = Value;
}

void SerializationInfo::_setDouble(double value)
{
    if (_t != t_double)
    {
        _releaseValue();
        _t = t_double;
    }

    _u._d = value;

    _category = Value;
}

void SerializationInfo::_setLongDouble(long double value)
{
    if (_t != t_ldouble)
    {
        _releaseValue();
        _t = t_ldouble;
    }

    _u._ld = value;

    _category = Value;
}

void SerializationInfo::getValue(String& value) const
{
    switch (_t)
    {
        case t_none:    value.clear(); break;
        case t_string:  value.assign(_String()); break;
        case t_string8: value.assign(_String8()); break;
        case t_char:    value.assign(1, _u._c); break;
        case t_bool:    convert(value, _u._b); break;
        case t_int:     convert(value, _u._i); break;
        case t_uint:    convert(value, _u._u); break;
        case t_float:   convert(value, _u._f); break;
        case t_double:  convert(value, _u._d); break;
        case t_ldouble: convert(value, _u._ld); break;
    }
}

void SerializationInfo::getValue(std::string& value) const
{
    switch (_t)
    {
        case t_none:    value.clear(); break;
        case t_string:  value = _String().narrow(); break;
        case t_string8: value.assign(_String8()); break;
        case t_char:    value.assign(1, _u._c); break;
        case t_bool:    convert(value, _u._b); break;
        case t_int:     convert(value, _u._i); break;
        case t_uint:    convert(value, _u._u); break;
        case t_float:   convert(value, _u._f); break;
        case t_double:  convert(value, _u._d); break;
        case t_ldouble: convert(value, _u._ld); break;
    }
}

namespace
{
    inline bool isFalse(char c)
    {
        return c == '\0'
            || c == '0'
            || c == 'f'
            || c == 'F'
            || c == 'n'
            || c == 'N';
    }
}

bool SerializationInfo::_getBool() const
{
    switch (_t)
    {
        case t_none:    return false;
        case t_string:  return !_String().empty() && !isFalse((_String())[0].narrow());
        case t_string8: return !_String8().empty() && !isFalse((_String8())[0]);
        case t_char:    return !isFalse(_u._c);
        case t_bool:    return _u._b;
        case t_int:     return _u._i;
        case t_uint:    return _u._u;
        case t_float:   return _u._f;
        case t_double:  return _u._d;
        case t_ldouble: return _u._ld;
    }

    // never reached
    return false;
}

wchar_t SerializationInfo::_getWChar() const
{
    switch (_t)
    {
        case t_none:    return L'\0';
        case t_string:  return _String().empty() ? L'\0' : _String()[0].toWchar();
        case t_string8: return _String8().empty() ? '\0' : _String8()[0];
        case t_char:    return _u._c;
        case t_bool:    return _u._b;
        case t_int:     return _u._i;
        case t_uint:    return _u._u;
        case t_float:   return static_cast<wchar_t>(_u._f);
        case t_double:  return static_cast<wchar_t>(_u._d);
        case t_ldouble: return static_cast<wchar_t>(_u._ld);
    }

    // never reached
    return 0;
}

char SerializationInfo::_getChar() const
{
    switch (_t)
    {
        case t_none:    return '\0'; break;
        case t_string:  return _String().empty() ? '\0' : (_String())[0].narrow(); break;
        case t_string8: return _String8().empty() ? '\0' : (_String8())[0]; break;
        case t_char:    return _u._c; break;
        case t_bool:    return _u._b; break;
        case t_int:     return _u._i; break;
        case t_uint:    return _u._u; break;
        case t_float:   return static_cast<char>(_u._f); break;
        case t_double:   return static_cast<char>(_u._d); break;
        case t_ldouble:   return static_cast<char>(_u._ld); break;
    }
    // never reached
    return 0;
}

SerializationInfo::int_type SerializationInfo::_getInt(const char* type, int_type min, int_type max) const
{
    int_type ret = 0;

    switch (_t)
    {
        case t_none:    break;

        case t_string:  try
                        {
                            ret = convert<int_type>(_String());
                        }
                        catch (const ConversionError&)
                        {
                            ConversionError::doThrow(type, "String", _String().narrow().c_str());
                        }
                        break;

        case t_string8: try
                        {
                            ret = convert<int_type>(_String8());
                        }
                        catch (const ConversionError&)
                        {
                            ConversionError::doThrow(type, "string", _String8().c_str());
                        }
                        break;

        case t_char:    ret = _u._c - '0'; break;
        case t_bool:    ret = _u._b; break;
        case t_int:     ret = _u._i; break;
        case t_uint:    if (_u._u > static_cast<unsigned_type>(std::numeric_limits<int_type>::max()))
                        {
                            std::ostringstream msg;
                            msg << "value " << ret << " does not fit into " << type;
                            throw std::range_error(msg.str());
                        }
                        ret = _u._u; break;
        case t_float:   ret = static_cast<int_type>(_u._f); break;
        case t_double:   ret = static_cast<int_type>(_u._d); break;
        case t_ldouble:   ret = static_cast<int_type>(_u._ld); break;
    }

    if (ret < min || ret > max)
    {
        std::ostringstream msg;
        msg << "value " << ret << " does not fit into " << type;
        throw std::range_error(msg.str());
    }

    return ret;
}

SerializationInfo::unsigned_type SerializationInfo::_getUInt(const char* type, unsigned_type max) const
{
    unsigned_type ret = 0;

    switch (_t)
    {
        case t_none:    break;

        case t_string:  try
                        {
                            ret = convert<unsigned_type>(_String());
                        }
                        catch (const ConversionError&)
                        {
                            ConversionError::doThrow(type, "String", _String().narrow().c_str());
                        }
                        break;

        case t_string8: try
                        {
                            ret = convert<unsigned_type>(_String8());
                        }
                        catch (const ConversionError&)
                        {
                            ConversionError::doThrow(type, "string", _String8().c_str());
                        }
                        break;

        case t_char:    ret = _u._c - '0'; break;
        case t_bool:    ret = _u._b; break;
        case t_int:     if (_u._i < 0)
                            throw std::range_error(std::string("negative values do not fit into ") + type);
                        ret = _u._i;
                        break;
        case t_uint:    ret = _u._u; break;
        case t_float:   ret = static_cast<unsigned_type>(_u._f); break;
        case t_double:  ret = static_cast<unsigned_type>(_u._d); break;
        case t_ldouble: ret = static_cast<unsigned_type>(_u._ld); break;
    }

    if (ret > max)
    {
        std::ostringstream msg;
        msg << "value " << ret << " does not fit into " << type;
        throw std::range_error(msg.str());
    }

    return ret;
}

float SerializationInfo::_getFloat() const
{
    float ret = 0;

    switch (_t)
    {
        case t_none:    break;

        case t_string:  try
                        {
                            ret = convert<float>(_String());
                        }
                        catch (const ConversionError&)
                        {
                            ConversionError::doThrow("float", "String", _String().narrow().c_str());
                        }
                        break;

        case t_string8: try
                        {
                            ret = convert<float>(_String8());
                        }
                        catch (const ConversionError&)
                        {
                            ConversionError::doThrow("float", "string", _String8().c_str());
                        }
                        break;

        case t_char:    ret = _u._c - '0'; break;
        case t_bool:    ret = _u._b; break;
        case t_int:     ret = _u._i; break;
        case t_uint:    ret = _u._u; break;
        case t_float:   ret = _u._f; break;

        case t_double:
            if (_u._d == std::numeric_limits<double>::infinity())
                ret = std::numeric_limits<float>::infinity();
            else if (_u._d == -std::numeric_limits<double>::infinity())
                ret = -std::numeric_limits<float>::infinity();
            else if (_u._d != _u._d)  // NaN
                ret = _u._d;
            else if (_u._d > std::numeric_limits<float>::max()
                  || _u._d < -std::numeric_limits<float>::max())
            {
                std::ostringstream msg;
                msg << "value " << _u._d << " does not fit into float";
                throw std::range_error(msg.str());
            }
            else
                ret = static_cast<float>(_u._d);
            break;

        case t_ldouble:
            if (_u._ld == std::numeric_limits<long double>::infinity())
                ret = std::numeric_limits<float>::infinity();
            else if (_u._ld == -std::numeric_limits<long double>::infinity())
                ret = -std::numeric_limits<float>::infinity();
            else if (_u._ld != _u._ld)  // NaN
                ret = _u._ld;
            else if (_u._ld > std::numeric_limits<float>::max()
                  || _u._ld < -std::numeric_limits<float>::max())
            {
                std::ostringstream msg;
                msg << "value " << _u._ld << " does not fit into float";
                throw std::range_error(msg.str());
            }
            else
                ret = static_cast<float>(_u._ld);
            break;
    }

    return ret;
}

double SerializationInfo::_getDouble() const
{
    double ret = 0;

    switch (_t)
    {
        case t_none:    break;

        case t_string:  try
                        {
                            ret = convert<double>(_String());
                        }
                        catch (const ConversionError&)
                        {
                            ConversionError::doThrow("double", "String", _String().narrow().c_str());
                        }
                        break;

        case t_string8: try
                        {
                            ret = convert<double>(_String8());
                        }
                        catch (const ConversionError&)
                        {
                            ConversionError::doThrow("double", "string", _String8().c_str());
                        }
                        break;

        case t_char:    ret = _u._c - '0'; break;
        case t_bool:    ret = _u._b; break;
        case t_int:     ret = _u._i; break;
        case t_uint:    ret = _u._u; break;
        case t_float:   ret = _u._f; break;
        case t_double:  ret = _u._d; break;

        case t_ldouble:
            if (_u._ld == std::numeric_limits<long double>::infinity())
                ret = std::numeric_limits<double>::infinity();
            else if (_u._ld == -std::numeric_limits<long double>::infinity())
                ret = -std::numeric_limits<double>::infinity();
            else if (_u._ld != _u._ld)  // NaN
                ret = _u._ld;
            else if (_u._ld > std::numeric_limits<double>::max()
                  || _u._ld < -std::numeric_limits<double>::max())
            {
                std::ostringstream msg;
                msg << "value " << _u._ld << " does not fit into double";
                throw std::range_error(msg.str());
            }
            else
                ret = static_cast<double>(_u._ld);
            break;
    }

    return ret;
}

long double SerializationInfo::_getLongDouble() const
{
    long double ret = 0;

    switch (_t)
    {
        case t_none:    break;

        case t_string:  try
                        {
                            ret = convert<long double>(_String());
                        }
                        catch (const ConversionError&)
                        {
                            ConversionError::doThrow("long double", "String", _String().narrow().c_str());
                        }
                        break;

        case t_string8: try
                        {
                            ret = convert<long double>(_String8());
                        }
                        catch (const ConversionError&)
                        {
                            ConversionError::doThrow("long double", "string", _String8().c_str());
                        }
                        break;

        case t_char:    ret = _u._c - '0'; break;
        case t_bool:    ret = _u._b; break;
        case t_int:     ret = _u._i; break;
        case t_uint:    ret = _u._u; break;
        case t_float:   ret = _u._f; break;
        case t_double:  ret = _u._d; break;
        case t_ldouble: ret = _u._ld; break;
    }

    return ret;
}

SerializationInfo::Nodes& SerializationInfo::nodes()
{
    if (!_nodes)
        _nodes = new Nodes;

    return *_nodes;
}

const SerializationInfo::Nodes& SerializationInfo::nodes() const
{
    static const Nodes emptyNodes;

    if (_nodes)
        return *_nodes;
    else
        return emptyNodes;
}

void SerializationInfo::assignData(const SerializationInfo& si)
{
    _category = si._category;
    _type = si._type;

    delete _nodes;
    _nodes = 0;
    if (si._nodes)
        _nodes = new Nodes(*si._nodes);

    if (si._t == t_string)
        _setString( si._String() );
    else if (si._t == t_string8)
        _setString8( si._String8() );
    else
    {
        _releaseValue();
        _u = si._u;
        _t = si._t;
    }

}

void operator <<=(SerializationInfo& si, const SerializationInfo& ssi)
{
    si.assignData(ssi);
}



} // namespace cxxtools
