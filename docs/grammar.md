# Grammar
    Program         → {Statement}
    Statement       → Expression
    Expression      → Assignment
    Assignment      → Equality { ("=" | "+=" | "-=" | "*=" | "**=" | "/=" | "//=" | "%=") Assignment }
    Equality        → Comparison {("==" | "!=" | "===" | "!==") Comparison}
    Comparison      → Shift {(">" | ">=" | "<" | "<=" | "<=>") Shift}
    Shift           → Term {("<<" | ">>") Term}
    Term            → Factor {("+" | "-") Factor}
    Factor          → Exponent {("*" | "/" | "//" | "%") Exponent}
    Exponent        → Prefix ["**" Exponent]
    Prefix          → {("+" | "-" | "!" | "~")} Primary
    Primary         → Literal | "(" Expression ")"
    Literal         → CharLiteral | String | Number | Boolean
    Character       → <Unicode scalar value except '"', "'", and '\'>
    CharLiteral     → "'" (Character | Escape) "'"
    String          → '"' {Character | Escape} '"'
    Escape          → "\" ("n" | "t" | "\"" | "'" | "\" | "u{" Hex {Hex} "}")
    Number          → Digit {Digit} ["." Digit {Digit}]
    Boolean         → "true" | "false"
    Digit           → "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"
    Hex             → Digit | "A" | "B" | "C" | "D" | "E" | "F"
                            | "a" | "b" | "c" | "d" | "e" | "f"
