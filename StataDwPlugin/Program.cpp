#include "stplugin.h"
#include "dwplugin.h"
#include "dwuse.h"
#include "strutils.h"
#include <iostream>
#include <fstream>


// variables that need to be remembered between calls
DwUseOptions* defaultOptions = NULL;
DwUseQuery* query = NULL;
const string COMMAND_LOG_FILE = "dwcommands.do";
const bool WRITE_MACRO_VARIABLES = false; // use the log file

// convert string to something STATA can print
char* toStataString( string msg ) {
	//return (char*) msg.c_str();

	/*
	vector<char> charr(msg.length() + 1);
	copy(msg.begin(), msg.end(), charr.begin());
	return &*charr.begin();
	*/

	// it seems the above calls lose their reference to the underlying string by the time 
	// STATA can print the value, thus it only prints garbage. with this we should call delete[] charr;
	char* charr = new char[msg.length()+1];
	strcpy(charr, msg.c_str());
	return charr;
}


// convert a string for STATA, print it, then free the array
void stataDisplay( string msg ) {
	char* charr = toStataString(msg);
	SF_display(charr);
	delete[] charr;
}


class CommandPrinter {
public: 
	CommandPrinter(DwUseOptions* opts) : options(opts) {
		// the file is going to be created in the Stata directory
		//this->commandlog.open(COMMAND_LOG_FILE, ios::trunc | ios::binary);
		this->commandlog = fopen(COMMAND_LOG_FILE.c_str(), "w");
	}
	// close file at the end
	~CommandPrinter(void) {
		//this->commandlog.close();
		fclose(this->commandlog);
	}
	// called with one row at a time
    void operator()( string cmd ) { 
		if( this->options->IsLogCommands() ) {
			//stataDisplay(cmd);
			//stataDisplay("\n");
			// this->commandlog << cmd << endl;
			// Now convert utf-8 back to ANSI:
			wchar_t *wText = CodePageToUnicode(65001,cmd.c_str());
			char *ansiText = UnicodeToCodePage(1252,wText);
			fprintf(this->commandlog,"%s\n",ansiText);
			delete [] ansiText;
			delete [] wText;
		}
    } 
private:
	DwUseOptions* options;
	//ofstream commandlog;
	FILE* commandlog;
};



// following functions are called by stata_call function down below
// start STATA with a start.bat file that adds the path to the Oracle client DLL-s first

// use the DLL as follows: 
// load with given path to given name
// program DW_use, plugin using("D:\Users\afarkas\mystuff\Arbeit\LearningCPlusPlus\DWH\DwUse\Debug\StataDwPlugin.dll")
// execute
// plugin call DW_use
// unload 
// program drop DW_use

// store common options (username, database) in a pointer
int setDefaultOptions( vector<string> args ) {	
	if( defaultOptions != NULL ) {
		delete defaultOptions; // previous
		defaultOptions = NULL;
	}
	try {
		// parse the new options and store them for future use
		DwUseOptionParser* parser = new DwUseOptionParser();			
		defaultOptions = parser->Parse( args );
		delete parser;
	}
	// show errors
	catch( exception ex ) {
		string msg = string(ex.what());
		stataDisplay( "Error: "+msg+"\n" );
	}
	// don't lett it bubble up to STATA because it crashes
	catch( ... ) {
		stataDisplay("An unexcpected error occured.");
	}
	return 0;
}

