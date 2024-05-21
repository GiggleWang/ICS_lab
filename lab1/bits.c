/*
 * CS:APP Data Lab
 *
 * bits.c - Source file with your solutions to the Lab.
 *          This is the file you will hand in to your instructor.
 *
 * WARNING: Do not include the <stdio.h> header; it confuses the dlc
 * compiler. You can still use printf for debugging without including
 * <stdio.h>, although you might get a compiler warning. In general,
 * it's not good practice to ignore compiler warnings, but in this
 * case it's OK.
 */

#include "btest.h"
#include <limits.h>

/*
 * Instructions to Students:
 *
 * STEP 1: Fill in the following struct with your identifying info.
 */
team_struct team =
    {
        /* Team name: Replace with either:
           Your login ID if working as a one person team
           or, ID1+ID2 where ID1 is the login ID of the first team member
           and ID2 is the login ID of the second team member */
        "522031910732",
        /* Student name 1: Replace with the full name of first team member */
        "WANG Yixiao",
        /* Login ID 1: Replace with the login ID of first team member */
        "522031910732",

        /* The following should only be changed if there are two team members */
        /* Student name 2: Full name of the second team member */
        "",
        /* Login ID 2: Login ID of the second team member */
        ""};

#if 0
/*
 * STEP 2: Read the following instructions carefully.
 */

You will provide your solution to the Data Lab by
editing the collection of functions in this source file.

CODING RULES:
 
  Replace the "return" statement in each function with one
  or more lines of C code that implements the function. Your code 
  must conform to the following style:
 
  int Funct(arg1, arg2, ...) {
      /* brief description of how your implementation works */
      int var1 = Expr1;
      ...
      int varM = ExprM;

      varJ = ExprJ;
      ...
      varN = ExprN;
      return ExprR;
  }

  Each "Expr" is an expression using ONLY the following:
  1. Integer constants 0 through 255 (0xFF), inclusive. You are
      not allowed to use big constants such as 0xffffffff.
  2. Function arguments and local variables (no global variables).
  3. Unary integer operations ! ~
  4. Binary integer operations & ^ | + << >>
    
  Some of the problems restrict the set of allowed operators even further.
  Each "Expr" may consist of multiple operators. You are not restricted to
  one operator per line.

  You are expressly forbidden to:
  1. Use any control constructs such as if, do, while, for, switch, etc.
  2. Define or use any macros.
  3. Define any additional functions in this file.
  4. Call any functions.
  5. Use any other operations, such as &&, ||, -, or ?:
  6. Use any form of casting.
 
  You may assume that your machine:
  1. Uses 2s complement, 32-bit representations of integers.
  2. Performs right shifts arithmetically.
  3. Has unpredictable behavior when shifting an integer by more
     than the word size.

EXAMPLES OF ACCEPTABLE CODING STYLE:
  /*
   * pow2plus1 - returns 2^x + 1, where 0 <= x <= 31
   */
  int pow2plus1(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     return (1 << x) + 1;
  }

  /*
   * pow2plus4 - returns 2^x + 4, where 0 <= x <= 31
   */
  int pow2plus4(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     int result = (1 << x);
     result += 4;
     return result;
  }


NOTES:
  1. Use the dlc (data lab checker) compiler (described in the handout) to 
     check the legality of your solutions.
  2. Each function has a maximum number of operators (! ~ & ^ | + << >>)
     that you are allowed to use for your implementation of the function. 
     The max operator count is checked by dlc. Note that '=' is not 
     counted; you may use as many of these as you want without penalty.
  3. Use the btest test harness to check your functions for correctness.
  4. The maximum number of ops for each function is given in the
     header comment for each function. If there are any inconsistencies 
     between the maximum ops in the writeup and in this file, consider
     this file the authoritative source.
#endif

