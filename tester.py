import os

import tester


GENERAL_ERROR = "[eE]rror:.*"


t = tester.HomeworkTester()

"""Modify this file with your tests.

The test is already filled out with some basic tests.

Basically, your main usage is:

	t.add_test("command to execute 1", "expected output as a regex string")
	t.add_test("command to execute 2", "expected output as a regex string")
	...
	t.add_test("command to execute 3", "expected output as a regex string")
	t.run()
	t.print_results()
	t.reset()
"""


##################### Basic Executables #########################
# ls should not be found
t.add_test("ls", GENERAL_ERROR)

# But /bin/echo should work
t.add_test("/bin/echo hello world", "hello world")
t.run()
t.print_results()
t.reset()

############################# Builtins ##########################
# Test that cd works
t.add_test("cd /tmp", "")
t.add_test("/bin/pwd", "/tmp")
t.add_test("cd /var", "")
t.add_test("/bin/pwd", "/var")
t.add_test("cd", GENERAL_ERROR)
t.run()
t.print_results()
t.reset()

# Test that history works as expected
t.add_test("history", "")
t.add_test("history -c", "")
t.add_test("    abc   abc   ", GENERAL_ERROR)
t.add_test("def", GENERAL_ERROR)
expected_output = [
    "0     abc   abc   |0 abc   abc   ",
    "1 def",
]
t.add_test("history", "\n".join(expected_output))
t.add_test("history -c", "")
t.add_test("history", "")
t.add_test("history blahblahblah", GENERAL_ERROR)
t.add_test("history", "")
t.add_test("/bin/echo hello", "hello")
t.add_test("history 0", "hello")
t.add_test("history", "0 /bin/echo hello")
t.run()
t.print_results()
t.reset()

############################# Pipes #############################
t.add_test("/bin/echo hello world | /bin/grep hello", "hello world")
t.add_test("/bin/echo blah          |/usr/bin/cut -b 3,4", "ah")
t.add_test("/bin/echo blah|/usr/bin/cut -b 3,4", "ah")
t.run()
t.print_results()
t.reset()

t.add_test("history | /bin/grep hello", GENERAL_ERROR)
t.add_test("history", "0 history | /bin/grep hello")
t.run()
t.print_results()
t.reset()

############################ MY TESTS ###########################
t.add_test("  /bin/ls|/bin/grep RE     |    /usr/bin/tr A-Z a-z", "readme") 
t.add_test(" history                 -c     ", "")
t.add_test("/bin/ls -all / | /bin/grep dev | /usr/bin/tr a-z A-Z | /usr/bin/tr O 0 | /usr/bin/tr T R | /bin/grep -oh R00R", "R00R\nR00R")
t.add_test("  history      0  ", "R00R\nR00R") 
t.add_test("     /bin/echo     hello          ", "hello") 
t.add_test("     history -asdasd   ",  GENERAL_ERROR)
t.add_test(" history -c manybabanananananananananas     ", GENERAL_ERROR)
t.add_test(" ", "")
t.add_test("                                            ", "")
t.add_test("                   |                   ", GENERAL_ERROR)
t.add_test("              || |\|  || /bin/ls |", GENERAL_ERROR)
 
t.run()
t.print_results()
t.reset()
