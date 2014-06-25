// load with given path to given name
program DW_use, plugin using("D:\Users\afarkas\mystuff\Arbeit\MNB\STATA\Plugin\StataDwPlugin\Debug\StataDwPlugin.dll")

// execute
plugin call DW_use

// unload 
program drop DW_use

// to switch between DLL and EXE output set Configuration Properties / General / Configuration Type in Project properties



or

// compile plugin from DLL and put it in the STATA directory
cp "D:\Users\afarkas\mystuff\Arbeit\MNB\STATA\Plugin\StataDwPlugin\Debug\StataDwPluginTest.dll" dwuse.plugin

program dwuse, plugin




DW Use Plugin usage: 
0. You can pass default values for common options, for example:
	plugin call DW_use, DEFAULTS username <user> password <pass> database <db> 
1. Call the plugin in CREATE mode to read table definition and prepare a STATA command file to create the variables: 
	plugin call DW_use, CREATE <table> 
	plugin call DW_use, CREATE [<varlist>] [if <expr>] using <table> [nulldata] [lowercase|uppercase] [label_variable [<label_variable_varlist>]] [label_values [<label_values_varlist>]] username <user> password <pass> database <db> [limit <n>] 
2. Execute the logged commands with "do dwcommands.do" to create the dataset. 
3. Call the plugin in LOAD mode to fill the dataset:
	plugin call DW_use, LOAD 





// examples of calling
plugin call DW_use, DEFAULTS database xe username vef password ***
plugin call DW_use, CREATE tenytabla lowercase limit 10

// plugin call DW_use, CREATE tenytabla lowercase limit 10 database xe username vef password ***



// In case macro variables were used instead of the dwcommands.do file 
// the followign commands could be used to create the dataset in STATA. 

set obs `obs'

local stop : word count `vars'

forvalues i = 1/`stop' {
	local var : word `i' of `vars'
	local type : word `i' of `types'
	if strpos("`type'", "str") > 0 {
		qui gen `type' `var' = ""
	}
	else {
		qui gen `type' `var' = .	
	}
}

describe

plugin call DW_use, LOAD 

list