// read table definition, rowcount, labels from the database
int createDataSet( vector<string> args ) {
	try {
		// parse the options		
		DwUseOptionParser* parser = new DwUseOptionParser();
		DwUseOptions* options = parser->Parse( args );
		delete parser;

		// print commands to a log file
		CommandPrinter printCommand(options);

		// merge the parsed options with global defaults
		if( defaultOptions != NULL )
			options->AddDefaults(defaultOptions);

		// print raw options
		SF_display( "Options: \n" );
		for( map<string,string>::const_iterator ii = options->Options().begin(); ii != options->Options().end(); ++ii ) {
			string opt = "   " + (*ii).first + string(": ") + (*ii).second + "\n";
			stataDisplay(opt);
		}

		// some error checking
		if(options->Database() == "" || options->Username() == "" || options->Password() == "") {
			throw DwUseException( "Database credentials are missing!" ); 
		}
		if(options->Table().find(" ") != string::npos || options->Table() == "") {
			throw DwUseException( "Could not parse the table name (found \"" + options->Table() 
									+ "\"). Are you missing a using keyword or mis-typed the one after the table name?" ); 
		}
		// check non-sense
		options->Validate();

		// http://www.stata.com/support/faqs/data-management/using-plugin-to-connect-to-database/
		// http://www.stata.com/support/faqs/data/connect_to_mysql.c
		string stata_vars    = "";
		string stata_types   = "";
		string stata_formats = "";
		string stata_obs     = "";

		// if there is anything left from a previous CREATE call, drop it
		if( query != NULL ) {
			delete query;
			query = NULL;
		}
		// if there is anything wrong the query will raise exceptions
		query = new DwUseQuery(options); // will free options on its own

		// the NULLDATA option means we just want to put labels on an existing dataset
		bool printDataCommands = !options->IsNullData();
		bool printLabelCommands = true;

		// will print STATA commands that can be executed later
		printCommand("* use the following commands to create the " +options->Table()+ " dataset in Stata: ");
		printCommand("");

		// count the rows, next time we shall run the query as well
		stata_obs = toString(query->RowCount());
		if( printDataCommands ) {
			printCommand("set obs " + stata_obs);
			printCommand("");
		}

		// display labels
		for( vector<DwColumn*>::const_iterator ii = query->Columns().begin(); ii != query->Columns().end(); ii++ ) {
			stata_vars    += (*ii)->VariableName() + " "; // TODO: spaces in labels will not work with the default mysql style macro
			stata_types   += (*ii)->StataDataType() + " ";
			stata_formats += (*ii)->StataFormat() + " ";

			// STATA variable creation command with formatting
			if( printDataCommands ) {
				string cmd = "qui gen "+(*ii)->StataDataType()+" "+(*ii)->VariableName() + " = ";
				if( (*ii)->IsNumeric() ) {
					cmd += ".";
				} else {
					cmd += "\"\"";
				}			
				printCommand(cmd);
				cmd = "format "+(*ii)->VariableName()+" "+(*ii)->StataFormat();
				printCommand(cmd);
			}

			// STATA commands to label variables
			if( printLabelCommands ) {
				if( (*ii)->IsLabelVariable() ) {
					// label variable REPRKOD6 "Almakompot"
					string labelVar = "label variable " 
						+ (*ii)->VariableName() + " \"" + (*ii)->ColumnLabel() + "\" ";
					printCommand(labelVar);
				}
				// instead of translating the column contents, print commands they can run to let STATA label them
				if( (*ii)->IsLabelValues() ) {
					// label define honap_label 1 "Január" 2 "Február"
					// label values HONAP honap_label
					string labelDef = "label define " + (*ii)->VariableName() + "_label ";
					for( map<string,string>::const_iterator iil = (*ii)->ValueLabels().begin(); iil != (*ii)->ValueLabels().end(); ++iil ) {
						labelDef +=  (*iil).first + " \"" + (*iil).second + "\" ";
					}			
					string labelVals = "label values " + (*ii)->VariableName() + " " + (*ii)->VariableName() + "_label";
					// Stata doesn't let us label string, but for debug we can print them in comments (otherwise they stop processing)
					string toggle = (*ii)->IsNumeric() ? "" : "* "; 
					printCommand(toggle+labelDef);
					printCommand(toggle+labelVals);
				}	
				printCommand("");
			}			
		}

		// tell the user where to look for the .do file
		if( options->IsLogCommands() ) {
			stataDisplay("Saved commands needed to create the dataset into the file \""+COMMAND_LOG_FILE+"\" in the Stata directory. \n");
		}

		if( WRITE_MACRO_VARIABLES ) {
			// Store variable names/types and observation number into Stata macro
			SF_macro_save("_vars",    toStataString(stata_vars));
			SF_macro_save("_types",   toStataString(stata_types));
			SF_macro_save("_formats", toStataString(stata_formats));
			SF_macro_save("_obs",     toStataString(stata_obs));

			// print out for the users information
			stataDisplay("Saved data size ("+stata_obs+" rows), column names, types ("+toString(query->Columns().size())+" cols) and suggested formats into marco variables called _obs, _vars, _types and _formats. \n");
		}
	} 
	// show errors
	catch( DwUseException ex ) { // for some reason catching the base exception class doesn't work while in STATA :(
		string msg = ex.what();
		stataDisplay( "Error: "+string(msg)+"\n" );
	}
	// don't lett it bubble up to STATA because it crashes
	catch( ... ) {
		stataDisplay("An unexcpected error occured.");
	}
	return 0;
}


