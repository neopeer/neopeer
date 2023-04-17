Coding style recommendations are a combination of good practice and recommendations from analysis tools.  

We request:  

&emsp;*Constants (including flags) should be in all caps to denote they are such*  
&emsp;*Global variables should be preceded with (g_) to indicate they are globals*  
&emsp;*Member variables in a class or structure should be preceded with (m_) to indicate they are members*  
&emsp;*Precede internal/private/protected function names in a class or structure with an indent and an underscore (_)*  
&emsp;*Favor variable declarations at top of function to ease pentesting unless there are benefits in delayed initialization*  
&emsp;*Avoid std::vectors in favor of in-house optimizations (e.g. automated linked lists / objects from memory banks)*  
&emsp;*Avoid excessive use of the keyword (auto) if it will force contributors to dig to identify the variable type*  
&emsp;*Avoid the use of lambdas for anything outside of simple operations, and please well-document what it is doing*  
&emsp;*Avoid convoluted program flow outside of implicit conversion pathways chosen by the compiler*  

The purpose of these guidelines is to expedite understanding, testing, and debugging of program code.  

If you are the only one who can interpret the code... it's not a flex. It's a flaw.  


**Automated static analysis**:  
  
The **cppcheck** static analysis utility is invoked automatically at compile time in the Codelite projects.  Generally, its stylization and warning recommendations should be adhered to. There are exceptions in edge cases of "it's a feature, not a bug".  

For example, the constructors in the math library are deliberately defined "loosely" to ease the amount of syntax required for calling code. In this case, the code is optimized for various casting types and would still be reasonably well-optimized if an unexpected implicit conversion pathway was used. Another example is the memory debugging library which is designed to catch programmer mishaps at runtime (leaks, structure copy errors, etc...). This system needs to be as transparent as possible, which leads to some atypical coding deviations that set off the static analyzer (e.g. a throw operation inside a copy operator).  

If the programmer determines that a recommendation from the static analyzer is an edge case that should be suppressed, it may be done by placing a comment at the tail of the preceding line of the following form:  

&emsp;*//cppcheck-suppress [warningcode]*  

In the above you would replace [warningcode] with the appropriate code found from the output of cppcheck in the error log.

**Example static analysis suppression:**  

Let's say you receive the following warning/error in the out log:  

&emsp;*(style) Struct 'biguint_t < 128 >' has a constructor with 1 argument that is not explicit. [noExplicitConstructor]*  

The [warningcode] is marked at the tail of the above message.

The comment to place on the preceding line to suppress the message would be:  

&emsp;*//cppcheck-suppress noExplicitConstructor*

