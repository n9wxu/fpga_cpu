10 DIM Z(30000,1): REM leave only a few KB for strings
20 DIM W$(50)
30 FOR R=1 TO 3000
40 K=INT(RND(1)*51): W$(K)=W$(K)+CHR$(65+R MOD 26): IF LEN(W$(K))>40 THEN W$(K)=MID$(W$(K),7)
50 A$=W$(K)+W$(50-K): B$=LEFT$(A$,3)+RIGHT$(A$,3)+B$: B$=LEFT$(B$,30)
60 NEXT
70 T=0: FOR I=0 TO 50: T=T+LEN(W$(I)): FOR J=1 TO LEN(W$(I)): T=T+ASC(MID$(W$(I),J,1)): NEXT J,I
80 PRINT T; LEN(B$); FRE(0) < 30000
RUN
