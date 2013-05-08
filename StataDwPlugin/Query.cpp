#include "dwplugin.h"
#include "strutils.h" 
#include <algorithm> 


// translator where there is no need
class EchoTranslator : public Translator {
  public:
    virtual string Translate(string msg) {
		return msg;
	}
};


// fill query into map
class DictAdapter {
public :
	DictAdapter( map<string,string>& m ) : dict(m) {}
	void operator()( oracle::occi::ResultSet* rs ) 
    { 
		string key = rs->getString(1);
		string value = rs->getString(2);
		// store it
		this->dict[key] = value;
    } 
private:
	map<string,string>& dict;
};

// translate based on an mapping that comes from the database
class DwTranslator : public Translator {
public:
	// the default message must be passed in
	DwTranslator(string missingLabel) {
		this->missingLabel = missingLabel;
	}
	// translate a key  
	virtual string Translate(string msg) {
		string val = this->labels[msg];
		if( val != "" )
			return val;
		if( val == "" && msg != "" && this->missingLabel != "" ) 
			return this->missingLabel;
		return msg;
	}
	// return a dictionary containing all labels
	virtual const map<string,string>& Mapping() {
		return this->labels;
	}
	virtual bool HasTranslation(string msg) {
		return this->labels.find(msg) != this->labels.end();
	}
private:
	map<string,string> labels;
	string missingLabel;
protected:
	DictAdapter Adapter() {
		DictAdapter da(this->labels);
		return da;
	}
};


// select the labels for the columns of the database table
class VariableTranslator : public DwTranslator {
  public:
	VariableTranslator(DbConnect* conn, string table) : 
		DwTranslator("") // by default use the column name
	{
		// we need the table that contain labels for variables
		// variables will be uppercase
		string sql = 
			"select VALTOZO, " 
				  " case when count_distinct_megnevezes = 1 "
				       " then max_megnevezes "
				       " else 'Időben változó értelmezés' " // Időben változó értelmezés
				  " end megnevezes "
			"from (	 select VALTOZO, "
			              " max(MEGNEVEZES) max_megnevezes, "
			              " count(distinct MEGNEVEZES) count_distinct_megnevezes"
			       " from DIMN.DIM_VALTOZO_CIMKEK "
				   " where TENYTABLA = :p_table and STATUSZ = 'I'"
			       " group by VALTOZO) ";
		// select all and fill up mapping
		vector<string> params;
		params.push_back(upperCase(table));
		try {
			conn->Select(this->Adapter(), sql, params);
		} catch( SQLException ex ) {
			throw DwUseException( "Error querying variable labels for " 
									+ table +" with \n"
									+ sql +": \n" + ex.getMessage() ); 
		}
	}
};


// select the labels for the columns of the database table
class ValueTranslator : public DwTranslator {
  public:
	ValueTranslator(DbConnect* conn, string table, string column) : 
		DwTranslator("Nem specifikált") // save as UTF-8 without signature (BOM) 
	{
		// we need the table that contain labels for variables
		// variables will be uppercase
		string sql = 
			"select KOD, " 
			      " case when count_distinct_megnevezes = 1 "
			      " then max_megnevezes "
			      " else 'Időben változó értelmezés' "
			      " end megnevezes "
			"from (	select KOD, "
			             " max(MEGNEVEZES) max_megnevezes, "
			             " count(distinct MEGNEVEZES) count_distinct_megnevezes"
			      " from DIMN.DIM_VALTOZO_ERTEK_CIMKEK  "
				  " where TENYTABLA = :p_table and VALTOZO = :p_column and STATUSZ = 'I'"
			      " group by KOD) ";
		// select all and fill up mapping
		vector<string> params;
		params.push_back(upperCase(table));
		params.push_back(upperCase(column));
		try {
			conn->Select(this->Adapter(), sql, params);
		} catch( SQLException ex ) {
			throw DwUseException( "Error querying variable labels for " 
									+ table + "." + column + " with \n"
									+ sql +": \n" + ex.getMessage() ); 
		}
	}
};


// check that all labels selected for translation are valid column names
void CheckLabels( set<string> selection, vector<string> valid ) {
	for(set<string>::const_iterator ii = selection.begin(); ii != selection.end(); ii++) {
		if(std::find(valid.begin(), valid.end(), *ii) == valid.end())
			throw DwUseException( "Invalid column name for translation: " + (*ii) ); 
	}
}


