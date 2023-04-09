**License**:  
MIT for all contributor work in this repository. Pirates welcome. &emsp;&emsp; ![Copy Me](docs/images/jolly-small.png)

**License Exceptions**:  
Binaries for AppImage packaging in ./deployment, copyrighted .pdf files in ./specifications/whitepapers, simplified BSD for third-party Python include in ./examples. We assert "Fair Use" for all derivative works in all (sub)directories of ./webtemplate/.

**Caution**:  
This repository is still in development.  
Use at your own risk.

**Installation on Debian/Devuan**:  
apt-get update && apt-get install aptitude  
aptitude install make gcc g++ gdb libgmp-dev codelite  
;git clone the repo

**Execution**:  
Open CodeLite  
Select C/C++ Development, GCC at compiler wizard, prefer tabs for indentation.  
Use codelite to open the workspace in /code/projects/linux/codelite/  
Compile, build, run.  

**Debugging**:  
Debugger -> Start/Continue Debugger  

**Enhanced Debugging**:  
Add the following to the compiler/linker options:  
&emsp;&emsp; -fsanitize=undefined,address  

**WARNING on Enhanced Debugging**:  
These checks can be glitchy if you run the debug executable without the debugger. Specifically, it's not uncommon to see an infinite loop of: AddressSanitizer:DEADLYSIGNAL. This error is a direct consequence of declaring static variables globally for the linker *and* executing under the aforementioned conditions. To prevent confusion these extra checks are disabled by default.	


