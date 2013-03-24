#include "dwplugin.h"
#include "strutils.h" 


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
				       " else 'Időben változó értelmezés' "
				  " end megnevezes "
			"from (	 select VALTOZO, "
			              " max(MEGNEVEZES) max_megnevezes, "
			              " count(distinct MEGNEVEZES) count_distinct_megnevezes"
			       " from DIMN.DIM_VALTOZO_CIMKEK "
				   " where TENYTABLA = :p_table and STATUSZ = :p_statusz"
			       " group by VALTOZO) ";
		// select all and fill up mapping
		vector<string> params;
		params.push_back(upperCase(table));
		params.push_back("I");
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
				  " where TENYTABLA = :p_table and VALTOZO = :p_column and STATUSZ = :p_statusz"
			      " group by KOD) ";
		// select all and fill up mapping
		vector<string> params;
		params.push_back(upperCase(table));
		params.push_back(upperCase(column));
		params.push_back("I");
		try {
			conn->Select(this->Adapter(), sql, params);
		} catch( SQLException ex ) {
			throw DwUseException( "Error querying variable labels for " 
									+ table + "." + column + " with \n"
									+ sql +": \n" + ex.getMessage() ); 
		}
	}
};



// class representing what we know about a column in the query
DwColumn::DwColumn(	  DbColumnMetaData metaData, 
					  int position,
					  VariableCasing variableCasing,
					  Translator* variableTranslator, 
					  Translator* valueTranslator ) {
	this->metaData = metaData;
	this->position = position;
	this->variableCasing = variableCasing;
	this->variableTranslator = variableTranslator;
	// pass NULL if we don't want any translation
	this->valueTranslator = valueTranslator;
	// whether the column is numeric will be checked for each row
	string stype = this->StataDataType();
	this->isNumeric = stype.substr(0,3) != "str"; // STATA only has string and double macro setters
	this->isDate = this->metaData.type == "DATE"; // will be numeric in STATA but cannot get as double
	this->isTime = this->metaData.type == "TIMESTAMP";
}

// free pointers
DwColumn::~DwColumn(void) {
	// value translator was created and passed just for this instance, so free it
	if(this->valueTranslator) {
		delete this->valueTranslator;
		this->valueTranslator = NULL;
	}
}

// the column name in the database that can be used in SQL
string DwColumn::ColumnName() {
	if( this->metaData.isQuoted ) // in the test tables there are columns like "Szuletesi_ido" which only work with double quotes and exact casing
		return "\"" + this->metaData.name + "\"";
	return this->metaData.name;
}

// the column label appear in STATA
string DwColumn::ColumnLabel() {
	// the label
	string label = NULL;
	if( this->variableTranslator != NULL ) {
		label = this->variableTranslator->Translate(upperCase(this->metaData.name)); // VALTOZO is uppercase according to the spec.
	}
	return label;
}

// the final variable name
string DwColumn::VariableName() {
	// the logical name
	string name = this->metaData.name; // without quotes even if there should be one
	// translate if necessary
	if( this->variableTranslator != NULL ) {
		name = this->variableTranslator->Translate(upperCase(name)); // VALTOZO is uppercase according to the spec.
	}
	// until we find a better way to split names in STATA we have to make sure there is no space in the labels
	name = replaceAll(name, " ", "_");
	// change casing if needed. 
	switch(this->variableCasing) {
		case UPPERCASE: return upperCase(name); break;
		case LOWERCASE: return lowerCase(name); break;
		default: return name; break;
	}
}

