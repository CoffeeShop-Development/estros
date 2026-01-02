# EstrOS ISA

## ABI

- `t0`..`t7`: Volatile registers, not preserved across calls.
- `a0`..`a3`: Non-volatile registers, preserved across calls, `a0` is return value, and first argument.
- `ra`: Return address.
- `bp`: Base pointer.
- `sp`: Stack pointer.
- `tp`: Thread pointer.

## Integer instruction set

If `$rB` is omitted, it's assumed to be `0`.

### `add $rD,$rA,imm8`
### `add $rD,$rA,$rB,imm4`
Calculates `$rD = $rA + ($rB + imm8)`

### `sub $rD,$rA,imm8`
### `sub $rD,$rA,$rB,imm4`
Calculates `$rD = $rA - ($rB + imm8)`

### `mul $rD,$rA,imm8`
### `mul $rD,$rA,$rB,imm4`
Calculates `$rD = $rA * ($rB + imm8)`

### `div $rD,$rA,imm8`
### `div $rD,$rA,$rB,imm4`
Calculates `$rD = $rA / ($rB + imm8)`

### `rem $rD,$rA,imm8`
### `rem $rD,$rA,$rB,imm4`
Calculates `$rD = $rA % ($rB + imm8)`

### `imul $rD,$rA,imm8`
### `imul $rD,$rA,$rB,imm4`
Calculates `$rD = signed($rA) * signed($rB + imm8)`

### `and $rD,$rA,imm8`
### `and $rD,$rA,$rB,imm4`
Calculates `$rD = $rA & ($rB + imm8)`

### `xor $rD,$rA,imm8`
### `xor $rD,$rA,$rB,imm4`
Calculates `$rD = $rA ^ ($rB + imm8)`

### `or $rD,$rA,imm8`
### `or $rD,$rA,$rB,imm4`
Calculates `$rD = $rA | ($rB + imm8)`

### `shl $rD,$rA,imm8`
### `shl $rD,$rA,$rB,imm4`
Calculates `$rD = $rA << ($rB + imm8)`

### `shr $rD,$rA,imm8`
### `shr $rD,$rA,$rB,imm4`
Calculates `$rD = $rA >> ($rB + imm8)`

### `popcnt $rD,$rA,imm8`
### `popcnt $rD,$rA,$rB,imm4`
Calculates `$rD = popcount($rA + ($rB + imm8))`

### `clz $rD,$rA,imm8`
### `clz $rD,$rA,$rB,imm4`
Calculates `$rD = count_leading_zeros($rA + ($rB + imm8))`

### `clo $rD,$rA,imm8`
### `clo $rD,$rA,$rB,imm4`
Calculates `$rD = count_leading_ones($rA + ($rB + imm8))`

### `bswap $rD,$rA,imm8`
### `bswap $rD,$rA,$rB,imm4`
Calculates `$rD = bswap($rA + ($rB + imm8))`

### `ipcnt $rD,$rA,imm8`
### `ipcnt $rD,$rA,$rB,imm4`
Calculates `$rD = 32 - popcount($rA + ($rB + imm8))`

### `stb $rD,$rA,imm8`
### `stb $rD,$rA,$rB,imm4`
Calculates `u8:memory[$rA + $rB * imm8 * 4] = $rD`

### `stw $rD,$rA,imm8`
### `stw $rD,$rA,$rB,imm4`
Calculates `u16:memory[$rA + $rB * imm8 * 4] = $rD`

### `stl $rD,$rA,imm8`
### `stl $rD,$rA,$rB,imm4`
Calculates `u32:memory[$rA + $rB * imm8 * 4] = $rD`

### `stq $rD,$rA,imm8`
### `stq $rD,$rA,$rB,imm4`
Calculates `u64:memory[$rA + $rB * imm8 * 4] = $rD`

### `ldb $rD,$rA,imm8`
### `ldb $rD,$rA,$rB,imm4`
Calculates `$rD = u8:memory[$rA + $rB * imm8 * 4]`

### `ldw $rD,$rA,imm8`
### `ldw $rD,$rA,$rB,imm4`
Calculates `$rD = u16:memory[$rA + $rB * imm8 * 4]`

### `ldl $rD,$rA,imm8`
### `ldl $rD,$rA,$rB,imm4`
Calculates `$rD = u32:memory[$rA + $rB * imm8 * 4]`

