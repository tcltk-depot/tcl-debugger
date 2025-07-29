# protest.tcl --
#
#	This file defines the ::protest namespace.  It is used by the
#       test harness for TclPro and finds and defines the workspace directory,
#       installation directory, tools, executables, source, etc. used by the
#       tests. See the README file for more details.
#
# Copyright (c) 1998-2000 by Ajuba Solutions
# Copyright (c) 2017 Forward Folio LLC
# See the file "license.terms" for information on usage and redistribution of this file.
# 

if {[string compare test [info procs test]] == 1} {
    lappend auto_path [info library]
    package require tcltest
    catch {namespace import ::tcltest::*} m
}

set oDir [pwd]
cd [file dirname [info script]]
set ::tcltest::testsDirectory [pwd]
cd $oDir

# create the "protest" namespace for all testing variables and procedures

namespace eval ::protest {
    # Don't want to trigger an error if this gets imported more than once
    #namespace import ::tcltest::*

    # Export the public protest procs
    namespace export findExeFile findSoFile testAllFiles resetTestsDirectory

    # TclPro is limited to a smaller platform set.  Set the
    # ::tcltest::platform variable to the platform you are currently using.
    # This variable is only used by the TclPro tests.

    set platformList [list \
	    win     win32-ix86 \
	    linux   linux-ix86 \
	    sun     solaris-sparc \
	    hp      hpux-parisc \
	    irix    irix-mips ]

    foreach {pmatch plat} $platformList {
	if {[regexp -nocase $pmatch $::tcl_platform(os)]} {
	    variable platform $plat
	}   
    }

    array set platformArray $platformList
    if {![info exists ::protest::platform]} {
	::tcltest::PrintError "\"tcl_platform(os)\" doesn't match the \
		supported platforms.  Acceptable responses are: \
		[array names platformArray]"
	exit 1
    }

    # Match defaults to all directories and skip patterns default to the empty
    # list 
    variable matchDirectories {*}
    variable skipDirectories {}

    # by default, put any interpreters this package creates into an interpreter
    # subdirectory of the temporaryDirectory; this is set in the
    # processCmdLineHooks proc since the temporaryDirectory can be redefined 
    variable interpreterDirectory {}
    
    # Default is to not specify an installation directory
    variable installationDirectory {}

    # Preset executableDirectory and sourceDirectory to {}; these variables
    # will be set to their actual values the command line arguments are
    # processed. 
    variable executableDirectory {}
    variable sourceDirectory {}

    # The workspace directory defaults to 2 levels up from the 'tests'
    # directory; since the default tests directory is different for pro than
    # for tcl, it's set in the processCmdLineArgsHook to ensure that it's been
    # reset. 
    variable workspaceDirectory {}

    # buildType defaults to Debug
    variable buildType Debug

    # Set the current Tcl, extension, and tools versions
    variable currentVersion
    array set currentVersion \
        [list \
             Tcl        $::tcl_version \
             Tcl-short  [string map {. {}} $::tcl_version] \
             Tcl-patch  $::tcl_patchLevel]
    if {[info exists ::tk_version]} {
        array set currentVersion \
            [list \
                 Tk        $::tk_version \
                 Tk-short  [string map {. {}} $::tk_version] \
                 Tk-patch  $::tk_patchLevel]
    } else {
        array set currentVersion \
            [list \
                 Tk        $currentVersion(Tcl) \
                 Tk-short  $currentVersion(Tcl-short) \
                 Tk-patch  $currentVersion(Tcl-patch)]
    }

    variable toolsDirectory ""

}

# ::tcltest::PrintUsageInfoHook --
#
#	Prints additional flag information specific to package protest
#
# Arguments:
#	none
#
proc ::tcltest::PrintUsageInfoHook {} {
    puts [format " \
	    -relateddir pattern\t Run tests in directories that match \n\
	    \t                 the glob pattern given. \n\
	    -asidefromdir pattern\t Skip tests in directories that match \n\
	    \t                 the glob pattern given."]
    return
}


proc ::tcltest::initConfigHook {} {
    # If the installation came from a CD, the -install flag must have been
    # used and a src directory must exist within the specified installation
    # directory.
    if {![string equal $::protest::installationDirectory ""] && \
	    [file exists \
	    [file join ::protest::installationDirectory set]]} {
	set ::tcltest::testConfig(installFromCD) 1
    } else {
	set ::tcltest::testConfig(installFromCD) 0
    }
    return
}

# ::tcltest::processCmdLineArgsAddFlagsHook --
#
#	Adds tclPro-specific flags to those processed by the main tcltest
#       command line processing routine.

