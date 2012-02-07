/*##############################################################################

    Copyright (C) 2011 HPCC Systems.

    All rights reserved. This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
############################################################################## */
#ifndef __HQLPREGEX_HPP_
#define __HQLPREGEX_HPP_

#ifndef CHEAP_UCHAR_DEF
#define CHEAP_UCHAR_DEF
#ifdef _WIN32
typedef wchar_t UChar;
#else //__WIN32
typedef unsigned short UChar;
#endif //__WIN32
#endif

IHqlExpression * convertPatternToExpression(unsigned len, const char * text);
IHqlExpression * convertUtf8PatternToExpression(unsigned len, const char * text);
IHqlExpression * convertPatternToExpression(unsigned len, const UChar * text);

#endif