### `ldq $rD,$rA,imm8`
### `ldq $rD,$rA,$rB,imm4`
Calculates `$rD = u64:memory[$rA + $rB * imm8 * 4]`

### `lea $rD,$rA,imm8`
### `lea $rD,$rA,$rB,imm4`
Calculates `$rD = $rA + $rB * imm8 * 4`

### `cmp $rD,$rA,imm8`
### `cmp $rD,$rA,$rB,imm4`
Compares `$rD = $rA + $rB + imm8`, updates the flags and stores the flags after the operation on `$rD`.

### `cmpkp $rD,$rA,imm8`
### `cmpkp $rD,$rA,$rB,imm4`
Compares `$rD = $rA + $rB + imm8` and stores the flags after the operation on `$rD` but keeps the flags without updating them.

## Accelerated DMA instruction set

### `memcpy $rD,$rA,$rB,$rC`

Copies `$rC` bytes from `$rB` to `$rA`. Computes `$rD = $rA`.

### `memmov $rD,$rA,$rB,$rC`

Copies `$rC` bytes from `$rB` to `$rA`. Computes `$rD = $rA`. If `$rA > $rB` then the copy is done backwards.

### `memset $rD,$rA,$rB,$rC`

Sets `$rC` bytes on `$rA` to the value of `u8($rB)`.
Set `$rD = $rA`.

### `memchr $rD,$rA,$rB,$rC`

Finds `u8($rB)` in memory area `$rA` with `$rC` bytes. Set `$rD` to the address where it gets found.

### `memchrf $rD,$rA,$rB,$rC`

Finds `u8($rB)` in memory area `$rA` with `$rC` bytes. Set `$rD` to the address where it gets found.
Updates `Z` and `N` appropriatedly.

### `strcpy $rD,$rA,$rB,$rC`

### `strcat $rD,$rA,$rB,$rC`

### `strpbrk $rD,$rA,$rB,$rC`

### `strncpy $rD,$rA,$rB,$rC`

### `strncat $rD,$rA,$rB,$rC`

### `strchr $rD,$rA,$rB,$rC`

### `strnchr $rD,$rA,$rB,$rC`

### `indtab $rD,$rA,$rB,$rC`

Obtains the pointer in the address `$rA + ($rB + $rC) * 4`.

### `indtab8 $rD,$rA,$rB,$rC`

Obtains the pointer in the address `$rA + ($rB + $rC) * 8`.

### `chtree $rD,$rA,$rB,$rC`

Executes the following code:
```c
uint32_t counter = rc;
uint32_t *p = ra;
do {
    p = read32(p + rb)
} while (p != NULL && counter-- > 0);
```

### `chtreeunchk $rD,$rA,$rB,$rC`

Executes the following code:
```c
uint32_t counter = rc;
uint32_t *p = ra;
do {
    p = read32(p + rb)
} while (counter-- > 0);
```

## Branch instruction set

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

### `fround $fD,$fA,$fB,$fC`
Computes `$fD = round($fA + $fB + $fC)`

### `ffloor $fD,$fA,$fB,$fC`
Computes `$fD = floor($fA + $fB + $fC)`

### `fceil $fD,$fA,$fB,$fC`
Computes `$fD = ceil($fA + $fB + $fC)`

### `fselnan $fD,$fA,$fB,$fC`
Computes `$fD = $fA == NaN ? $fB : $fC`

### `fload $fD,$rA,$rB,$rC`
Loads from memory address `$fD = float32[$rA + $rB + $rC]`

### `fstore $fD,$rA,$rB,$rC`
Stores into memory address `float32[$rA + $rB + $rC] = $fD`

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

### `fsqrtcrr $fD,$fA,$fB,$fC`
Computes `$fD = R(csqrt(complex($fA, $fB) + $fC))`

### `fsqrtcri $fD,$fA,$fB,$fC`
Computes `$fD = I(csqrt(complex($fA, $fB) + $fC))`

## Conversion instruction set

### `fcvti $rD,$fA,$fB,$fC`
Computes `$rD = bit_pattern($fA + $fB + $fC)`, doesn't do rounding

### `icvtf $fD,$rA,$rB,$rC`
Computes `$fD = bit_pattern($rA + $rB + $rC)`, doesn't do rounding

### `fcvtri $rD,$fA,$fB,$fC`
Computes `$rD = round($fA + $fB + $fC)`

### `icvtrf $fD,$rA,$rB,$rC`
Computes `$fD = round($rA + $rB + $rC)`
