This file describes syntax used in *.resource files

*.resource file describes resource composed of multiple files

Definitions:
resource := absolute path to target resource starting from www_root directory as root
LF := "\n" line feed character

Syntax of *.resource file
*(resource LF) LF

Example:
/abc.resource
---------------------------------------
/abc.head
/abc.body
/abc.tail

---------------------------------------

When sending response for GET request to /abc.resource
server should send files /abc.head first then 
/abc.body and finally file /abc.tail

=======================================

If one of the files listed ends with .resource it isn't send,
it is processed recursively in the same way instead.

Example 2:
/abc.resource
---------------------------------------
/def.resource
/jkl.html

---------------------------------------

/def.resource
---------------------------------------
/ghi.html

---------------------------------------

When sending response for GET request to /abc.resource
server should send files /ghi.html first then /jkl.html

