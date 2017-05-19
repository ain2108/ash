Anton Nefededenkov
ain2108

## Files:
Makefile
README
shell.c
shell.h
tester.py

## cash
cash is a very simple shell that supports pipes and three builtin commands:
cd, history and exit. Builtin commands do not work with pipelines, as specified
in the spec. 

A line is read from stdin. This is handled by a function get_big_line().
The function will initially malloc a buffer of size COMMON_SIZE. Afer that,
if line is bigger then COMMON_SIZE, the function will realloc() to COMMON_SIZE * 2.

The read line is then passed to function execute_line(). Execute line will save the input
line into history[HISTORY_SIZE], make minor formattig changes to the line, use strtok() to
tokenize the line and finally build a stack allocated array of strings, where each string can
be either a cmd, an argument to the command or a pipe "|". execute_line() will call inerpret()
on the array of strings. 

interpret() will make decisions on what to do with the array of strings. It will recognize if the array represents 
a single executable, a built in function or a pipeline of executables. If the array represents a pipeline, interpret() will
find the positions of pipes, check for correct semantics, isolate parts of the input array of string into an array of lines
where each line is a single executable with its arguments. After that, it will create two pipe1 and pipe2, loop throuh the array of
individual executables callin execute_line() on each of them. To avoid duplication of history, flag PIPELINE=1 which will 
influence the behaviour of execute_line() appropriately. During the iteration, pipes will be swapped and recycled appropriately.
If the input array of strings is a single executable command with its arguments, interpret() will call execute(). Finally, if the 
input array of strings is a built in command, interpret will perform actions appropriate to the built in. 

execute() has awareness of who and under what cicumstances has called it through consultin a set of global variables. 
In short, execute forks(), parent returns immediately, child sets up the I/O appropriately to the state of the program, and
executes the executable. If error occurs, child cleans up and exits with an error message printed to stderr. 

Child processes are all waited for by their main parrent. In the end of interpret one can see this routine.
History is implemented using an array and modular arithmeti, and some more hacked up arithmetic. But it works. 
Each error is assigned an integer value. The function handle_error(int ERROR_CODE) will, you guessed it, handle errors. 
As of now, for most of the errors, error handle = careful termination of the program. 
I think this should cover it more or less.

cash should run valgrind clean.
If run on MAC, there will be 10k bytes displayed as still reachable on exit.
At the top of shell.c there is a flag ON_MAC, that if set to 1 will 
This problem does not exist on Debian.

## Valgrind
w4118@w4118:~/OS/hmwk1-ain2108$ valgrind ./w4118_sh
==1275== Memcheck, a memory error detector
==1275== Copyright (C) 2002-2013, and GNU GPL'd, by Julian Seward et al.
==1275== Using Valgrind-3.10.0 and LibVEX; rerun with -h for copyright info
==1275== Command: ./w4118_sh
==1275== 
$/bin/ls
Makefile	 README   shell.h  w4118_sh
myhmw1-tests.py  shell.c  shell.o  written.txt
$ls
error: No such file or directory
==1278== 
==1278== HEAP SUMMARY:
==1278==     in use at exit: 0 bytes in 0 blocks
==1278==   total heap usage: 6 allocs, 6 frees, 296 bytes allocated
==1278== 
==1278== All heap blocks were freed -- no leaks are possible
==1278== 
==1278== For counts of detected and suppressed errors, rerun with: -v
==1278== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
$/bin/ls | /bin/grep shell
shell.c
shell.h
shell.o
$exit
==1275== 
==1275== HEAP SUMMARY:
==1275==     in use at exit: 0 bytes in 0 blocks
==1275==   total heap usage: 16 allocs, 16 frees, 788 bytes allocated
==1275== 
==1275== All heap blocks were freed -- no leaks are possible
==1275== 
==1275== For counts of detected and suppressed errors, rerun with: -v
==1275== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)



Build the shell by simply running make in the root directory. 
Notice, the test run is done on OS X, therefore paths are slightly 
different. 

## Test Run
nemo$ make
gcc -I -Wall   -c -o shell.o shell.c
gcc -o w4118_sh shell.o -I -Wall 

