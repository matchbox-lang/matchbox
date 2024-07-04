# Operator Precedence
| Operator                      | Description       | Associativity |
| :---------------------------- | :---------------- | :------------ |
| ()                            | Group             | N/a           |
| []                            | Array             | Left          |
| * &                           | Address           | N/a           |
| **                            | Exponent          | Right         |
| ++ --                         | Postfix           | N/a           |
| ++ -- ! ~ + -                 | Prefix            | N/a           |
| =~ !~                         | Regex             | Left          |
| * / // %                      | Factor            | Left          |
| + -                           | Term              | Left          |
| << >>                         | Shift             | Left          |
| < <= > >= <=>                 | Comparison        | Left          |
| == !=                         | Equality          | Left          |
| &                             | Bitwise AND       | Left          |
| ^                             | Bitwise XOR       | Left          |
| \|                            | Bitwise OR        | Left          |
| &&                            | Logical AND       | Left          |
| \|\|                          | Logocal OR        | Left          |
| ??                            | Coalescing        | Left          |
| = += -= *= **= /= //= %=      | Assignment        | Right         |
| &= ^= \|= ??=                 | Assignment        | Right         |
| ?:                            | Ternary           | Right         |
| =>                            | Arrow             | N/a           |
| ,                             | Sequence          | Left          |
