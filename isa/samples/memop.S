store:
    # u8[r1 + r2 * 1] = $r0
    stb $r0,$r1,$r2,1
    # u16[r1 + r2 * 1] = $r0
    stw $r0,$r1,$r2,1
    # u32[r1 + r2 * 1] = $r0
    stl $r0,$r1,$r2,1

    # $r0 = u8[r1 + r2 * 1]
    ldb $r0,$r1,$r2,1
    # $r0 = u16[r1 + r2 * 1]
    ldw $r0,$r1,$r2,1
    # $r0 = u32[r1 + r2 * 1]
    ldl $r0,$r1,$r2,1