/*
 * STEP 3: Modify the following functions according the coding rules.
 *
 *   IMPORTANT. TO AVOID GRADING SURPRISES:
 *   1. Use the dlc compiler to check that your solutions conform
 *      to the coding rules.
 *   2. Use the btest test harness to check that your solutions produce
 *      the correct answers. Watch out for corner cases around Tmin and Tmax.
 */
/* Copyright (C) 1991-2020 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */
/* This header is separate from features.h so that the compiler can
   include it implicitly at the start of every compilation.  It must
   not itself include <features.h> or any other header that includes
   <features.h> because the implicit include comes before any feature
   test macros that may be defined in a source file before it first
   explicitly includes a system header.  GCC knows the name of this
   header in order to preinclude it.  */
/* glibc's intent is to support the IEC 559 math functionality, real
   and complex.  If the GCC (4.9 and later) predefined macros
   specifying compiler intent are available, use them to determine
   whether the overall intent is to support these features; otherwise,
   presume an older compiler has intent to support these features and
   define these macros by default.  */
/* wchar_t uses Unicode 10.0.0.  Version 10.0 of the Unicode Standard is
   synchronized with ISO/IEC 10646:2017, fifth edition, plus
   the following additions from Amendment 1 to the fifth edition:
   - 56 emoji characters
   - 285 hentaigana
   - 3 additional Zanabazar Square characters */

/*
 * bitCount - returns count of number of 1's in word
 *   Examples: bitCount(5) = 2, bitCount(7) = 3
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 40
 *   Rating: 4
 */
int bitCount(int x)
{
  int mask0 = 0x1 | (0x1 << 4);
  int mask1 = mask0 | (mask0 << 8);
  int mask2 = mask1 | (mask1 << 16);
  int sum = (mask2 & x);
  sum += (mask2 & (x >> 1));
  sum += (mask2 & (x >> 2));
  sum += (mask2 & (x >> 3));
  sum = ((sum >> 16) + sum) & (0xff | (0xff << 8));
  sum = (sum & (0xf | 0xf << 8)) + ((sum >> 4) & (0xf | 0xf << 8));
  return (sum & 0x3f) + ((sum >> 8) & 0x3f);
}
/*
 * copyLSB - set all bits of result to least significant bit of x
 *   Example: copyLSB(5) = 0xFFFFFFFF, copyLSB(6) = 0x00000000
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 5
 *   Rating: 2
 */
int copyLSB(int x)
{
  int tmp = x & 0x1;        // 得到最后一位数
  return (tmp << 31) >> 31; // 利用算数右移
}
/*
 * evenBits - return word with all even-numbered bits set to 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 8
 *   Rating: 2
 */
int evenBits(void)
{
  int mask1 = 0x55;
  int mask2 = mask1 | (mask1 << 8); // 制造辅助数0x5555
  return mask2 | (mask2 << 16);     // 返回数0x55555555
}
/*
 * fitsBits - return 1 if x can be represented as an
 *  n-bit, two's complement integer.
 *   1 <= n <= 32
 *   Examples: fitsBits(5,3) = 0, fitsBits(-4,3) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 15
 *   Rating: 2
 */
int fitsBits(int x, int n)
{
  int a1 = 32 + (~n + 0x1);
  int y = x << a1 >> a1;
  return !(x + (~y + 0x1));
}
/*
 * getByte - Extract byte n from word x
 *   Bytes numbered from 0 (LSB) to 3 (MSB)
 *   Examples: getByte(0x12345678,1) = 0x56
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 6
 *   Rating: 2
 */
int getByte(int x, int n)
{
  /*首先利用+算出需要右移的位数，即8*n */
  int tmp1 = n + n;
  int tmp2 = tmp1 + tmp1;
  int tmp3 = tmp2 + tmp2;
  return (x >> tmp3) & 0xFF;
}
/*
 * isGreater - if x > y  then return 1, else return 0
 *   Example: isGreater(4,5) = 0, isGreater(5,4) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 24
 *   Rating: 3
 */