DwUseQuery::DwUseQuery(DwUseOptions* options) {
	this->options = options;
	// create a database connection
	try {
		this->conn = new DbConnect( options->Username(),
									options->Password(),
									options->Database() );
	} catch( SQLException ex ) {
		throw DwUseException( "Error connecting to the database with " 
								+ options->Username() +"/" + options->Password() + "@" + options->Database() +": \n"
								+ ex.getMessage() ); 
	}
	// create variable translator for column
	this->variableTranslator = new VariableTranslator(this->conn, this->options->Table());
	// collect final list of variables
	// if there are none in the options, read all from the database
	// else read only the ones in the list
	string probeSql = "select ";
	// create a DwColumn for each column that we need (maybe leave out the technical cols)
	vector<string> cols = this->options->Variables();
	if( cols.size() == 0 ) {
		probeSql += "* "; // we'll check what kind of columns we see. I don't know the schema, or whether table is a view or synonym
	} else {
		for(size_t i = 0; i < cols.size(); i++) {
			if( i > 0 ) 
				probeSql += ", ";
			probeSql += cols[i];
		}
	}
	probeSql += " from " + this->options->Table() + " where 1=2 ";
	// run the probe query and collect columns
	vector<DbColumnMetaData> colMeta;
	vector<string> colNames;
	try {
		colMeta = this->conn->Describe(probeSql);
	} catch( SQLException ex ) {
		string msg = ex.getMessage();
		throw DwUseException( "Error reading column definitions with \n" 
								+ probeSql +": \n" + msg ); 
	}
	// we'll need to know what to translate
	set<string> transVars = this->options->LabelVariables();
	set<string> transVals = this->options->LabelValues();
	bool isTransAllVars   = this->options->IsLabelVariables() && transVars.size() == 0;
	bool isTransAllVals   = this->options->IsLabelValues()    && transVals.size() == 0;
	// create meta data holders
	for(size_t i=0; i<colMeta.size(); i++) {
		// get the column name so we can decide if it needs tranlation or not
		string colName = colMeta[i].name; // not qouted
		// translate or not?
		DwColumn* dwCol = new DwColumn(
			colMeta[i], 
			i+1, // position in ResultSet
			this->options->VariableCasing(),
			(isTransAllVars || transVars.find(upperCase(colName)) != transVars.end()) ?
				this->variableTranslator : NULL, 
			(isTransAllVals || transVals.find(upperCase(colName)) != transVals.end()) ?
				new ValueTranslator(this->conn, 
									this->options->Table(),
									colName) : NULL 
			);
		this->columns.push_back( dwCol );
		colNames.push_back( upperCase(colName) ); // to test translations
	}
	// check that all the variables selected for labeling are valid column names
	CheckLabels( transVars, colNames );
	CheckLabels( transVals, colNames );
};


DwUseQuery::~DwUseQuery(void) {
	if(this->options) {
		delete this->options;
		this->options = NULL;
	}
	if(this->conn) {
		delete this->conn;
		this->conn = NULL;
	}
	if(this->variableTranslator) {
		delete this->variableTranslator;
		this->variableTranslator = NULL;
	}
	// fee all meta data 
	for(size_t i=0; i<this->columns.size(); i++) {
		delete this->columns[i];
	}
}


string DwUseQuery::QuerySQL() {
	string sql = "select ";
	for(size_t i=0; i < this->columns.size(); i++) {
		if(i > 0) 
			sql += ", ";
		sql += this->columns[i]->ColumnName();
	}
	sql += " from " + this->options->Table();
	// apply filters
	string whereSql = this->options->WhereSQL();
	if( this->options->Limit() > 0 ) {
		if(whereSql != "")
			whereSql = "(" + whereSql + ") and ";
		whereSql += "rownum <= " + toString(this->options->Limit());
	}
	else if (this->options->IsNullData()) {
		if (whereSql != "") {
			whereSql = "(" + whereSql + ") and ";
		}
		whereSql += "rownum = 0";
	}
	if( whereSql != "" ) 
		sql += " where " + whereSql;
	return sql;
}


// helper class to count rows
class RowCounter {
public: 
	RowCounter(int& c) : cnt(c) {
	}
	// there will only be one row with one column
    void operator()( oracle::occi::ResultSet* rs ) 
    { 
		if (!rs->isNull(1)) {
			this->cnt = rs->getInt(1);
		} else {
			this->cnt = 0;
		}
    } 
private:
	int& cnt;
};


int DwUseQuery::RowCount() {
	string sql = "select count(1) from (" + this->QuerySQL() + ")";
	int cnt = 0;
	RowCounter rc(cnt);
	vector<string> params;
	try {
		this->conn->Select( rc, sql, params );
		return cnt;
	} catch( SQLException ex ) {
		string msg = ex.getMessage();
		throw DwUseException( "Error querying row count with \n" 
								+ sql+ ": \n" + msg ); 
	}
}

const vector<DwColumn*>& DwUseQuery::Columns() {
	return this->columns;
}
