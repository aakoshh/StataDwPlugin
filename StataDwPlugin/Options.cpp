#include "dwplugin.h"
#include "strutils.h"
#include <algorithm> 



OptionParser::OptionParser(set<string> keys) {
	this->keys = keys;
}

map<string,string> OptionParser::Parse(vector<string> words) {	
	// return a dictionary collecting the various parts of the commands according to the keys we got in the constructor
	map<string,string> options;	
	// parse words into dictionary: if the current word is not a keyword add it to the already parsed part, if it is, add a new entry	
	string currentKey;
	for( size_t i=0; i < words.size(); i++ ) {
		// if the current word is a keyword, use it from now
		bool isKeyword = this->keys.find(words[i]) != this->keys.end();
		if( isKeyword ) {
			currentKey = words[i];
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

	// see if we have a using anywhere, if we do, the first part is the varlist, if not it is the tablename
	bool hasUsing = std::find(words.begin(), words.end(), "using") != words.end();

	// if it contains using somewhere we can start with variables (if missing it will be an empty string, if the list is there it will be expected)
	if( !hasUsing ) 
		words.insert(words.begin(), "using"); 
	else if( words[0] != "if" && words[0] != "using" )
		words.insert(words.begin(), "variables");

	// parse the options
	map<string,string> options = parser->Parse( words );
	delete parser;
	
	// create a meaningful options object
	DwUseOptions* useOptions = new DwUseOptions( options );	
	return useOptions;
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
	return this->HasOption("log_commands") 
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