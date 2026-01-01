# EstrOS ISA

## ABI

- `t0`..`t7`: Volatile registers, not preserved across calls.
- `a0`..`a3`: Non-volatile registers, preserved across calls, `a0` is return value, and first argument.
- `ra`: Return address.
- `bp`: Base pointer.
- `sp`: Stack pointer.
- `tp`: Thread pointer.

## Integer instruction set

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
Compares `$rD = $rA + $rB + imm8`, updates the flags and stores the flags after the operation on `$rD`.

### `cmpkp $rD,$rA,$rB,imm8`
Compares `$rD = $rA + $rB + imm8` and stores the flags after the operation on `$rD` but keeps the flags without updating them.

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

## Floating point instruction set

If an instruction takes `sign`, and `sign = 0` then the result will be converted to an absolute positive value, if it's `sign = 1` then the result will be kept as-is.

### `fadd3 $fD,$fA,$fB,$fC`
Computes `$fD = ($fA + $fB) + $fC`

### `fsub3 $fD,$fA,$fB,$fC`
Computes `$fD = ($fA + $fB) - $fC`

### `fdiv3 $fD,$fA,$fB,$fC`
Computes `$fD = ($fA + $fB) / $fC`

### `fmul3 $fD,$fA,$fB,$fC`
Computes `$fD = ($fA + $fB) * $fC`

### `fmod3 $fD,$fA,$fB,$fC`
Computes `$fD = mod($fA + $fB, $fC)`

### `fmadd $fD,$fA,$fB,$fC`
Computes `$fD = $fA + $fB * $fC`

### `fmsub $fD,$fA,$fB,$fC`
Computes `$fD = $fA - $fB * $fC`

### `fsqrt3 $fD,$fA,$fB,$fC`
Computes `$fD = sqrt($fA + $fB + $fC)`

### `fhyp $fD,$fA,$fB,$fC`
Computes `$fD = hypot($fA + $fB, $fC)`

### `fnorm $fD,$fA,$fB,$fC`
Computes `$fD = sqrt($fA * $fA + $fB * $fB + $fC * $fC)`

### `fabs $fD,$fA,$fB,$fC`
Computes `$fD = abs($fA + $fB + $fC)`

### `fsign $fD,$fA,$fB,$fC`
Computes `$fD = sign($fA + $fB + $fC)`, if the result is `> 0` then `$fD` is `1`, otherwise `$fD` is `-1`, for `0` it's `0`.

### `fnabs $fD,$fA,$fB,$fC`
Computes `$fD = -abs($fA + $fB + $fC)`

### `fcos $fD,$fA,$fB,$fC`
Computes `$fD = cos($fA + $fB + $fC)`

### `fsin $fD,$fA,$fB,$fC`
Computes `$fD = sin($fA + $fB + $fC)`

### `ftan $fD,$fA,$fB,$fC`
Computes `$fD = tan($fA + $fB + $fC)`

### `facos $fD,$fA,$fB,$fC`
Computes `$fD = acos($fA + $fB + $fC)`

### `fasin $fD,$fA,$fB,$fC`
Computes `$fD = asin($fA + $fB + $fC)`

### `fatan $fD,$fA,$fB,$fC`
Computes `$fD = atan($fA + $fB + $fC)`

### `fcbrt $fD,$fA,$fB,$fC`
Computes `$fD = cbrt($fA + $fB + $fC)`

### `fy0 $fD,$fA,$fB,$fC`
Computes `$fD = y0($fA + $fB + $fC)`

### `fy1 $fD,$fA,$fB,$fC`
Computes `$fD = y1($fA + $fB + $fC)`

### `fj0 $fD,$fA,$fB,$fC`
Computes `$fD = j0($fA + $fB + $fC)`

### `fj1 $fD,$fA,$fB,$fC`
Computes `$fD = j1($fA + $fB + $fC)`

### `fexp $fD,$fA,$fB,$fC`
Computes `$fD = exp($fA + $fB + $fC)`

### `frsqrt $fD,$fA,$fB,$fC`
Computes `$fD = inverse_sqrt($fA + $fB + $fC)`

### `frcbrt $fD,$fA,$fB,$fC`
Computes `$fD = inverse_cbrt($fA + $fB + $fC)`

### `fpow2 $fD,$fA,$fB,$fC`
Computes `$fD = pow($fA + $fB, $fC)`

### `fpow3 $fD,$fA,$fB,$fC`
Computes `$fD = pow(pow($fA, $fB), $fC)`

### `fmax $fD,$fA,$fB,$fC`
Computes `$fD = ($fA + $fB) > $fC ? ($fA + $fB) : $fC`

### `fmin $fD,$fA,$fB,$fC`
Computes `$fD = ($fA + $fB) < $fC ? ($fA + $fB) : $fC`

### `fclamp $fD,$fA,$fB,$fC`
Computes `$fD = clamp($fA, $fB, $fC)`

### `finv $fD,$fA,$fB,$fC`
Computes `$fD = 1 / ($fA + $fB + $fC)`

### `fconstpi $fD,$fA,$fB,$fC`
Computes `$fD = M_PI * ($fA + $fB + $fC)`

### `fconste $fD,$fA,$fB,$fC`
Computes `$fD = M_E * ($fA + $fB + $fC)`

### `fconstpi2 $fD,$fA,$fB,$fC`
Computes `$fD = M_PI_2 * ($fA + $fB + $fC)`

### `frad $fD,$fA,$fB,$fC`
Computes `$fD = degrees_to_radians($fA + $fB + $fC)`

### `fdeg $fD,$fA,$fB,$fC`
Computes `$fD = radians_to_degrees($fA + $fB + $fC)`

### `fsel $fD,$fA,$fB,$fC`
Computes `$fD = $fA > $fB ? $fC : 0`

### `fsel2 $fD,$fA,$fB,$fC`
Computes `$fD = $fA + $fB > 0 ? $fC : 0`

### `fgamma $fD,$fA,$fB,$fC`
Computes `$fD = gamma($fA + $fB + $fC)`

### `flgamma $fD,$fA,$fB,$fC`
Computes `$fD = lgamma($fA + $fB + $fC)`

## Complex instruction set

### `faddcrr $fD,$fA,$fB,$fC`
Computes `$fD = R(complex($fA, $fB) + $fC)`

### `faddcri $fD,$fA,$fB,$fC`
Computes `$fD = I(complex($fA, $fB) + $fC)`

### `fsubcrr $fD,$fA,$fB,$fC`
Computes `$fD = R(complex($fA, $fB) - $fC)`

### `fsubcri $fD,$fA,$fB,$fC`
Computes `$fD = I(complex($fA, $fB) - $fC)`

### `fdivcrr $fD,$fA,$fB,$fC`
Computes `$fD = R(complex($fA, $fB) / $fC)`

### `fdivcri $fD,$fA,$fB,$fC`
Computes `$fD = I(complex($fA, $fB) / $fC)`

### `fmulcrr $fD,$fA,$fB,$fC`
Computes `$fD = R(complex($fA, $fB) * $fC)`

### `fmulcri $fD,$fA,$fB,$fC`
Computes `$fD = I(complex($fA, $fB) * $fC)`

### `fmodcrr $fD,$fA,$fB,$fC`
Computes `$fD = R(mod(complex($fA, $fB), $fC))`

### `fmodcri $fD,$fA,$fB,$fC`
Computes `$fD = I(mod(complex($fA, $fB), $fC))`