proc ::tcltest::processCmdLineArgsAddFlagsHook {} {
    return [list -install -ws -exedir -build -srcsdir -asidefromdir \
	    -relateddir -toolsdir]
}

# ::tcltest::processCmdLineArgsHook --
#
#	Use the command line arguments provided by the
#       processCmdLineArgsAddFlagsHook to set the installationDirectory,
#       workspaceDirectory, executableDirectory, sourceDirectory, and buildType.
#
# Arguments:
#	flagArray        flags provided to ::tcltest::processCmdLineArgs
#
# Results:
#	Sets the above-named variables in the ::protest namespace.

proc ::tcltest::processCmdLineArgsHook {flagArray} {
    global tcl_platform env

    array set flag $flagArray

    # Handle -relateddir and -asidefromdir flags
    if {[info exists flag(-relateddir)]} {
	set ::protest::matchDirectories $flag(-relateddir)
    }
    if {[info exists flag(-asidefromdir)]} {
	set ::protest::skipDirectories $flag(-asidefromdir)
    }

    # Set the ::protest::installationDirectory the arg of -install, if
    # given; otherwise "". 
    #
    # If the path is relative, make it absolute.  If the file is not an
    # existing dir, then return an error.

    if {[info exists flag(-install)]} {
	set ::protest::installationDirectory $flag(-install)
	if {![file isdir $::protest::installationDirectory]} {
	    ::tcltest::PrintError "bad argument \
		    \"$::protest::installationDirectory\" to -install: \
		    \"$::protest::installationDirectory\" is not an \
		    existing directory" 
	    exit 1
	}
	if {[string equal \
		[file pathtype $::protest::installationDirectory] \
		"absolute"] == 0} {
	    set ::protest::installationDirectory \
		    [file join [pwd] $::protest::installationDirectory]
	}
    } 

    # Set the ::protest::workspaceDirectory the arg of -ws, if given.
    #
    # If the path is relative, make it absolute.  If the file is not an
    # existing dir, then return an error.

    if {[info exists flag(-ws)]} {
	set ::protest::workspaceDirectory $flag(-ws)
	if {![file isdir $::protest::workspaceDirectory]} {
	    ::tcltest::PrintError "bad argument \
		    \"$::protest::workspaceDirectory\" to -ws: \
		    \"$::protest::workspaceDirectory\" is not an existing \
		    directory" 
	    exit 1
	}
	if {[string compare \
		[file pathtype $::protest::workspaceDirectory] \
		"absolute"] != 0} { 
	    set ::protest::workspaceDirectory [file join [pwd] \
		    $::protest::workspaceDirectory] 
	}
    } else {
	set oDir [pwd]
	cd [file join [file dirname [info script]] .. ..]
	set ::protest::workspaceDirectory [pwd]
	cd $oDir
    }

    # Set the ::protest::sourceDirectory to the arg of -srcsdir, if
    # given, or <::protest::workspaceDirectory>/pro/srcs 
    # 
    # If the path is relative, make it absolute.  If the file is not an
    # existing dir, then return an error.

    if {[info exists flag(-srcsdir)]} {
	set ::protest::sourceDirectory $flag(-srcsdir)
	if {[string compare \
		[file pathtype $::protest::sourceDirectory] \
		"absolute"] != 0} { 
	    set ::protest::sourceDirectory [file join [pwd] \
		    $::protest::sourceDirectory] 
	}
	if {![file isdir $::protest::sourceDirectory]} {
	    ::tcltest::PrintError "bad argument \"$flag(-srcsdir)\" to \
		    -srcsdir: \"$::protest::sourceDirectory\" is not \
		    an existing directory" 
	    exit 1
	}
    } else {
	set oDir [pwd]
	cd $::tcltest::testsDirectory
	cd ..
	set ::protest::sourceDirectory [pwd]
	cd $oDir
    }

    # Set the ::protest::executableDirectory the arg of -exedir, if given; 
    # otherwise, if -install is specified, use
    # ::protest::installationDirectory/::protest::platform/bin 
    # else use
    # ::protest::workspaceDirectory/pro/out/<-build>/::protest::platform/bin  
    # -build arg defaults to "Debug"
    #
    # If the path is relative, make it absolute.  If the file is not an
    # existing dir, then return an error.

    if {[info exists flag(-exedir)]} {
	set ::protest::executableDirectory $flag(-exedir)
	if {![file isdir $::protest::executableDirectory]} {
	    ::tcltest::PrintError "bad argument \"$flag(-exedir)\" to \
		    -exedir: \"$::protest::executableDirectory\" is \
		    not an existing directory" 
	    exit 1
	}
    } else {
	set ::protest::executableDirectory [file dirname \
		[info nameofexecutable]]
    }

    if {[string compare \
	    [file pathtype $::protest::executableDirectory] \
	    "absolute"] != 0} { 
	set ::protest::executableDirectory [file join [pwd] \
		$::protest::executableDirectory] 
    }

    # Set the ::protest::toolsDirectory to 
    #   //pop/tools/<currentVersion(Tools)>/<::protest::platform>/<bin>  
    # or
    #   /tools/<::protest::currentVersion(Tools)>/<::protest::platform>/<bin> 
    # depending on whether ::protest::platform is windows of unix

    if {[info exists flag(-toolsdir)]} {
	if {[file isdirectory $flag(-toolsdir)]} {
	    set ::protest::toolsDirectory [file join \
		    $flag(-toolsdir) $::protest::currentVersion(Tools) \
		    ${::protest::platform} bin]
	} else {
	    ::tcltest::PrintError "location specified for -toolsdir is not a \
		    directory: $flag(-toolsdir)"
	    exit 1
	}
    } else {
	if {$::tcl_platform(platform) == "windows"} {
	    set ::protest::toolsDirectory \
		    //pop/tools/$::protest::currentVersion(Tools)/${::protest::platform}/bin 
	} else {
	    set ::protest::toolsDirectory \
		    /tools/$::protest::currentVersion(Tools)/${::protest::platform}/bin 
	}
    }

    if {![info exists flag(-tmpdir)]} {
	# reset the default output directory to
	# ./testOutputDir/platform-date-time-pid
#    set temporarySubDirectory $::protest::platform
#    append temporarySubDirectory \
#	    [clock format [clock seconds] -format {-%d%b%y-%H%M%S-}] [pid]
#    set ::tcltest::temporaryDirectory [file join [pwd] testOutputDir \
#	    $temporarySubDirectory]
	set ::tcltest::temporaryDirectory [file join \
		$::tcltest::workingDirectory testOutputDir]
	if {[file exists $::tcltest::temporaryDirectory]} {
	    if {![file isdir $::tcltest::temporaryDirectory]} { 
		::tcltest::PrintError "$tmpDirError \"$::tcltest::temporaryDirectory\" \
			is not a directory"
		exit 1
	    } elseif {![file writable $::tcltest::temporaryDirectory]} {
		::tcltest::PrintError "$tmpDirError \"$::tcltest::temporaryDirectory\" \
			is not writeable" 
		exit 1
	    } elseif {![file readable $::tcltest::temporaryDirectory]} {
		::tcltest::PrintError "$tmpDirError \"$::tcltest::temporaryDirectory\" \
			is not readable" 
		exit 1
	    }
	} else {
	    file mkdir $::tcltest::temporaryDirectory
	}
    }

    set ::tcltest::preserveCore 2

    # Create an unwrapped executable in ::tcltest::temporaryDirectory for
    # this tool.  The unwrapped executable file will source the
    # appropriate sources in ::protest::sourceDirectory. 

    set interp [info nameofexecutable]
    regsub tclsh $interp wish interp
    set tool [file tail $::protest::sourceDirectory]

    set ::protest::interpreterDirectory [file join \
	    $::tcltest::temporaryDirectory bin]
    if {![file isdir $::protest::interpreterDirectory]} {
	file mkdir $::protest::interpreterDirectory
    }

    if {[info exists flag(-srcsdir)]} {
	set ::protest::executableDirectory $::protest::interpreterDirectory
    }

    if {$::tcltest::debug > 1} {
	puts "::protest::installationDirectory = $::protest::installationDirectory"
	puts "::protest::workspaceDirectory = $::protest::workspaceDirectory"
	puts "::protest::executableDirectory = $::protest::executableDirectory"
	puts "::protest::sourceDirectory = $::protest::sourceDirectory"
	puts "::protest::toolsDirectory = $::protest::toolsDirectory"
	puts "::tcltest::testsDirectory = $::tcltest::testsDirectory"
	puts "::tcltest::temporaryDirectory = $::tcltest::temporaryDirectory"
    }

    # Set the DISPLAY environment variable if it doesn't already exist.
    if {$::tcl_platform(platform) == "unix" && ![info exists ::env(DISPLAY)]} {
	set ::env(DISPLAY) weasel:0.0
    }

    if {$::tcltest::debug > 1} {
	puts "::protest::platform = $::protest::platform"
	if {$::tcl_platform(platform) == "unix"} {
	    puts "::env(DISPLAY) = $::env(DISPLAY)"
	}
    }
    return
}

