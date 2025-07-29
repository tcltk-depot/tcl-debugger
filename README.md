This repository contains the Tcl debugger component originally from the TclPro
product open-sourced by Scriptics Corporation. It has been extracted from the
[tcltk-depot TclProDebug](https://github.com/tcltk-depot/TclProDebug) repository
which in turn is a fork of [flightaware](https://github.com/flightaware/TclProDebug).

Changes with respect to the flightware repository include:

- Update for Tcl/Tk 9.0
- Removal of vestigial TclPro components not required by the debugger
- Refactoring to remove internal dependencies
- Use of external [parser](https://github.com/tcltk-depot/tcl-parser) and
[Tcllib cmdline](https://core.tcl-lang.org/tcllib/doc/trunk/embedded/index.md)
packages. These need to be installed before running the debugger.
- Working test suite

To run: run the file `main.tcl` with `wish`.

*Note: the starkit functionality for 8.6 probably needs fixing.*

The Help menu item on the Debugger's menu bar has an option to open the TclPro
user's guide, which will appear as a PDF file in the user's default browser.
The information in the chapter on the Debugger is still valid.

Below is the relevant portion of the Flightaware README
```
The debugger code has been upgraded to function with up-to-date releases of 
Tcl/Tk (i.e., versions 8.5, 8.6):

* Tk GUI code upgraded to work with current Tk API.

* Upgraded OS interaction code to work with current operating system releases.

* Instrumentation code added to accomodate the expand operator.

* Code added for proper custom instrumentation of new Tcl commands (e.g. apply,
dict, try) and subcommands.

* Put remote-debugging client code file into package for ease of access.

* Cleanup and correction of doc files.

* Files and directories re-arranged into starkit-ready format.

* Added script to wrap debugger code into a starkit of minimum size.

* Miscellaneous bug fixes.
```