int isGreater(int x, int y)
{
  int diff = ((!(x >> 31)) & 0x1) ^ ((!(y >> 31)) & 0x1);
  int helpNum = diff << 31 >> 31;
  int minusNum = y + (~x + 1);
  int re1 = (~helpNum) & ((minusNum >> 31) & 0x1);
  int re2 = helpNum & ((!(x >> 31)) & 0x1);
  int re = re1 + re2;
  return re;
}
/*
 * isNonNegative - return 1 if x >= 0, return 0 otherwise
 *   Example: isNonNegative(-1) = 0.  isNonNegative(0) = 1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 6
 *   Rating: 3
 */
int isNonNegative(int x)
{
  return (!(x >> 31)) & 0x1;
}
/*
 * isNotEqual - return 0 if x == y, and 1 otherwise
 *   Examples: isNotEqual(5,5) = 0, isNotEqual(4,5) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 6
 *   Rating: 2
 */
int isNotEqual(int x, int y)
{
  return !(!(x + (~y + 0x1)));
}
/*
 * leastBitPos - return a mask that marks the position of the
 *               least significant 1 bit. If x == 0, return 0
 *   Example: leastBitPos(96) = 0x20
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 6
 *   Rating: 4
 */
int leastBitPos(int x)
{
  int tmp = ~x + 0x1;
  return (x & tmp);
}
/*
 * logicalShift - shift x to the right by n, using a logical shift
 *   Can assume that 1 <= n <= 31
 *   Examples: logicalShift(0x87654321,4) = 0x08765432
 *   Legal ops: ~ & ^ | + << >>
 *   Max ops: 16
 *   Rating: 3
 */
int logicalShift(int x, int n)
{
  int tmp = n + (~0x1 + 0x1);
  int tmp1 = (~(0x1 << 31));
  int tmp2 = tmp1 >> tmp;
  return (x >> n) & tmp2;
}
/*
 * satAdd - adds two numbers but when positive overflow occurs, returns
 *          the maximum value, and when negative overflow occurs,
 *          it returns the minimum value.
 *   Examples: satAdd(0x40000000,0x40000000) = 0x7fffffff
 *             satAdd(0x80000000,0xffffffff) = 0x80000000
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 30
 *   Rating: 4
 */
int satAdd(int x, int y)
{
  int sum = x + y;
  int xhelpNum = (x >> 31) & 0x1;
  int xyIsSame = xhelpNum ^ ((y >> 31) & 0x1);
  int xsIsSame = xhelpNum ^ ((sum >> 31) & 0x1);
  int over = (!xyIsSame) & xsIsSame;
  int negOver = (over & ((!(x >> 31)) & 0x1)) << 31 >> 31;
  int posOver = (over & (xhelpNum)) << 31 >> 31;
  int noOver = ~(posOver | negOver);
  int maxNum = 0x80 << 24;
  int minNum = ~maxNum;
  int re1 = posOver & maxNum;
  int re2 = negOver & minNum;
  int re3 = noOver & sum;
  int re = re1 + re2 + re3;
  return re;
}

/* howManyBits - return the minimum number of bits required to represent x in
 *             two's complement
 *  Examples: howManyBits(12) = 5
 *            howManyBits(298) = 10
 *            howManyBits(-5) = 4
 *            howManyBits(0)  = 1
 *            howManyBits(-1) = 1
 *            howManyBits(0x80000000) = 32
 *  Legal ops: ! ~ & ^ | + << >>
 *  Max ops: 90
 *  Rating: 4
 */
