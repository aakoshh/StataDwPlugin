#pragma once // VC++

#ifndef DWPLUGIN_H
#define DWPLUGIN_H

#define DllImport   __declspec( dllimport )
#define DllExport   __declspec( dllexport )

#include <map>
#include <set>
#include <vector>
#include <iostream>
#include "dwuse.h" 


using namespace std;


// given a set of expected keywords parse a list of words and return a dictionary
// containing each keyword and the concatenated list of options that were following it
// OR an empty string if it was just a flag
class OptionParser
{
public:
	OptionParser(set<string> keys);
	map<string,string> Parse(vector<string> words);
private:
	set<string> keys; // set of expected keys
};


class DwUseException : exception {
  public:
    DwUseException(string message) : msg(message) {}
    const char* what() const {return this->msg.c_str();}

  private:
    string msg;
};


enum VariableCasing { ORIGINAL, UPPERCASE, LOWERCASE };


class DwUseOptions
{
public: 
	// constructor uses the options parsed
	DwUseOptions(map<string,string> options);	
	void Validate();
	// login credentials
	string Username();
	string Password();
	string Database();
	// list of columns to fetch from the DB. fetch all from DB if this is empty
	vector<string> Variables();
	// the SQL filter to apply on the table
	string WhereSQL();
	// database table to query
	string Table();
	// TODO: I don't understand what this should do
	bool IsNullData();
	// practical way to limit rows for debugging
	int Limit();
	// Upper, Lower or the original casing of variables
	VariableCasing VariableCasing();
	// use the logical name of variables or their textual labels
	bool IsLabelVariables();
	// which variables to label if any. if empty and IsLabelVariables is true then all of them
	set<string> LabelVariables();
	// use the original value of the variable content or rename it to labels
	bool IsLabelValues();
	// which variable values to label. if empty and IsLabelValues is true then all of them
	set<string> LabelValues();

	// the option to print STATA commands to a .do file
	bool IsLogCommands();

	// the original options for debugging
	const map<string,string>& Options();
	void AddDefaults(DwUseOptions* defaults);
private :
	map<string,string> options;
	bool HasOption(string name);
	string GetOption(string name);
	vector<string> GetOptionAsList(string name, bool toUpperCase=false);
	void ThrowIfHasValue(string name);
};

// turn a list of command line arguments into a DwUseOption object we can use

class DwUseOptionParser
{
public:
	// plugin call DW_use, <table> username <user> password <pass> database <db>
	// plugin call DW_use, [<varlist>] [if <expr>] using <table> [nulldata] [lowercase|uppercase]
	//						[label_variable [<label_variable_varlist>]] [label_values [<label_values_varlist>]]
	//						username <user> password <pass> database <db>
	DwUseOptions* Parse(vector<string> words);
};


// translate labels
class Translator {
  public:
	  virtual string Translate(string msg) = 0;
	  virtual bool HasTranslation(string msg) = 0;
	  virtual const map<string,string>& Mapping() = 0;
};


// hold the processing instructions for a given column
class DwColumn {
public :
	DwColumn( DbColumnMetaData metaData, 
			  int position,
			  VariableCasing variableCasing,
			  Translator* variableTranslator, 
			  Translator* valueTranslator );
	// free pointers
	~DwColumn(void);
	// the column name in the database
	string ColumnName();
	// the column label in STATA
	string ColumnLabel();
	// the final variable name that will appear in STATA
	string VariableName();
	// indicate whether the variable name can be labeled (translated)
	bool IsLabelVariable();
	// indicate whether the values in the column can be labeled (translated)
	bool IsLabelValues();
	// if values are labeled, return the mapping for STATA label generation
	const map<string,string>& ValueLabels();
	// the appropriate STATA datatype
	string StataDataType();
	// the format mask to show friendly values for dates
	string StataFormat();
	// STATA can store either double or string
	// I will use rs->getDouble for numeric and rs->getString for the rest
	bool IsNumeric();
	// if there is no data we must not give STATA anything
	bool IsNull(ResultSet* rs);
	// retrieve the column value from a record as number
	double AsNumber(ResultSet* rs);
	// retrieve the column value from a record as a (translated) string
	string AsString(ResultSet* rs);
private :
	DbColumnMetaData metaData; // to access name, type
	int position; // which column is it
	VariableCasing variableCasing;
	Translator* variableTranslator; // what to put into variable name
	Translator* valueTranslator; // what to put into column values
	// Method which prints the data type http://docs.oracle.com/cd/B10500_01/appdev.920/a96583/cciaadem.htm
	string printType (int type);
	// speed up type checking
	bool isNumeric; 
	bool isDate;
	bool isTime;
	bool translateContents;
};


// process the options, connect to the DWH, create translators, perform the query
class DwUseQuery
{
public:
	DwUseQuery(DwUseOptions* options);
	~DwUseQuery(void);
	// compile the SQL statement
	string QuerySQL();
	// run a count on the query to know how big STATA dataset to create
	int RowCount();
	// provide access to column definitions for creation of macro variables
	const vector<DwColumn*>& Columns();
	// accept a result set processor that fills data into STATA
	template< typename F > 
	void QueryData(F processor);
private:
	DwUseOptions* options;
	DbConnect* conn;
	Translator* variableTranslator;
	vector<DwColumn*> columns;
};


// template function so needs to be in the header
template< typename F > 
void DwUseQuery::QueryData(F processor) {
	string sql = this->QuerySQL();
	vector<string> params;
	this->conn->Select(processor, sql, params);
};

#endif