// the appropriate STATA datatype
string DwColumn::StataDataType() {
	// MetaData gives the underlying data type which may be translated to string
	if( this->valueTranslator != NULL ) {
		// varchar2 or int dictionary value will be translated to string
		return "str100"; // the size of MEGNEVEZES
	}
	// no translation
	string type   = this->metaData.type;
	int size      = this->metaData.size;
	int precision = this->metaData.precision;
	int scale     = this->metaData.scale;
	// decide following the specification
	if( type == "DATE" ) return "double";
	if( type == "TIMESTAMP" ) return "double";
	if( type == "VARCHAR2" ) return "str" + toString(size);
	if( type == "NUMBER" ) {
		if( scale == 0 ) {
			if( precision <= 2 ) return "byte";
			if( precision <= 4 ) return "int";
			if( precision <= 9 ) return "long";
			return "double";
		} else {
			if( precision <= 7 ) return "float";
			return "double";
		}
	}
	if( type == "INTEGER" ) return "long";
	return "str255"; // unknown
}


// the appropriate STATA format 
// for now these are the same values as Stata assigns by default copied after using "describe"
string DwColumn::StataFormat() {
	if( this->valueTranslator != NULL ) {
		return "%100s"; 
	}
	// http://www.stata.com/help.cgi?format
	string type   = this->metaData.type;
	int size      = this->metaData.size;
	int precision = this->metaData.precision;
	int scale     = this->metaData.scale;
	// decide following the specification
	if( type == "DATE" ) return "%td";
	if( type == "TIMESTAMP" ) return "%tc";
	if( type == "VARCHAR2" ) return "%" + toString(size) + "s";
	if( type == "NUMBER" ) {
		if( scale == 0 ) {
			if( precision <= 2 ) return "%8.0g";
			if( precision <= 4 ) return "%8.0g";
			if( precision <= 9 ) return "%12.0g";
			return "%10.0g";
		} else {
			if( precision <= 7 ) return "%9."+toString(scale)+"f";
			return "%10."+toString(scale)+"f";
		}
	}
	if( type == "INTEGER" ) return "%12.0g";
	return "%255s"; // unknown
}

// STATA can store either double or string
// I will use rs->getDouble for numeric and rs->getString for the rest
bool DwColumn::IsNumeric() {
	return this->isNumeric;
}

bool DwColumn::IsNull(ResultSet* rs) {
	return rs->isNull(this->position);
}

// retrieve the column value from a record as number
double DwColumn::AsNumber(ResultSet* rs) {
	if(this->isDate) {
		// return a double but we have to create that
		oracle::occi::Date date = rs->getDate(this->position);
		// https://www.stanford.edu/group/ssds/cgi-bin/drupal/files/Guides/Working%20with%20Dates%20and%20Times%20in%20Stata.pdf
		// according to them dates are integers: days since 1960, jan 1
		// create the epoch by first making a copy of our date, because occi dates need environment
		oracle::occi::Date base(date); 
		base.setDate(1960,1,1);
		oracle::occi::IntervalDS dt = date.daysBetween(base); 
		return dt.getDay();
	} else if( this->isTime ) {
		oracle::occi::Timestamp ts = rs->getTimestamp(this->position);
		// STATA wants milliseconds since the above data
		oracle::occi::Timestamp base(ts);
		base.setDate(1960,1,1);
		base.setTime(0,0,0,0);
		oracle::occi::IntervalDS dt = ts.subDS(base);
		double seconds = ((double)dt.getDay()) * 24 * 60 * 60 +
						 ((double)dt.getHour()) * 60 * 60 + 
						 ((double)dt.getMinute()) * 60 + 
						 ((double)dt.getSecond());
		return seconds * 1000;
	}
	return rs->getDouble(this->position);
}

// retrieve the column value from a record as a (translated) string
string DwColumn::AsString(ResultSet* rs) {
	string val = rs->getString(this->position);
	if( this->valueTranslator != NULL ) {
		return this->valueTranslator->Translate(val);
	}
	return val;
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
	//bool isTransAllVars   = this->options->IsLabelVariables() && transVars.size() == 0;
	bool isTransAllVals   = this->options->IsLabelValues()    && transVals.size() == 0;
	bool isTransAllVars   = false; // nem a valtozokat kell atnevezni
	//bool isTransAllVals   = false; // nem az ertekeket kell atnevezni
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
	}
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
	else if (this->options->Limit() == 0 || this->options->IsNullData()) {
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
