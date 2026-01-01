# EstrOS ISA

## ABI

- `t0`..`t7`: Volatile registers, not preserved across calls.
- `a0`..`a3`: Non-volatile registers, preserved across calls, `a0` is return value, and first argument.
- `ra`: Return address.
- `bp`: Base pointer.
- `sp`: Stack pointer.
- `tp`: Thread pointer.

## Instruction set

### `add $rD,$rA,$rB,imm8`
Calculates `$rD = $rA + ($rB + imm8)`

### `sub $rD,$rA,$rB,imm8`
Calculates `$rD = $rA + ($rB + imm8)`

### `mul $rD,$rA,$rB,imm8`
Calculates `$rD = $rA + ($rB + imm8)`

### `div $rD,$rA,$rB,imm8`
Calculates `$rD = $rA + ($rB + imm8)`

### `rem $rD,$rA,$rB,imm8`
Calculates `$rD = $rA + ($rB + imm8)`

### `imul $rD,$rA,$rB,imm8`
Calculates `$rD = $rA + ($rB + imm8)`

### `and $rD,$rA,$rB,imm8`
Calculates `$rD = $rA + ($rB + imm8)`

### `xor $rD,$rA,$rB,imm8`
Calculates `$rD = $rA + ($rB + imm8)`

### `or $rD,$rA,$rB,imm8`
Calculates `$rD = $rA + ($rB + imm8)`

### `shl $rD,$rA,$rB,imm8`
Calculates `$rD = $rA + ($rB + imm8)`

### `shr $rD,$rA,$rB,imm8`
Calculates `$rD = $rA + ($rB + imm8)`

### `popcnt $rD,$rA,$rB,imm8`
Calculates `$rD = popcount($rA + ($rB + imm8))`

### `clz $rD,$rA,$rB,imm8`
Calculates `$rD = count_leading_zeros($rA + ($rB + imm8))`

### `clo $rD,$rA,$rB,imm8`
Calculates `$rD = count_leading_ones($rA + ($rB + imm8))`

### `bswap $rD,$rA,$rB,imm8`
Calculates `$rD = bswap($rA + ($rB + imm8))`

### `ipcnt $rD,$rA,$rB,imm8`
Calculates `$rD = 32 - popcount($rA + ($rB + imm8))`

### `stb $rD,$rA,$rB,imm8`
Calculates `u8:memory[$rA + $rB * imm8 * 4] = $rD`

### `stw $rD,$rA,$rB,imm8`
Calculates `u16:memory[$rA + $rB * imm8 * 4] = $rD`

### `stl $rD,$rA,$rB,imm8`
Calculates `u32:memory[$rA + $rB * imm8 * 4] = $rD`

### `stq $rD,$rA,$rB,imm8`
Calculates `u64:memory[$rA + $rB * imm8 * 4] = $rD`

### `ldb $rD,$rA,$rB,imm8`
Calculates `$rD = u8:memory[$rA + $rB * imm8 * 4]`

### `ldw $rD,$rA,$rB,imm8`
Calculates `$rD = u16:memory[$rA + $rB * imm8 * 4]`

### `ldl $rD,$rA,$rB,imm8`
Calculates `$rD = u32:memory[$rA + $rB * imm8 * 4]`

### `ldq $rD,$rA,$rB,imm8`
Calculates `$rD = u64:memory[$rA + $rB * imm8 * 4]`

### `lea $rD,$rA,$rB,imm8`
Calculates `$rD = $rA + $rB * imm8 * 4`

### `cmp $rD,$rA,$rB,imm8`
Compares `$rD = $rA + $rB * imm8 * 4`, updates the flags and stores the flags after the operation on `$rD`.

### `cmpkp $rD,$rA,$rB,imm8`
Compares `$rD = $rA + $rB * imm8 * 4` and stores the flags after the operation on `$rD` but keeps the flags without updating them.

### `jmp addr16`
Jumps to the absolute address `addr16`.

### `jumprel rela16`
Jumps to the relative address `$pc + signed(rela16)`

### `call $rD,rela8`
Jumps to the relative address `$pc + signed(rela8)`, stores `$rD = $pc + 1`.

### `ret`
Sets `$pc = $ra` (Return Address register).

### `b $rA,rela8,cc`
Various variants of this instruction exists (with the same parameters):
- `bz`: Branch if `$rA == 0`
- `b`: Branch always
- `bgzs`: Branch if `signed($rA) > 0`
- `bgpc`: Branch if `$rA > $pc`
- `bgpcrela`: Branch if `$rA > $pc + rela8`
- `bo`: Branch if `$rA == 1`
- `bgoz`: Branch if `signed($rA) > 1`
- `bemax`: Branch if `$rA == 0xffffffff`
- `bet0` ... `bet1`: Branch if `$rA == $t0` up to `$rA == $t7`.

`cc` can then be composed of the following:
- `!`: Negate the whole conditional expression, for example if used on a `bz $rA,0,!nc` then the comparison would be `!($rA == 0 && N && C)`.
- `n`: Check for `N` flag.
- `c`: Check for `C` flag.
- `z`: Check for `Z` flag.

- `g`: Shorthand for for `!nc`.
- `l`: Shorthand for `n`.
- `e`: Shorthand for `z`.
