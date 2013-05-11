#pragma once // VC++

#ifndef DWUSE_H
#define DWUSE_H

#include "occi.h"
#include <vector>
#include <iostream>


// http://www.tidytutorials.com/2009/08/oracle-c-occi-database-example.html
// http://oradim.blogspot.hu/2009/07/getting-started-with-occi-windows.html
// http://thisthread.blogspot.co.uk/2011/07/oracle-occi-for-visual-studio-2010.html
// https://forums.oracle.com/forums/thread.jspa?messageID=10280744
// http://www.oracle.com/technetwork/database/occidownloads-083553.html

// according to some Oracle forum Visual Studio 2010 will only work with OCCI v11, which only works with a 11g client
// so I downloaded a v11 instant client (32-bit version), OCCI and v11 SDK and linked against that
// added D:\Programs\Oracle10gXE\instantclient_11_2\oci\include\ to Project Properties / C++ / General / Additional Include Directories
// added D:\Programs\Oracle10gXE\instantclient_11_2\oci\lib\msvc\vc10\oraocci11.lib to Project Properties / Linker / Input / Additional Dependencies
// at this time I always had crashes at res->next(), so I replaced the above lib with the oraocci11d.lib, the debug version

// I renamed the oraocci11.dll to be unusable in the D:\Programs\Oracle10gXE\instantclient_11_2 because it is not built with VC++ 
// and then added a D:\Programs\Oracle10gXE\instantclient_11_2\VC10 directory with the new DLL-s downloaded with the SDK
// which should work with Visual Studio 2010 and added that to the PATH for debugging
// to debug I have to add the paths to the instant client and the oraocci11d.dll to the path in Properties / Debugging / Environment 
// as PATH=client;occi;%PATH% or exactly: PATH=D:\Programs\Oracle10gXE\instantclient_11_2;D:\Programs\Oracle10gXE\instantclient_11_2\vc10;$(LocalDebuggerEnvironment)
// to start the exe otherwise I need a start.bat like this: SET PATH=client;occi;%PATH%


using namespace std;
using namespace oracle::occi; 

// copy the needed fields from MetaData for later use
// the MetaData would become inaccessible once the ResultSet is closed
// http://docs.oracle.com/cd/B10500_01/appdev.920/a96583/cciaadem.htm
struct DbColumnMetaData { 
	string name;	
	bool isQuoted; // whether the name has to be put in double quotes in SQL
	string type; 
	int size; // for varchar2
	int precision; // for numeric
	int scale;
};


class DbConnect
{
public:
	// connect to the database
	DbConnect(string user, string password, string db);
	// disconnect
	~DbConnect(void);

	// run a select and feed the rows to the processor function
	template< typename F > 
	void Select(F processor, string sql, vector<string> params);

	// get a list of columns from a query
	vector<DbColumnMetaData> Describe(string sql);

private:
	// OCCI connection
	Environment *env; 
	Connection  *conn;

	// connection properties
	string user; 
	string password; 
	string db; // tnsnames alias
};

// include the cpp as it contains the template implementation
// #include "DbConnect.cpp"  or define it here
template< typename F > 
void DbConnect::Select(F processor, string sql, vector<string> params) {
	Statement *stmt = NULL; 
	ResultSet *rs = NULL; 
	//try { 
		stmt = this->conn->createStatement(sql); 
	//} 
	//catch (SQLException& ex) { 
	//	cout << ex.getMessage(); 
	//}
	if (stmt) { 
		//try {		  
			stmt->setPrefetchRowCount(10);
			// set parameters (for now use only strings)
			for(size_t i = 0; i < params.size(); i++) {
				stmt->setString(i+1, params[i]); // even if we bound it by name it would only look at the position
			}
			// execute
			rs = stmt->executeQuery(); 
		//} 
		//catch (SQLException& ex) { 
		//	cout << ex.getMessage(); 
		//}
		// iterate resultset and return rows
		if (rs) { 
			while (rs->next()) { 
				// call a functor object with each row (http://ubuntuforums.org/showthread.php?t=901695)
				processor( rs );
			}
			stmt->closeResultSet(rs); 
		}
		// close the statement
		this->conn->terminateStatement(stmt); 
	} 
}

#endif