// class to fill STATA
class FillDataSet {
public: 
	// created with the columns that need to be filled
	FillDataSet(const vector<DwColumn*>& cols) : columns(cols) {
		row = 0;
	}
	// called with one row at a time
    void operator()( oracle::occi::ResultSet* rs ) 
    { 
		row++; // from 1
		for(size_t i=0; i < columns.size(); i++) {
			if( !columns[i]->IsNull(rs) ) {
				// STATA has separate storing functions for numbers and strings
				// the dataset has to be created with the appropriate number of 
				// columns and rows in a STATA macro before load is called
				if(columns[i]->IsNumeric()) {
					double val = columns[i]->AsNumber(rs);
					// this did not work with SD_SAFEMODE enabled in stplugin.h
					SF_vstore(i+1, row, val);
				} else {
					string val = columns[i]->AsString(rs);
					SF_sstore(i+1, row, toStataString(val));
				}
			}
		}
    } 
private:
	int row;
	const vector<DwColumn*>& columns;
};


// fill the previously opened data set into STATA
int loadDataSet() {
	if( query != NULL ) {
		// query and fill
		try {
			// prepare the filler that will load a row into STATA
			FillDataSet fds(query->Columns());
			try {
				// run the query and pass the filler
				query->QueryData(fds);
			} catch( SQLException ex ) {
				throw DwUseException( "Error querying data with \n" 
										+ query->QuerySQL()+ ": \n" + ex.getMessage() ); 
			}
		}
		// show errors
		catch( DwUseException ex ) { // for some reason catching the base exception class doesn't work while in STATA :(
			stataDisplay( "Error: "+string(ex.what())+"\n" );
		}
		// don't lett it bubble up to STATA because it crashes
		catch( ... ) {
			stataDisplay("An unexcpected error occured.");
		}
	} else {
		stataDisplay("First you have to CREATE a dataset. \n");
	}
	return 0;
}


// Entry point of STATA plugin
STDLL stata_call(int argc, char *argv[])
{
	// the first word determines what we want to do
	if(argc == 0) {
		// print usage
		SF_display("DW Use Plugin usage: \n") ;
		SF_display("0. You can pass default values for common options, for example:\n");
		SF_display("	plugin call DW_use, DEFAULTS username <user> password <pass> database <db> \n") ;
		SF_display("1. Call the plugin in CREATE mode to read table definition and prepare a STATA command file to create the variables: \n");
		SF_display("	plugin call DW_use, CREATE <table> \n") ;
		SF_display("	plugin call DW_use, CREATE [<varlist>] [if <expr>] using <table> [nulldata] [lowercase|uppercase] [label_variable [<label_variable_varlist>]] [label_values [<label_values_varlist>]] username <user> password <pass> database <db> [limit <n>] \n") ;
		SF_display("2. Execute the logged commands with \"do dwcommands.do\". \n");
		SF_display("3. Call the plugin in LOAD mode to fill the dataset: \n");
		SF_display("	plugin call DW_use, LOAD \n") ;
	} else {
		// parse the options
		string mode = upperCase(argv[0]);
		// transform the rest of the options to a list of strings
		vector<string> args;
		for(int i=1; i<argc; i++) {
			string arg = argv[i];
			arg = replaceAll(arg, "`", "\"");
			// if we use a filter condition with parantheses it will be one word including the "if"
			if( lowerCase(arg.substr(0,3)) == "if " ) {
				args.push_back("if");
				args.push_back(arg.substr(3));
			} else {
				args.push_back( arg );
			}
		}

		if( mode == "DEFAULTS" ) {
			return setDefaultOptions(args);
		} else if (mode == "CREATE") {
			return createDataSet(args);
		} else if (mode == "LOAD") {
			return loadDataSet();
		} else {
			stataDisplay("Unknown mode " + mode + ". Use DEFAULTS, CREATE or LOAD! \n");
		}
	} 
    return 0;
}




