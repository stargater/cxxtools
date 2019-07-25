/*
 * Copyright (C) 2007 Tommi Maekitalo
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

#ifndef CXXTOOLS_PROPERTIESPARSER_H
#define CXXTOOLS_PROPERTIESPARSER_H

#include <string>
#include <cxxtools/char.h>
#include <cxxtools/string.h>
#include <cxxtools/textstream.h>
#include <cxxtools/serializationerror.h>

namespace cxxtools
{
  class PropertiesParser
  {
    public:
      class Event
      {
        public:
          virtual ~Event()  { }
          // return true, if parser should stop
          virtual bool onKeyPart(const String& key) = 0;
          virtual bool onKey(const String& key) = 0;
          virtual bool onValue(const String& value) = 0;
      };

    private:
      Event& _event;
      String _key;
      String _keypart;
      String _value;
      String _space;
      Char::value_type _unicode;
      unsigned _unicodeCount;
      unsigned _lineNo;
      bool _trim;

      enum {
        state_0,
        state_key,
        state_key_esc,
        state_key_unicode,
        state_key_sp,
        state_value0,
        state_value,
        state_value_esc,
        state_value_cont,
        state_value_space,
        state_unicode,
        state_comment
      } _state;

    public:
      PropertiesParser(Event& event_, bool trim_ = false)
        : _event(event_),
          _lineNo(1),
          _trim(trim_),
          _state(state_0)
        { }

      bool trim() const   { return _trim; }
      void trim(bool sw)  { _trim = sw; }
      void parse(std::basic_istream<Char>& in);
      void parse(std::istream& in, TextCodec<Char, char>* codec = 0);
      bool advance(Char ch);
      void end();
  };

  class PropertiesParserError : public SerializationError
  {
    public:
      explicit PropertiesParserError(const std::string& msg)
        : SerializationError(msg)
        { }
      PropertiesParserError(const std::string& msg, unsigned lineNo);
  };
}

#endif // CXXTOOLS_PROPERTIESPARSER_H