nemo$ ./w4118_sh
$/bin/ls -all | /usr/bin/grep shell | /usr/bin/tr a-z A-Z
-RW-R--R--   1 NEMO  STAFF  17186 SEP 17 21:05 SHELL.C
-RW-R--R--   1 NEMO  STAFF    970 SEP 17 20:59 SHELL.H
-RW-R--R--   1 NEMO  STAFF  17356 SEP 17 21:51 SHELL.O
$/bin/echo "Cats Again Shell"
"Cats Again Shell"
$ls      
error: No such file or directory
$banana
error: No such file or directory
$             |||  ||  ||| | | | || | | | || | | | |
error: bad pipeline structure
$history
0 /bin/ls -all | /usr/bin/grep shell | /usr/bin/tr a-z A-Z
1 echo "Cats Again Shell"
2 /bin/echo "Cats Again Shell"
3 ls
4 banana
5              |||  ||  ||| | | | || | | | || | | | |
$history 1
error: No such file or directory
$history 2
"Cats Again Shell"
$exit
nemo$ make clean
rm shell.o w4118_sh


## Test Script
myhmw1-tests.py contains the standard tests plus few more that I have added.
I ran out of time, so my set of tests is far from impressive. 
The tester completes all the tests succesfully on Debian:
w4118@w4118:~/OS/tester-hmwk1$ python myhmw1-tests.py 
2 of 2 tests passed
5 of 5 tests passed
12 of 12 tests passed
3 of 3 tests passed
2 of 2 tests passed
11 of 11 tests passed





## Git Log
(HEAD, master) Finsihed the written part of the hw (53 seconds ago) <ain2108>
(origin/master, origin/HEAD) added half of the written part (13 hours ago) <ain2108>
Ran valgrind again to make sure everything is still memory error free. Dumped the ouput into README. (20 hours ago) <Anton Nefedenkov>
Merge branch 'master' of https://github.com/W4118/hmwk1-ain2108 (20 hours ago) <ain2108>
Done with the programming part (21 hours ago) <Anton Nefedenkov>
Tiny little bug fixed. README added. (20 hours ago) <ain2108>
more more more more robustness (21 hours ago) <ain2108>
added tests on robustness (21 hours ago) <Anton Nefedenkov>
Made the shell a littl more robust to broken input (22 hours ago) <ain2108>
Added few of my own tests. Everything seems to work. Addiing the to the repo. (22 hours ago) <W4118 Student>
moved some stuff from heap to stack (2 days ago) <ain2108>
forgot to free the heap of children where execv failed (2 days ago) <ain2108>
closing the pipes in case of error in execv (2 days ago) <ain2108>
children have still reachable bytes on execl error, fixing it (2 days ago) <ain2108>
fixed a potential bug with history (2 days ago) <ain2108>
more fixes for the spec (2 days ago) <ain2108>
one of tests is fails, but the expected output and the output seem identical (2 days ago) <ain2108>
fixed to satisfy the change in the history spec (2 days ago) <ain2108>
Everything seems to work just as expected. Added error handling to all system calls (2 days ago) <ain2108>
fixed bugs with history. No errors in pipe usage. Need to finish adding error recovery (2 days ago) <ain2108>
Shell is more or less ready. Pipes and all built ins working as specified. Need to clean code and fix few bugs. (2 days ago) <ain2108>
Implmentation for single pipe complete (2 days ago) <ain2108>
Fixed memory errors in history (2 days ago) <ain2108>
Problem found with pipe recycling (3 days ago) <ain2108>
Multiple pipes template implemented and working. There is some bug in history again... (3 days ago) <ain2108>
global var rename (4 days ago) <ain2108>
Fixed minoe concurance bug. (4 days ago) <ain2108>
Added the functionality to support pipes. Left allow multiple pipes as well as better interpretation (4 days ago) <ain2108>
Fixed a bug with argc. Added formatting to simplify pipe interpretation. (4 days ago) <ain2108>
Added history -c command (7 days ago) <ain2108>
Fixed the history (7 days ago) <ain2108>
The skeleton of the shell. History is not working according to the spec. (Inverse order) (7 days ago) <ain2108>