proc ::tcltest::cleanupTestsHook {} {    
    return
}

# ::protest::testAllFiles --
#
#    Instead of repeating the same code for each <tool>/all.tcl file,
#    those files now invoke this procedure.  When running tests in a Tk
#    shell, output of subprocesses must be directed to a temporary
#    log file, as wish does not have access to stdout.
#
# args:
#
#    tool      Name of the dir containing the tests to run (e.g. util)
#    interp    Interp in which to run all <tool>/*.test files.
#
# results:
#
#    Files matching <tool>/*.test are run in the <interp>.  The results of the
#    tests are output to ::tcltest::outputChannel. This proc has no return value.

proc ::protest::testAllFiles {tool interp} {
    global argv

    set shell [::protest::findExeFile $interp 1]

    # We need to pass the values that were set in the top-level test file (and
    # the command line) down into the sub-interpreters. We need to reconstruct
    # the argument list because variable values could have been reset without
    # using command line flags.
    
    set flags [list -tmpdir $::tcltest::temporaryDirectory \
	    -ws $::protest::workspaceDirectory]

    # Run each matching file in the selected shell

    set testList [::tcltest::getMatchingFiles $::tcltest::testsDirectory]

    # The results of each test file are printed out separately.  Initialize
    # the sum of all of the individual test results so that we can print out
    # a summary of all the tests at the end.

    foreach index [list "Total" "Passed" "Skipped" "Failed"] {
	set ::tcltest::numTests($index) 0
    }
    set ::tcltest::failFiles {}

    foreach file [lsort $testList] {
	set tail [file tail $file]
	puts $::tcltest::outputChannel $tail

	set logfile [file join $::tcltest::temporaryDirectory \
		"${tool}Log.txt"] 
    
	# Run each *.test file in the selected Tk shell.
	# This is used for parser and debugger tests on Windows...

	# Direct the output individual test files to a temporary log file.
	# Note that shell and file can have spaces in their names, and
	# argv can have spaces in individual elements.

	set cmd [concat [list | $shell $file] $argv [list -outfile \
		$logfile] $flags] 

	if {$::tcltest::debug > 2} {
	    puts "Command to be executed: $cmd"
	}

	if {[catch {
	    set pipeFd [open $cmd "r"]
	    while {[gets $pipeFd line] >= 0} {
		puts $::tcltest::outputChannel $line
	    }
	    close $pipeFd
	} msg]} {
	    # Print results to ::tcltest::outputChannel.
	    puts $::tcltest::outputChannel $msg
	}

	# Now concatenate the temporary log file to
	# ::tcltest::outputChannel 

	if {[catch {
	    set fd [open $logfile "r"]
	    while {![eof $fd]} {
		gets $fd line
		if {![eof $fd]} {
		    if {[regexp {^([^:]+):\tTotal\t([0-9]+)\tPassed\t([0-9]+)\tSkipped\t([0-9]+)\tFailed\t([0-9]+)} $line null testFile Total Passed Skipped Failed]} {
			foreach index [list "Total" "Passed" "Skipped" \
				"Failed"] {
			    incr ::tcltest::numTests($index) [set $index]
			}
			incr ::tcltest::numTestFiles
			if {$Failed > 0} {
			    lappend ::tcltest::failFiles $testFile
			}
		    } else {
			puts $::tcltest::outputChannel $line
		    }
		}
	    }
	    close $fd
	} msg]} {
	    puts $::tcltest::outputChannel $msg
	}
	#file delete -force $logfile
    }
}

