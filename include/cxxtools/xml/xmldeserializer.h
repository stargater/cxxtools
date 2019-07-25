/*
 * Copyright (C) 2008 by Marc Boris Duerner
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
#ifndef xis_cxxtools_XmlDeserializer_h
#define xis_cxxtools_XmlDeserializer_h

#include <cxxtools/string.h>
#include <cxxtools/deserializer.h>
#include "cxxtools/xml/xmlreader.h"
#include "cxxtools/xml/startelement.h"
#include <sstream>

namespace cxxtools
{

namespace xml
{

    class XmlReader;

    /** @brief Deserialize objects or object data to XML

        Thic class performs XML deserialization of a single object or
        object data.
    */
    class XmlDeserializer : public Deserializer
    {
        public:
            /** Initializes a deserializer.

                To read a xml structure, the parse method has to be called.
             */
            explicit XmlDeserializer(bool readAttributes = false, const String& attributePrefix = cxxtools::String())
                : _readAttributes(readAttributes),
                  _attributePrefix(attributePrefix)
            { }

            /** Initializes a deserializer and reads a xml structure into the underlying SerializationInfo.
             */
            explicit XmlDeserializer(XmlReader& reader, bool readAttributes = false, const String& attributePrefix = cxxtools::String());

            /** Initializes a deserializer and reads a xml structure into the underlying SerializationInfo.
             */
            explicit XmlDeserializer(std::istream& is, bool readAttributes = false, const String& attributePrefix = cxxtools::String());

            /** Reads a xml structure into the underlying SerializationInfo.
             */
            void parse(XmlReader& reader);

            /** Reads a xml structure into the underlying SerializationInfo.
             */
            void parse(std::istream& is);

            /** Reads a xml structure into the underlying SerializationInfo.
             */
            void parse(std::basic_istream<Char>& is);

            /** Specifies whether xml attributes should be read into members.

                By default attributes are ignored. When the flag is set,
                attributes are added as scalar members.
             */
            void readAttributes(bool readAttributes)
            { _readAttributes = readAttributes; }

            /** Returns true, when the readAttribute flag is set.
             */
            bool readAttributes() const
            { return _readAttributes; }

            /** When reading attributes, the member names can be prefixed with a string.

                The string is set with this method.
             */
            void attributePrefix(const String& attributePrefix)
            { _attributePrefix = attributePrefix; }

            /** When reading attributes, the member names can be prefixed with a string.

                The currently set string is returned with this method.
             */
            const String& attributePrefix() const
            { return _attributePrefix; }

            template <typename T>
            static void toObject(const std::string& str, T& type, bool readAttributes = false)
            {
               std::istringstream in(str);
               XmlDeserializer d(in, readAttributes);
               d.deserialize(type);
            }

            template <typename T>
            static void toObject(XmlReader& in, T& type, bool readAttributes = false)
            {
               XmlDeserializer d(in, readAttributes);
               d.deserialize(type);
            }

            template <typename T>
            static void toObject(std::istream& in, T& type, bool readAttributes = false)
            {
               XmlDeserializer d(in, readAttributes);
               d.deserialize(type);
            }

        private:

            //! @internal
            void beginDocument(XmlReader& reader);

            //! @internal
            void onRootElement(XmlReader& reader);

            //! @internal
            void onStartElement(XmlReader& reader);

            //! @internal
            void onWhitespace(XmlReader& reader);

            //! @internal
            void onContent(XmlReader& reader);

            //! @internal
            void onEndElement(XmlReader& reader);

            //! @internal
            typedef void (XmlDeserializer::*ProcessNode)(XmlReader&);

            //! @internal
            ProcessNode _processNode;

            size_t _startDepth;

            bool _readAttributes;

            //! @internal
            String _nodeName;

            String _nodeId;

            String _nodeType;

            String _nodeCategory;

            Attributes _attributes;

            String _attributePrefix;

            SerializationInfo::Category nodeCategory() const;

            void processAttributes(const Attributes& attributes);

    };

} // namespace xml

} // namespace cxxtools

#endif
