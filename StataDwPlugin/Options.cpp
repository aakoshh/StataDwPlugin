#include "dwplugin.h"
#include "strutils.h"
#include <algorithm> 

bool ALWAYS_LOG_COMMANDS = true;

OptionParser::OptionParser(set<string> keys) {
	// keys are assumed to be lowercase and casing will be ignored
	this->keys = keys;
}

map<string,string> OptionParser::Parse(vector<string> words) {	
	// return a dictionary collecting the various parts of the commands according to the keys we got in the constructor
	map<string,string> options;	
	// parse words into dictionary: if the current word is not a keyword add it to the already parsed part, if it is, add a new entry	
	string currentKey;
	for( size_t i=0; i < words.size(); i++ ) {
		// if the current word is a keyword, use it from now
		bool isKeyword = this->keys.find(lowerCase(words[i])) != this->keys.end();
		if( isKeyword ) {
			currentKey = lowerCase(words[i]);
			// take note in the map that the option was there, so it is at least a flag
			options[currentKey] = ""; // it would have returned this anyway if looked up this way, so later we must check with find
		} else { // a value must be
			if( currentKey == "" ) // the words have to begin with a keyword 
				throw DwUseException("Cannot decide what option '" + words[i] + "' belongs to.");
			string opt = options[currentKey];
			// insert a space between parts, but until the next option it will be one string
			options[currentKey] = (opt == "" ? opt : opt + " ") + words[i];
		}
	}
	return options;
}


bool hasKeyword(vector<string> words, string word) {
	for( size_t i=0; i < words.size(); i++ ) {
		if( lowerCase(words[i]) == lowerCase(word) ) {
			return true;
		}
	}
	return false;
}

// parse a STATA command 
DwUseOptions* DwUseOptionParser::Parse(vector<string> words) {	

	// there are 10 keywords we expect to see
	string keys[] = {"variables", "if", "using", "limit",
					 "nulldata", "lowercase", "uppercase", 
					 "label_variable", "label_values", 
					 "username", "password", "database"};
	size_t nkeys(sizeof(keys) / sizeof(string));
	// create parser that accepts these keywords
	OptionParser* parser = new OptionParser( set<string>(keys, keys + nkeys) );

	// prepare another vector where we can search 

	// see if we have a using anywhere, if we do, the first part is the varlist, if not it is the tablename
	bool hasUsing = hasKeyword(words, "using");

	// if it contains using somewhere we can start with variables (if missing it will be an empty string, if the list is there it will be expected)
	if( !hasUsing ) 
		words.insert(words.begin(), "using"); 
	else if( lowerCase(words[0]) != "if" && lowerCase(words[0]) != "using" )
		words.insert(words.begin(), "variables");

	// parse the options
	map<string,string> options = parser->Parse( words );
	delete parser;

	// create a meaningful options object
	DwUseOptions* useOptions = new DwUseOptions( options );	
	return useOptions;
}


void DwUseOptions::ThrowIfHasValue(string name) {
	if( HasOption(name) && GetOption(name) != "" )
		throw DwUseException( "Invalid value for '"+name+"': " + GetOption(name) ); 
}

// check that by mistake the next word was not mistyped and thus became part of this option
void DwUseOptions::Validate() {
	ThrowIfHasValue("lowercase");
	ThrowIfHasValue("uppercase");
	ThrowIfHasValue("nulldata");
}


DwUseOptions::DwUseOptions(map<string, string> options) {
	this->options = options;
}

const map<string,string>& DwUseOptions::Options() {
	return this->options;
}

bool DwUseOptions::HasOption(string name) {
	return this->options.find(name) != this->options.end();
}

string DwUseOptions::GetOption(string name) {
	if(this->HasOption(name))
		return this->options[name]; // would add empty string
	return ""; 
}

vector<string> DwUseOptions::GetOptionAsList(string name, bool toUpperCase) {
	// return empty list if there was no variables specified
	// convert to uppercase so that we can compare it with column names
	string opt = this->GetOption(name);
	if( toUpperCase ) // for example if we need membership checks
		opt = upperCase(opt);
	return split( replaceAll(opt, ",",""), ' ');
}

string DwUseOptions::Username() {
	return this->GetOption("username"); 
}

string DwUseOptions::Password() {
	return this->GetOption("password"); 
}

string DwUseOptions::Database() {
	return this->GetOption("database"); 
}

vector<string> DwUseOptions::Variables() {
	// return empty list if there was no variables specified
	return this->GetOptionAsList("variables");
}

string DwUseOptions::Table() {
	return this->GetOption("using"); 
}

// parse the if expression and translate it to SQL
string DwUseOptions::WhereSQL() {
	string filter = this->GetOption("if");
	filter = replaceAll(filter, "|", " or ");
	filter = replaceAll(filter, "&", " and ");
	filter = replaceAll(filter, "==", "=");
	//filter = replaceAll(filter, "\"", "'"); // command lines don't pass ", but some variables can contain them in their names
	return filter;
}

bool DwUseOptions::IsNullData() {
	return this->HasOption("nulldata");
}

VariableCasing DwUseOptions::VariableCasing() {
	if( this->HasOption("uppercase") ) return UPPERCASE;
	if( this->HasOption("lowercase") ) return LOWERCASE;
	return ORIGINAL;
}

bool DwUseOptions::IsLabelVariables() {
	// if the word was present, it might have been followed by a list of variables
	return this->HasOption("label_variable"); 
}

set<string> DwUseOptions::LabelVariables() {
	// return empty list if there was no variables specified, which combined with a True means all should be labelled
	vector<string> lst = this->GetOptionAsList("label_variable", true);
	return set<string>(lst.begin(), lst.end());
}

bool DwUseOptions::IsLabelValues() {
	return this->HasOption("label_values"); 
}

set<string> DwUseOptions::LabelValues() {
	vector<string> lst = this->GetOptionAsList("label_values", true);
	return set<string>(lst.begin(), lst.end());
}

int DwUseOptions::Limit() {
	if(!this->HasOption("limit")) 
		return 0;
	string limit = this->GetOption("limit");
	return atoi(limit.c_str());
}

// for basic data and formatting we can use the macro variables but for labeling we can't
bool DwUseOptions::IsLogCommands() {
	return ALWAYS_LOG_COMMANDS 
		|| this->HasOption("log_commands") 
		|| this->IsLabelValues() 
		|| this->IsLabelValues(); 
}


void DwUseOptions::AddDefaults(DwUseOptions* defaults) {

	for( map<string,string>::const_iterator ii = defaults->Options().begin(); ii != defaults->Options().end(); ++ii ) {
		string key = (*ii).first; 
		string value = (*ii).second;
		if( key == "uppercase" || key == "lowercase" ) {
			// only set this if the current setting is neutral
			if( this->VariableCasing() == ORIGINAL ) 
				this->options[key] = value;
		} else if (value != "" || !this->HasOption(key)) {
			// other keys should be non-conflicting, but don't overwrite to empty if it exists
			this->options[key] = value;
		}
	}
}