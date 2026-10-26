10 for i=10 to 1 step -3:print i;:next:print
20 score=5:total=score*2:print "score";score,"total";total
30 if 1 then if 0 then print "A" else print "B" else print "C"
40 if 0 then if 1 then print "D" else print "E" else print "F"
50 print tab(5);"X";spc(3);"Y";tab(2);"Z": print pos(0)
60 for i%=1 to 5 step 2:print i%;:next i%:print
70 x=2:on x goto 80,90
80 print "WRONG"
90 print "RIGHT":goto100
100 a$="HELLO":b$=a$:a$="X":print a$;b$;-3;"-";1E-5
110 print 1;2;3,"Z";:print " END"
120 print "a"<"b","b"<"a","abc"="abc","ab"<"abc",
130 print:? 2+3*4^2/8-1, -2^2, (2+3)*4, 10-2-3, 2^3^2
140 if a$="X" then 160
150 print "SKIPPED?"
160 print "OK":rem done
170 print not 1=2, 5>3 and 2>1, 1 or 0, -1 and 7
180 FOR Q=5 TO 1:PRINT "NO":NEXT:PRINT "ZERO-TRIP OK"
190 FOR Q=1 TO 2:FOR W=9 TO 1:PRINT "NO":NEXT W,Q:PRINT "NESTED OK"
LIST
RUN
