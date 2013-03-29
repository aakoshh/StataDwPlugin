#include <algorithm>
#include <string>
#include <vector>
#include <sstream>
#include <windows.h>

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



// http://www.chilkatsoft.com/p/p_348.asp

char *UnicodeToCodePage(int codePage, const wchar_t *src) {
    if (!src) return 0;
    int srcLen = wcslen(src);
    if (!srcLen){
		char *x = new char[1];
		x[0] = '\0';
		return x;
	}
	
    int requiredSize = WideCharToMultiByte(codePage,
        0,
        src,srcLen,0,0,0,0);
	
    if (!requiredSize){
        return 0;
    }
	
    char *x = new char[requiredSize+1];
    x[requiredSize] = 0;
	
    int retval = WideCharToMultiByte(codePage,
        0,
        src,srcLen,x,requiredSize,0,0);
    if (!retval){
        delete [] x;
        return 0;
    }
    return x;
}


// 65001 is utf-8.
wchar_t *CodePageToUnicode(int codePage, const char *src) {
    if (!src) return 0;
    int srcLen = strlen(src);
    if (!srcLen){
		wchar_t *w = new wchar_t[1];
		w[0] = 0;
		return w;
	}
	
    int requiredSize = MultiByteToWideChar(codePage,
        0,
        src,srcLen,0,0);
	
    if (!requiredSize){
        return 0;
    }
	
    wchar_t *w = new wchar_t[requiredSize+1];
    w[requiredSize] = 0;
	
    int retval = MultiByteToWideChar(codePage,
        0,
        src,srcLen,w,requiredSize);
    if (!retval){
        delete [] w;
        return 0;
    }
    return w;
}


/*
	const char *text = "Sôn bôn de magnà el véder, el me fa minga mal.";
	
	// Convert ANSI (Windows-1252, i.e. CP1252) to utf-8:
	wchar_t *wText = CodePageToUnicode(1252,text);
	
	char *utf8Text = UnicodeToCodePage(65001,wText);
	
	FILE *fp = fopen("utf8File.txt","w");
	fprintf(fp,"%s\n",utf8Text);
	fclose(fp);
	
	// Now convert utf-8 back to ANSI:
	wchar_t *wText2 = CodePageToUnicode(65001,utf8Text);
	
	char *ansiText = UnicodeToCodePage(1252,wText2);
	
	fp = fopen("ansiFile.txt","w");
	fprintf(fp,"%s\n",ansiText);
	fclose(fp);
	
	delete [] ansiText;
	delete [] wText2;
	delete [] wText;
	delete [] utf8Text;
*/