/*
 * Copyright (C) 2003,2004 Tommi Maekitalo
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

#include "cxxtools/query_params.h"
#include "cxxtools/serializationinfo.h"
#include "cxxtools/serializationerror.h"
#include "cxxtools/utf8codec.h"
#include "cxxtools/log.h"

#include <iterator>
#include <iostream>
#include <stdlib.h>

log_define("cxxtools.queryparams")

namespace cxxtools
{
namespace
{
  class UrlParser
  {
      QueryParams& _q;
      enum {
        state_0,
        state_key,
        state_value,
        state_keyesc,
        state_valueesc
      } _state;

      std::string _key;
      std::string _value;
      unsigned _cnt;
      unsigned _v;

    public:
      explicit UrlParser(QueryParams& q)
        : _q(q),
          _state(state_0),
          _cnt(0),
          _v(0)
      { }

      void parse(char ch);
      void finish();
  };

  void UrlParser::parse(char ch)
  {
    switch(_state)
    {
      case state_0:
        if (ch == '=')
          _state = state_value;
        else if (ch == '&')
          ;
        else if (ch == '%')
          _state = state_keyesc;
        else if (ch == '+')
        {
          _key = ' ';
          _state = state_key;
        }
        else
        {
          _key = ch;
          _state = state_key;
        }
        break;

      case state_key:
        if (ch == '=')
          _state = state_value;
        else if (ch == '&')
        {
          _q.add(_key);
          _key.clear();
          _state = state_0;
        }
        else if (ch == '%')
          _state = state_keyesc;
        else if (ch == '+')
          _key += ' ';
        else
          _key += ch;
        break;

      case state_value:
        if (ch == '%')
          _state = state_valueesc;
        else if (ch == '&')
        {
          _q.add(_key, _value);
          _key.clear();
          _value.clear();
          _state = state_0;
        }
        else if (ch == '+')
          _value += ' ';
        else
          _value += ch;
        break;

      case state_keyesc:
      case state_valueesc:
        if (ch >= '0' && ch <= '9')
        {
          ++_cnt;
          _v = (_v << 4) + (ch - '0');
        }
        else if (ch >= 'a' && ch <= 'f')
        {
          ++_cnt;
          _v = (_v << 4) + (ch - 'a' + 10);
        }
        else if (ch >= 'A' && ch <= 'F')
        {
          ++_cnt;
          _v = (_v << 4) + (ch - 'A' + 10);
        }
        else
        {
          if (_cnt == 0)
          {
            if (_state == state_keyesc)
            {
              _key += '%';
              _state = state_key;
            }
            else
            {
              _value += '%';
              _state = state_value;
            }
          }
          else
          {
            if (_state == state_keyesc)
            {
              _key += static_cast<char>(_v);
              _state = state_key;
            }
            else
            {
              _value += static_cast<char>(_v);
              _state = state_value;
            }

            _cnt = 0;
            _v = 0;
          }

          parse(ch);
          break;
        }

        if (_cnt >= 2)
        {
          if (_state == state_keyesc)
          {
            _key += static_cast<char>(_v);
            _state = state_key;
          }
          else
          {
            _value += static_cast<char>(_v);
            _state = state_value;
          }
          _cnt = 0;
          _v = 0;
        }

        break;

    }

  }

  void UrlParser::finish()
  {
    switch(_state)
    {
      case state_0:
        break;

      case state_key:
        if (!_key.empty())
        {
          _q.add(_key);
          _key.clear();
        }
        break;

      case state_value:
        _q.add(_key, _value);
        _key.clear();
        _value.clear();
        break;

      case state_keyesc:
      case state_valueesc:
        if (_cnt == 0)
        {
          if (_state == state_keyesc)
          {
            _key += '%';
            _q.add(_key);
          }
          else
          {
            _value += '%';
            _q.add(_key, _value);
          }
        }
        else
        {
          if (_state == state_keyesc)
          {
            _key += static_cast<char>(_v);
            _q.add(_key);
          }
          else
          {
            _value += static_cast<char>(_v);
            _q.add(_key, _value);
          }
        }

        _value.clear();
        _key.clear();
        _cnt = 0;
        _v = 0;
        break;
    }

  }

  void appendUrl(std::string& url, char ch)
  {
    static const char hex[] = "0123456789ABCDEF";
    if (ch > 32 && ch < 127 && ch != '%' && ch != '+' && ch != '=' && ch != '&')
      url += ch;
    else if (ch == ' ')
      url += '+';
    else
    {
      url += '%';
      char hi = (ch >> 4) & 0x0f;
      char lo = ch & 0x0f;
      url += hex[static_cast<int>(hi)];
      url += hex[static_cast<int>(lo)];
    }
  }

  void appendUrl(std::string& url, const std::string& str)
  {
    for (std::string::const_iterator i = str.begin();
         i != str.end(); ++i)
      appendUrl(url, *i);
  }

}

void QueryParams::parse_url(const std::string& url)
{
  UrlParser p(*this);

  for (std::string::const_iterator it = url.begin(); it != url.end(); ++it)
    p.parse(*it);

  p.finish();
}

void QueryParams::parse_url(const char* url)
{
  UrlParser p(*this);

  while (*url)
  {
    p.parse(*url);
    ++url;
  }

  p.finish();
}

void QueryParams::parse_url(std::istream& url_stream)
{
  UrlParser p(*this);

  char ch;
  while (url_stream.get(ch))
    p.parse(ch);

  p.finish();
}

QueryParams& QueryParams::remove(const std::string& name)
{
  for (size_type nn = 0; nn < _values.size(); )
  {
    if (_values[nn].name == name)
      _values.erase(_values.begin() + nn);
    else
      ++nn;
  }

  return *this;
}

/// get nth named parameter.
const std::string& QueryParams::param(const std::string& name, size_type n) const
{
  for (size_type nn = 0; nn < _values.size(); ++nn)
  {
    if (_values[nn].name == name)
    {
      if (n == 0)
        return _values[nn].value;
      --n;
    }
  }

  static std::string emptyValue;
  return emptyValue;
}

/// get nth named parameter with default value.
std::string QueryParams::param(const std::string& name, size_type n, const std::string& def) const
{
  for (size_type nn = 0; nn < _values.size(); ++nn)
  {
    if (_values[nn].name == name)
    {
      if (n == 0)
        return _values[nn].value;
      --n;
    }
  }

  return def;
}

/// get number of parameters with the given name
QueryParams::size_type QueryParams::paramcount(const std::string& name) const
{
  size_type count = 0;

  for (size_type nn = 0; nn < _values.size(); ++nn)
    if (_values[nn].name == name)
      ++count;

  return count;
}

/// checks if the named parameter exists
bool QueryParams::has(const std::string& name) const
{
  for (size_type nn = 0; nn < _values.size(); ++nn)
    if (_values[nn].name == name)
      return true;

  return false;
}

/// get parameters as url
std::string QueryParams::getUrl() const
{
  std::string url;
  for (size_type nn = 0; nn < _values.size(); ++nn)
  {
    if (nn > 0)
      url += '&';

    if (!_values[nn].name.empty())
    {
      appendUrl(url, _values[nn].name);
      url += '=';
    }

    appendUrl(url, _values[nn].value);
  }

  return url;
}

void operator<<= (cxxtools::SerializationInfo& si, const QueryParams& q)
{
    for (QueryParams::values_type::const_iterator it = q._values.begin(); it != q._values.end(); ++it)
    {
        enum {
            state_0,
            state_key,
            state_keyend
        } state = state_0;

        std::string nodename;

        cxxtools::SerializationInfo* current = &si;
        log_debug("parse query param name <" << it->name << '>');

        for (unsigned n = 0; n < it->name.size(); ++n)
        {
            char ch = it->name[n];
            switch (state)
            {
                case state_0:
                    if (ch == '[')
                    {
                        cxxtools::SerializationInfo* p = current->findMember(nodename);
                        current = p ? p : &current->addMember(nodename);
                        nodename.clear();
                        state = state_key;
                    }
                    else
                        nodename += ch;
                    break;

                case state_key:
                    if (ch == ']')
                        state = state_keyend;
                    else
                        nodename += ch;
                    break;

                case state_keyend:
                    if (ch == '[')
                    {
                        cxxtools::SerializationInfo* p = current->findMember(nodename);
                        current = p ? p : &current->addMember(nodename);
                        nodename.clear();
                        state = state_key;
                    }
                    else
                    {
                        log_warn("invalid query param name <" << it->name.substr(0, n) << " *** " << it->name.substr(n) << "> (1)");
                        SerializationError::doThrow("'[' expected in query parameters");
                    }
                    break;
            }
        }

        current->addMember(nodename) <<= Utf8Codec::decode(it->value);
    }
}

}