# findExeFile --
#
#       If wrapped file exists, use it:
#          ::protest::executableDirectory/filePattern(.exe)
#       Otherwise construct it from the sources, and save it in:
#          use unwrapped ::tcltest::temporaryDirectory/filePattern(.exe)
#
#    Return the full path and name of the file or error if none exists.
#

proc ::protest::findExeFile {tool {wrapped 0}} {
    global tcl_platform flag

    set filePattern $tool
    if {$tcl_platform(platform) == "windows"} {
	# Windows files need .exe extensions
	set fileTail "$filePattern.exe"
	# 'd' is appended to root of Windows debug execuatbles
	set fileDebugTail "$filePattern\d.exe"
    } else {
	set fileTail $filePattern
	# 'g' is appended to root of Unix debug execuatbles
	set fileDebugTail "$filePattern\g"
    }

    # look for fileTail (and then fileDebugTail) in
    # ::protest::executableDirectory 

    foreach tail [list $fileTail $fileDebugTail] {
	set file [file join $::protest::executableDirectory $tail]
	if {[file isfile $file] && [file executable $file]} {
	    if {$::tcltest::debug > 2} {
		puts "wrapped $filePattern --> $file"
	    }
	    return "$file"
	}
    }
    
    # Use the unwrapped stuff built in the interp directory if it exists

    set file [file join $::protest::interpreterDirectory $fileTail]
    if {[file isfile $file] && [file executable $file]} {
	if {$::tcltest::debug > 2} {
	    puts "unwrapped $filePattern --> $file"
	}
	if {$tcl_platform(platform) == "windows"} {
	    regsub {exe$} $file bat file
	}
	return "$file"
    }
	
    ::tcltest::PrintError "Cannot find executable files \"$fileTail\" (or \
	    \"$fileDebugTail\") in $::protest::executableDirectory\
	    \nor in $::protest::interpreterDirectory."
}