// below is for .exe console testing


// process the query data and print it on the console
class PrintQuery {
public: 
	PrintQuery(const vector<DwColumn*>& cols) : columns(cols) {
	}
    void operator()( oracle::occi::ResultSet* rs ) 
    { 
		for(size_t i=0; i < columns.size(); i++) {
			if(i > 0) 
				cout << ", ";
			if(columns[i]->IsNumeric()) {
				cout << columns[i]->AsNumber(rs);
			} else {
				cout << "\"" << columns[i]->AsString(rs) << "\"";
			}
		}
		cout << endl;
    } 
private:
	const vector<DwColumn*>& columns;
};


// testing in command line
// to start here change the configuration type to .exe but also the Linker / System / SubSystem to CONSOLE from WINDOWS 
int main(int argc, char *argv[])
{

	if(argc > 1) {  // not just exe
		// parse the options
		DwUseOptions* options = NULL;
		DwUseQuery* query = NULL;
		try 
		{
			// transform options to a list of strings
			vector<string> args;
			for(int i=1; i<argc; i++) { // first is the name of the .exe
				string arg = string(argv[i]);
				// some column names require " but that is not passed as it is part of command line syntax for multi word arguments. 
				// as a workaround treate ` (AltGr+7) as "
				// still, < in if expressions won't work unless put in ""
				arg = replaceAll(arg, "`", "\"");
				args.push_back( arg );
			}

			// parse the options		
			DwUseOptionParser* parser = new DwUseOptionParser();
			// don't use global options here, this is just the exe test
			options = parser->Parse( args );
			delete parser;

			// print raw options
			for( map<string,string>::const_iterator ii = options->Options().begin(); ii != options->Options().end(); ++ii ) {
				string opt = ii->first + string(": ") + (*ii).second;
				cout << opt << endl;
			}
			// print some of the parsed options
			vector<string> vars = options->Variables();
			cout << endl;
			cout << "Variables: ";
			for( vector<string>::const_iterator ii = vars.begin(); ii != vars.end(); ii++ ) {
				cout << (*ii) << ", ";
			}
			cout << endl;
			cout << "Casing: " << options->VariableCasing() << endl;
			cout << "Table: " << options->Table() << endl;
			cout << "Where: " << options->WhereSQL() << endl;

			// some error checking
			if(options->Database() == "" || options->Username() == "" || options->Password() == "") {
				throw DwUseException( "Database credentials are missing!" ); 
			}
			if(options->Table().find(" ") != string::npos) {
				throw DwUseException( "Could not parse the table name (found \"" + options->Table() 
										+ "\"). Are you missing a 'using' keyword or mis-typed the one after the table name?" ); 
			}

			// use the database		
			query = new DwUseQuery(options);
		
			//display labels
			cout << "Variable labels and types: " << endl;
			for( vector<DwColumn*>::const_iterator ii = query->Columns().begin(); ii != query->Columns().end(); ii++ ) {
				cout << (*ii)->ColumnName() << " (" << (*ii)->VariableName() << "): " << (*ii)->StataDataType() << endl;
			}

			// compile SQL
			cout << "Query: " << query->QuerySQL() << endl;
			cout << "Row count: " << query->RowCount() << endl;
			cout << "Data: " << endl;

			// run the query results
			PrintQuery pq(query->Columns());
			try {
				query->QueryData(pq);
			} catch( SQLException ex ) {
				throw DwUseException( "Error querying data with \n" 
										+ query->QuerySQL()+ ": \n" + ex.getMessage() ); 
			}
		}
		// show errors
		catch( DwUseException ex ) {
			cout << ex.what() << endl;
		}
		catch( exception ex ) {
			cout << ex.what() << endl;
		}
		// not in plugin, so delete
		if( query != NULL )		delete query; // deletes options too
		else					delete options;
	} else {
		cout << "example usage: " << endl;
		cout << "nem `Szuletesi_ido` if szam_002 \"<=\" 300 | szoveg == 'barack' using tenytabla label_variable label_values nem database xe username usr password *** limit 10" << endl;
	}

	std::puts("Press any key to continue...");
	std::getchar();
	return 0;
}
