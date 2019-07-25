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

#ifndef CXXTOOLS_DATEUTILS_H
#define CXXTOOLS_DATEUTILS_H

#include <string>

namespace cxxtools
{
  extern const char* weekdaynames[7];
  extern const char* monthnames[12];

  void skipNonDigit(std::string::const_iterator& b, std::string::const_iterator e);
  void skipWord(std::string::const_iterator& b, std::string::const_iterator e);
  unsigned getUnsigned(std::string::const_iterator& b, std::string::const_iterator e, unsigned digits);
  unsigned getUnsignedF(std::string::const_iterator& b, std::string::const_iterator e, unsigned digits);
  unsigned getInt(std::string::const_iterator& b, std::string::const_iterator e, unsigned digits);
  unsigned getMicroseconds(std::string::const_iterator& b, std::string::const_iterator e, unsigned digits);
  void appendDn(std::string& s, unsigned short n, unsigned v);
  void appendDn(std::string& s, unsigned short n, int v);
  unsigned getMonthFromName(std::string::const_iterator& b, std::string::const_iterator e);

}

#endif // CXXTOOLS_DATEUTILS_H

