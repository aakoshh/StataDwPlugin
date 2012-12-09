#include "dwuse.h"
#include "strutils.h"

DbConnect::DbConnect(string user, string password, string db) {
	this->user = user;
	this->password = password;
	this->db = db;
	this->env = Environment::createEnvironment(Environment::DEFAULT);
	this->conn = NULL;
	//try { 
		this->conn = this->env->createConnection(user, password, db); 
	//} 
	//catch (SQLException& ex) { 
	//	cout << ex.getMessage();
	//	exit(EXIT_FAILURE); // TODO
	//}
}


DbConnect::~DbConnect(void) {
	if( this->conn )
		this->env->terminateConnection (this->conn);
	Environment::terminateEnvironment (this->env); 
}


// http://docs.oracle.com/cd/B10500_01/appdev.920/a96583/cciaadem.htm
string printType (int type) {
	switch (type) {
		case OCCI_SQLT_CHR : return "VARCHAR2"; break;
		case OCCI_SQLT_NUM : return "NUMBER"; break;
		case OCCIINT : return "INTEGER"; break;
		case OCCIFLOAT : return "FLOAT"; break;
		case OCCI_SQLT_STR : return "STRING"; break;
		case OCCI_SQLT_VNU : return "VARNUM"; break;
		case OCCI_SQLT_LNG : return "LONG"; break;
		case OCCI_SQLT_VCS : return "VARCHAR"; break;
		case OCCI_SQLT_RID : return "ROWID"; break;
		case OCCI_SQLT_TIMESTAMP : return "TIMESTAMP"; break;
		case OCCI_SQLT_DAT : return "DATE"; break;
		case OCCI_SQLT_VBI : return "VARRAW"; break;
		case OCCI_SQLT_BIN : return "RAW"; break;
		case OCCI_SQLT_LBI : return "LONG RAW"; break;
		case OCCIUNSIGNED_INT : return "UNSIGNED INT"; break;
		case OCCI_SQLT_LVC : return "LONG VARCHAR"; break;
		case OCCI_SQLT_LVB : return "LONG VARRAW"; break;
		case OCCI_SQLT_AFC : return "CHAR"; break;
		case OCCI_SQLT_AVC : return "CHARZ"; break;
		case OCCI_SQLT_RDD : return "ROWID"; break;
		case OCCI_SQLT_NTY : return "NAMED DATA TYPE"; break;
		case OCCI_SQLT_REF : return "REF"; break;
		case OCCI_SQLT_CLOB: return "CLOB"; break;
		case OCCI_SQLT_BLOB: return "BLOB"; break;
		case OCCI_SQLT_FILE: return "BFILE"; break;
		default: 
			throw exception( "Unknown Oracle column type " + type ); // maybe timestamp is missing from the list			
	}
} // End of printType (int)

vector<DbColumnMetaData> DbConnect::Describe(string sql) {
	Statement *stmt = NULL; 
	ResultSet *rs = NULL; 
	vector<DbColumnMetaData> cols;
	stmt = this->conn->createStatement(sql); 
	if (stmt) { 
		// execute
		rs = stmt->executeQuery(); 
		vector<MetaData> meta = rs->getColumnListMetaData();
		// we must do this while it is open
		for(size_t i=0; i<meta.size(); i++) {
			// create a record with the most relevant features from  http://docs.oracle.com/cd/B10500_01/appdev.920/a96583/cciaadem.htm
			DbColumnMetaData md;
			md.name	= meta[i].getString(MetaData::ATTR_NAME);
			md.isQuoted = md.name != upperCase(md.name); // I don't know how else to look this up. quoted column name need to go into SQL with " around them
			md.type	= printType(meta[i].getInt(MetaData::ATTR_DATA_TYPE));
			md.size = meta[i].getInt(MetaData::ATTR_DATA_SIZE);
			md.precision = meta[i].getInt(MetaData::ATTR_PRECISION);
			md.scale = meta[i].getInt(MetaData::ATTR_SCALE);
			// add it to the result vector
			cols.push_back(md);
		}
		// close the statement
		this->conn->terminateStatement(stmt); 
	} 	
	return cols;
}