int howManyBits(int x)
{
  int helpNum1, y, num16, mask1, mask2, num8, mask3, mask4, num4, mask5, mask6, num2, mask7, mask8, num1, ret, iszero, isMinusOne, bothNot, retu;
  int tmp16, tmp8, tmp4, tmp2;

  // 容易发现，如果x为正数，那么返回值为x从左向右第一次出现1的位数+1
  // 如果x为负数，那么返回值为~x从左向右第一次出现1的位数+1
  // 特殊的，如果x=0，那么也是第一次出现1的位数（0）+1
  // 所以，我们先将正负值和0进行统一
  helpNum1 = x >> 31;
  y = (helpNum1 & (~x)) | ((~helpNum1) & x);
  // 下面对y进行操作，容易知道，返回值最多是31（0x11111），那么我们先求出权值为16的一位是多少
  num16 = !!(y >> 16);
  // 如果权值为16处为1，那么接下来讨论的范围就是前16位，否则为后16位
  mask1 = 0xff | (0xff << 8);
  mask2 = ~mask1;
  tmp16 = num16 << 31 >> 31;
  y = ((tmp16 & (mask2 & y)) >> 16) | ((~tmp16) & (mask1 & y));
  // 下面我们得到了所要求的16位数y
  // 同样的方法，我们求权值为8处的值并且得到所要求的8位数
  num8 = !!(y >> 8);
  mask3 = 0xff;
  mask4 = (0xff << 8);
  tmp8 = num8 << 31 >> 31;
  y = ((tmp8 & (mask4 & y)) >> 8) | ((~tmp8) & (mask3 & y));
  // 下面我们得到了所要求的8位数y
  // 同样的方法，我们求权值为4处的值并且得到所要求的4位数
  num4 = !!(y >> 4);
  mask5 = 0xf;
  mask6 = 0xf0;
  tmp4 = (num4 << 31 >> 31);
  y = ((tmp4 & (mask6 & y)) >> 4) | ((~tmp4) & (mask5 & y));
  // 下面我们得到了所要求的4位数y
  // 同样的方法，我们求权值为2处的值并且得到所要求的1位数
  num2 = !!(y >> 2);
  mask7 = 0x3;
  mask8 = 0xc;
  tmp2 = (num2 << 31 >> 31);
  y = ((tmp2 & (mask8 & y)) >> 2) | ((~tmp2) & (mask7 & y));
  num1 = !!(y >> 1);
  ret = ((num16 << 4) | (num8 << 3) | (num4 << 2) | (num2 << 1) | num1) + 2;
  iszero = !(x ^ 0x0);
  isMinusOne = !(x ^ (~0x0));
  bothNot = (!(iszero | isMinusOne)) << 31 >> 31;
  retu = (bothNot & ret) | ((~bothNot) & 1);
  return retu;
}

/*
 * logicalNeg - implement the ! operator, using all of
 *              the legal operators except !
 *   Examples: logicalNeg(3) = 0, logicalNeg(0) = 1
 *   Legal ops: ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 4
 */
int logicalNeg(int x)
{
  int tmp = x | (~x + 1);
  return ((~tmp) >> 31) & 0x1;
}

/*
 * dividePower2 - Compute x/(2^n), for 0 <= n <= 30
 *  Round toward zero
 *   Examples: dividePower2(15,1) = 7, dividePower2(-33,4) = -2
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 15
 *   Rating: 3
 */
int dividePower2(int x, int n)
{
  int tmp = x >> n;
  int tmp1 = x >> 31;
  int mask = (~(0x1 << 31)) >> (32 + (~n));
  int tmp2 = !(!(x & mask));
  return tmp + (tmp1 & tmp2);
}

/*
 * bang - Convert from two's complement to sign-magnitude
 *   where the MSB is the sign bit
 *   You can assume that x > TMin
 *   Example: bang(-5) = 0x80000005.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 15
 *   Rating: 4
 */
int bang(int x)
{
  int neg = ~x;
  int helpNum = (x >> 31);
  int neg2=neg+1;
  int tmp1 = x & neg2;
  int tmp0 = helpNum << 31;
  int tmp2 = (neg) & neg2;
  int re1 = (helpNum & (tmp0 | (tmp1 | tmp2)));
  int re2 = (~helpNum) & x;
  int re = re1 + re2;
  return re;
}
