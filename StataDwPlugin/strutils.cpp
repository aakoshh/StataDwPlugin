#include <algorithm>
#include <string>
#include <vector>
#include <sstream>

using namespace std;

// split string
vector<string> &split(const std::string &s, char delim, vector<string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while(getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
};

vector<string> split(const string &s, char delim) {
    vector<string> elems;
    return split(s, delim, elems);
};

// string replace
string replaceAll(string str, const string& from, const string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
	return str;
};


string upperCase(string strToConvert)
{
    std::transform(strToConvert.begin(), strToConvert.end(), strToConvert.begin(), ::toupper);
    return strToConvert;
};

string lowerCase(string strToConvert)
{
    std::transform(strToConvert.begin(), strToConvert.end(), strToConvert.begin(), ::tolower);
    return strToConvert;
};


