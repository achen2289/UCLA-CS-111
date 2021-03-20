#! /bin/bash

# No arguments passed in
echo "hi" > input.txt
./lab0 < input.txt > output.txt
if test "$?" -eq 0 ; then
    echo "TEST #1 PASSED: program exited with status 0 (no arguments)";
else
    echo "TEST #1 FAILED: program did not exit with status 0 (no arguments)";
fi;

# Input file specified, output not specified
echo "hi again" >> input.txt
./lab0 --input=input.txt > output.txt
if test "$?" -eq 0; then
    echo "TEST #2 PASSED: program exited with status 0 (input file specified)";
else
    echo "TEST #2 FAILED: program did not exit with status 0 (input file specified)";
fi;

# Output file specified, input not specified
echo "more text" >> input.txt
./lab0 --output=another_output.txt < input.txt
if test "$?" -eq 0; then
    echo "TEST #3 PASSED: program exited with status 0 (output file specified)";
else
    echo "TEST #3 FAILED: program did not exit with status 0 (output file specified)";
fi;

# Both input and output file specified
./lab0 --input=input.txt --output=another_output.txt
if test "$?" -eq 0; then
    echo "TEST #4 PASSED: program exited with status 0 (input and output files specified)";
else
    echo "TEST #4 FAILED: program did not exit with status 0 (input and output files specified)";
fi;

# Invalid option passed
./lab0 --input=haha --output=random --whatisthis
if test "$?" -eq 1; then
    echo "TEST #5 PASSED: program exited with status 1 (invalid option)";
else
    echo "TEST #5 FAILED: program did not exit with status 1 (invalid option)";
fi;

# Missing argument
./lab0 --output=somefile --input 
if test "$?" -eq 1; then
    echo "TEST #6 PASSED: program exited with status 1 (missing argument)";
else
    echo "TEST #6 FAILED: program did not exit with status 1 (missing argument)";
fi;

# Cannot open input file
chmod a-r input.txt 
./lab0 --input=input.txt > output.txt
if test "$?" -eq 2; then
    echo "TEST #7 PASSED: program exited with status 2 (cannot open input file)";
else
    echo "TEST #7 FAILED: program did not exit with status 2 (cannot open input file)";
fi; 

# Cannot open output file
chmod a-w another_output.txt 
./lab0 --output=another_output.txt > output.txt
if test "$?" -eq 3; then
    echo "TEST #8 PASSED: program exited with status 3 (cannot open output file)";
else
    echo "TEST #8 FAILED: program did not exit with status 3 (cannot open output file)";
fi;

# Caught and received SIGSEGV
./lab0 --segfault --catch
if test "$?" -eq 4; then
    echo "TEST #9 PASSED: program exited with status 4 (caught and received SIGSEGV)";
else
    echo "TEST #9 FAILED: program did not exit with status 4 (caught and received SIGSEGV)";
fi;

# Segmentation fault
./lab0 --segfault
if test "$?" -eq 139; then
    echo "TEST #10 PASSED: program exited with status 139 (forced segmentation fault)";
else
    echo "TEST #10 FAILED: program did not exit with status 139 (forced segmentation fault)";
fi;

# Matching file content
chmod a+r input.txt
chmod a+w another_output.txt
echo "new text" > input.txt
./lab0 --input=input.txt --output=another_output.txt > output.txt
cmp input.txt another_output.txt
if test "$?" -eq 0; then
    echo "TEST #11 PASSED: program exited with status 0 (matching file contents)";
else
    echo "TEST #11 FAILED: program did not exit with status 0 (matching file contents)";
fi;

rm -f input.txt output.txt another_output.txt