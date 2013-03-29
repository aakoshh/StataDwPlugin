#pragma once // VC++

#ifndef STRUTILS_H
#define STRUTILS_H

#include <string>
#include <sstream>
#include <vector>

using namespace std;

// split string into list
vector<string> &split(const std::string &s, char delim, vector<string> &elems);
vector<string> split(const string &s, char delim);
// string replace
string replaceAll(string str, const string& from, const string& to);

string upperCase(string strToConvert);
string lowerCase(string strToConvert);

string intToString(int i);

template< typename T > 
string toString(T i) // convert number to string
{
    std::stringstream s;
    s << i;
    return s.str();
}


char *UnicodeToCodePage(int codePage, const wchar_t *src);
wchar_t *CodePageToUnicode(int codePage, const char *src);

#endif