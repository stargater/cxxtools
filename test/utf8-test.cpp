/*
 * Copyright (C) 2013 Tommi Maekitalo
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

#include "cxxtools/utf8codec.h"
#include "cxxtools/utf8.h"
#include "cxxtools/unit/testsuite.h"
#include "cxxtools/unit/registertest.h"
#include "cxxtools/string.h"

class Utf8Test : public cxxtools::unit::TestSuite
{

  public:
    Utf8Test()
    : cxxtools::unit::TestSuite("utf8")
    {
      registerMethod("encode", *this, &Utf8Test::encodeTest);
      registerMethod("decode", *this, &Utf8Test::decodeTest);
      registerMethod("byteordermark", *this, &Utf8Test::byteordermarkTest);
      registerMethod("incompleteBom", *this, &Utf8Test::incompleteBomTest);
      registerMethod("partialBom", *this, &Utf8Test::partialBomTest);
    }

    void encodeTest()
    {
      cxxtools::String ustr(L"Hi \xe4 there");
      std::string bstr = cxxtools::Utf8Codec::encode(ustr);
      CXXTOOLS_UNIT_ASSERT_EQUALS(bstr, "Hi \xc3\xa4 there");

      std::string bstr2 = cxxtools::Utf8(ustr);
      CXXTOOLS_UNIT_ASSERT_EQUALS(bstr2, "Hi \xc3\xa4 there");
    }

    void decodeTest()
    {
      std::string bstr("Hi \xc3\xa4 there");
      cxxtools::String ustr = cxxtools::Utf8Codec::decode(bstr);
      CXXTOOLS_UNIT_ASSERT(ustr == L"Hi \xe4 there");
    }

    void byteordermarkTest()
    {
      std::string bstr("\xef\xbb\xbfHi \xc3\xa4 there");
      cxxtools::String ustr = cxxtools::Utf8Codec::decode(bstr);
      CXXTOOLS_UNIT_ASSERT(ustr == L"Hi \xe4 there");
    }

    void incompleteBomTest()
    {
      std::string bstr("\xef\xbb");
      cxxtools::String ustr = cxxtools::Utf8Codec::decode(bstr);
      CXXTOOLS_UNIT_ASSERT(ustr.empty());
    }

    void partialBomTest()
    {
      // check whether codec is able to partially consume byte order mark
      cxxtools::Utf8Codec codec;
      const char data[] = { '\xef', '\xbb', '\xbf', 'A' };
      cxxtools::Char to[10];
      const char* fromBegin = data;
      const char* fromNext = data;
      cxxtools::Char* toNext = to;
      cxxtools::MBState mbstate;

      // consume first char
      codec.in(mbstate, fromBegin, fromBegin + 1, fromNext, to, to + 10, toNext);
      CXXTOOLS_UNIT_ASSERT(fromNext > fromBegin);  // codec must consume char
      CXXTOOLS_UNIT_ASSERT_EQUALS(toNext - to, 0);   // no output yet

      fromBegin = fromNext;

      codec.in(mbstate, fromBegin, fromBegin + 1, fromNext, to, to + 10, toNext);
      CXXTOOLS_UNIT_ASSERT(fromNext > fromBegin);  // codec must consume another char
      CXXTOOLS_UNIT_ASSERT_EQUALS(toNext - to, 0);   // no output yet

      fromBegin = fromNext;

      codec.in(mbstate, fromBegin, fromBegin + 2, fromNext, to, to + 10, toNext);
      CXXTOOLS_UNIT_ASSERT(fromNext > fromBegin);  // codec must consume the third and forth char
      CXXTOOLS_UNIT_ASSERT_EQUALS(toNext - to, 1);   // now output
      CXXTOOLS_UNIT_ASSERT_EQUALS(to[0].narrow(), 'A');   // now output
    }

};

cxxtools::unit::RegisterTest<Utf8Test> register_Utf8Test;
