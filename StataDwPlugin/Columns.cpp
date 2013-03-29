#include "dwplugin.h"
#include "strutils.h" 


// class representing what we know about a column in the query
DwColumn::DwColumn(	  DbColumnMetaData metaData, 
					  int position,
					  VariableCasing variableCasing,
					  Translator* variableTranslator, 
					  Translator* valueTranslator ) {
	this->metaData = metaData;
	this->position = position;
	this->variableCasing = variableCasing;
	this->translateContents = false; // we don't translate variable content in the dataset, we use STATA labeling
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
	string label = this->metaData.name;
	if( this->variableTranslator != NULL ) {
		label = this->variableTranslator->Translate(upperCase(label)); // VALTOZO is uppercase according to the spec.
	}
	return label;
}

// only show that we can label a column if we really have a translation for it
bool DwColumn::IsLabelVariable() {
	return this->variableTranslator != NULL 
		&& this->variableTranslator->HasTranslation(upperCase(this->metaData.name));
}

// the final variable name in the dataset
string DwColumn::VariableName() {
	// the logical name
	string name = this->metaData.name; // without quotes even if there should be one
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
	if( this->translateContents && this->valueTranslator != NULL ) {
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
	if( type == "VARCHAR2" ) {
		size = size > 244 ? 244 : size;
		return "str" + toString(size);
	}
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
	return "str244"; // unknown
}


// the appropriate STATA format 
// for now these are the same values as Stata assigns by default copied after using "describe"
string DwColumn::StataFormat() {
	if( this->translateContents && this->valueTranslator != NULL ) {
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
	if( type == "VARCHAR2" ) {
		size = size > 244 ? 244 : size;
		return "%" + toString(size) + "s";
	}
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
	return "%244s"; // unknown
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
	if( this->translateContents && this->valueTranslator != NULL ) {
		return this->valueTranslator->Translate(val);
	}
	return val;
}

// only show that we can label a variable if there are some translations as well
bool DwColumn::IsLabelValues() {
	return this->valueTranslator != NULL 
		&& this->ValueLabels().size() > 0;
		// && this->IsNumeric(); // Stata says we cannot label strings, but leave it for now for testingd
}

const map<string,string>& DwColumn::ValueLabels() {
	if( this->valueTranslator != NULL ) {
		return this->valueTranslator->Mapping();
	}
	// return empty mapping
	map<string,string> labels;
	return labels;
}
