# Store A2 bytes from A0 into A1, returning A0
memcpy:
    bz $a2,L1,?
    ldb $t0,$a0,0
    stb $a1,$t0,0
    sub $a2,$a2,1
    bz $a2,memcpy,?!
L1:
    #ret -- just halt
