REM list_line bound: ? expands to PRINT, LIST and SAVE/LOAD round-trip
10 ?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?:?
LIST
SAVE "LONG1"
NEW
LOAD "LONG1"
LIST
SAVE "LONG2"
NEW
20 ??????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
SAVE "LONG3"
NEW
LOAD "LONG3"
PRINT LEN(STR$(PEEK(0)))>0
LIST 20
NEW
REM recursive DEF FN and deep nesting
10 DEF FNA(X)=FNA(X)+1
20 DEF FNB(X)=FNC(X)+1
30 DEF FNC(X)=FNB(X)+1
40 DEF FNS(X)=X*2
50 PRINT FNS(FNS(FNS(FNS(3))))
60 PRINT FNA(1)
RUN
PRINT FNB(1)
NEW
PRINT ((((((((((((1))))))))))))
PRINT (((((((((((((((((((((((((((((((((((((((1)))))))))))))))))))))))))))))))))))))))
REM long graphics spans terminate
HGR
LINE -20000,0 TO 20000,100
LINE 0,-30000 TO 100,30000
CIRCLE 160,100,30000
PRINT "LINES DONE"
GR
REM non-integral STEP on an integer loop
FOR I%=1 TO 3 STEP .5:PRINT I%:NEXT
FOR I%=1 TO 3 STEP 1.5E9:NEXT
FOR I%=3 TO 1 STEP -1:PRINT I%;:NEXT
PRINT
FOR X=1 TO 2 STEP .5:PRINT X;:NEXT
PRINT
REM LOAD closes its file when a line is rejected
LOAD "BAD"
LOAD "BAD"
LOAD "BAD"
LOAD "BAD"
LOAD "BAD"
LOAD "BAD"
LOAD "BAD"
LOAD "BAD"
LOAD "BAD"
LOAD "BAD"
LOAD "BAD"
LOAD "BAD"
LOAD "GOOD"
RUN
REM edits drop stale GOSUB / DATA state
NEW
10 GOSUB 100
20 END
100 STOP
RUN
15 REM
RETURN
NEW
10 DATA 1,2,3
20 READ A:PRINT A
RUN
5 REM
READ B:PRINT B
REM backslash truncates toward zero
PRINT -7.5\2,-7\2,7.5\2,7\2,-7.9\-2.9
REM long numeric text is not cut to a prefix
A$="0000000000000000000000000000000000000000000000042"
PRINT VAL(A$)
A$=A$+A$+A$
PRINT VAL(A$)
NEW
10 DATA 0000000000000000000000000000000000000000000000000000000000000000000042
20 READ A
RUN
REM file names: .BAS is always added
NEW
10 PRINT "X"
SAVE "NINECHARS"
SAVE "ABCDEFGH"
SAVE "ABCDEFGH.TXT"
LOAD "ABCDEFGH"
LIST
LOAD "ABCDEFGH.TXT"
LIST
REM long REM and DATA lines
NEW
10 REM XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
20 DATA 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
LIST
SAVE "REMS"
NEW
LOAD "REMS"
LIST
