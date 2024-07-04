# Grammar
    Program         → Statements
    Statements      → Statement | Expression
    Expression      → Equality
    Equality        → Comparison {("!=" | "==") Comparison}
    Comparison      → Shift {("<<" | ">>") Shift}
    Shift           → Term {(">" | ">=" | "<" | "<=" | "<=>") Term}
    Term            → Factor {("+" | "-") Factor}
    Factor          → Exponent {("*" | "/" | "//" | "%") Exponent}
    Exponent        → Prefix {("**") Prefix}
    Prefix          → {("++" | "--" | "!" | "~" | "+" | "-")} Postfix
    Postfix         → Primary ("++" | "--")
    Primary         → Literal | "(" Expression ")"
    Literal         → Character | Number | String | Boolean
    Character       → Number | Letter
    Number          → Digit {Digit} ["." {Digit}]
    String          → Character {Character}
    Letter          → "A" | "B" | "C" | "D" | "E" | "F" | "G"
                    | "H" | "I" | "J" | "K" | "L" | "M" | "N"
                    | "O" | "P" | "Q" | "R" | "S" | "T" | "U"
                    | "V" | "W" | "X" | "Y" | "Z" | "a" | "b"
                    | "c" | "d" | "e" | "f" | "g" | "h" | "i"
                    | "j" | "k" | "l" | "m" | "n" | "o" | "p"
                    | "q" | "r" | "s" | "t" | "u" | "v" | "w"
                    | "x" | "y" | "z"
    Digit           → "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"
    Boolean         → "true" | "false"