proc ::protest::findSoFile {ext index} {
    global tcl_platform 

    if {$tcl_platform(platform) == "windows"} {
	regsub -all {\.} $::protest::currentVersion($index) "" vers
	return [file join $::protest::executableDirectory $ext$vers.dll]
    } else {
	if {$tcl_platform(os) == "HP-UX"} {
	    set tail "sl"
	} else {
	    set tail "so"
	}
	set vers $::protest::currentVersion($index)
	return [file join \
		[file dirname $::protest::executableDirectory] \
		lib lib$ext$vers.$tail] 
    }
}

# ::protest::getMatchingDirectories --
#
#	Looks at the patterns given to match and skip directories and uses them
#       to put together a list of the test directories that we should attempt
#       to run.  (Only subdirectories containing an "all.tcl" file are put into
#       the list.)
#
# Arguments:
#	none
#
# Results:
#	The constructed list is returned to the user.  This is used in the
#       primary all.tcl file.  Lower-level all.tcl files should use the
#       ::protest::testAllFiles proc instead.

proc ::protest::getMatchingDirectories {} {
    set matchingDirs {}
    set matchDirList {}
    # Find the matching directories in ::tcltest::testsDirectory and then
    # remove the ones that match the skip pattern
    foreach match $::protest::matchDirectories {
	foreach file [glob -nocomplain [file join $::tcltest::testsDirectory \
		$match]] {
	    if {([file isdirectory $file]) && \
		    ([file exists [file join $file all.tcl]])} {
		set matchDirList [concat $matchDirList $file]
	    }
	}
    }
    if {$::protest::skipDirectories != {}} {
	set skipDirs {} 
	foreach skip $::protest::skipDirectories {
	    set skipDirs [concat $skipDirs \
		    [glob -nocomplain [file join $::tcltest::testsDirectory \
		    $skip]]]
	}
	foreach dir $matchDirList {
	    # Only include directories that don't match the skip pattern
	    if {[lsearch -exact $skipDirs $dir] == -1} {
		lappend matchingDirs $dir
	    }
	}
    } else {
	set matchingDirs [concat $matchingDirs $matchDirList]
    }
    if {$matchingDirs == {}} {
	::tcltest::PrintError "No test directories remain after applying your \
		match and skip patterns!"
    }
    return $matchingDirs
}

proc ::protest::resetTestsDirectory {dir} {
    set ::tcltest::testsDirectory $dir
    set oDir [pwd]
    if {[file tail $::tcltest::testsDirectory] == "tests"} {
	cd [file join $::tcltest::testsDirectory .. ..]
    } else {
	cd [file join $::tcltest::testsDirectory .. .. ..]
    }
    set ::protest::workspaceDirectory [pwd]
    cd $oDir
    set ::protest::sourceDirectory [file join \
	    $::protest::workspaceDirectory tcldebugger] 
    if {$::protest::installationDirectory == ""} {
	set ::protest::executableDirectory [file join \
		$::protest::workspaceDirectory pro out \
		$::protest::buildType $::protest::platform \
		bin] 
	if {![file isdir $::protest::executableDirectory]} {
	    ::tcltest::PrintError "bad argument \
		    \"$::protest::buildType\" to -build: \ 
	    \"$::protest::executableDirectory\" is not an existing \
		    directory"
	    exit 1
	}
    }
}

# Initialize the constraints and set up command line arguments 
namespace eval protest {
    ::tcltest::InitConstraints
    ::tcltest::ProcessCmdLineArgs
}

package provide protest 1.0

return
