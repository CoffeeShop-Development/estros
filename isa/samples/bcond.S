bcond_test:
    bz $a0,L1,? # If ($a0 == 0)
    # If NOT ($a0 == 0)
    bz $a0,L1,?
    # If NOT (($a0 == 0) AND (N OR Z OR C))
    bz $a0,L1,?!nzc
    # If NOT (($a0 != 0) AND (N OR C))
    bn $a0,L1,?!nc
    # If ($a0 != 0) AND (N OR C)
    bn $a0,L1,?nc
    # If ($a0 != 0) AND Z
    bn $a0,L1,?z
L1:
    ret
