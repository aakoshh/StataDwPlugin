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



// examples of calling
plugin call DW_use, DEFAULTS database xe username vef password vef
plugin call DW_use, CREATE tenytabla lowercase limit 10

// plugin call DW_use, CREATE tenytabla lowercase limit 10 database xe username vef password vef

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