# Standard Library

## class Array\<T\>
    compact         Return an array of variables and their values
    current         Return the current element of an array
    empty           Determine if an array is empty
    end             Set internal pointer to the end of an array
    extract         Import variables into the current symbol table from an array
    first           Return the first element of an array
    get             Return the value at the index
    join            Join elements of an array into a string
    key             Return the current index of an array
    keys            Return the keys of an array
    last            Return the last element of an array
    length          Return the length of an array
    map             Apply a function to all elements in an array
    merge           Merge the elements of one or more arrays
    next            Advance internal pointer of an array
    pop             Remove the last element of an array
    prev            Rewind internal pointer of an array
    push            Add new elements onto the end of an array
    rand            Return a randon element from an array
    reset           Reset internal pointer to the beginning of an array
    rsort           Sort an array in descending order
    shift           Remove an element from the beginning of an array
    shuffle         Shuffle an array
    slice           Extract one or more elements from an array
    sort            Sort an array in ascending order
    splice          Replace one or more elements in an array
    split           Split a string into an array of substrings
    unique          Removes duplicate values from an array
    unshift         Prepend an element to the beginning of an array
    values          Return the values of an array

## class Map\<K, V\>
    ksort           Sort a map by key in ascending order
    krsort          Sort a map by key in descending order
    rsort           Sort a map in descending order
    sort            Sort a map in ascending order

## class Set\<T\>
    rsort           Sort a set in descending order
    sort            Sort a set in ascending order

## class String
    capitalize      Convert the first character to uppercase
    length          Return the length of a string
    lower           Convert a string to lowercase
    ltrim           Trim whitespace from the left side of a string
    rtrim           Trim whitespace from the right side of a string
    strpos          Return the position of the first occurrence of a string
    strstr          Return the first occurrence of a string
    strtok          Tokenize a string using the delimiter
    substr          Return part of a string
    swapcase        Swap the case of each character in a string
    trim            Trim whitespace from a string
    title           Convert the first character of each word to uppercase
    upper           Convert a string to uppercase

## struct Complex\<T\>
    real            Real number
    imaginary       Imaginary number

## enum Optional\<T\>
    unwrap          Unwrap and return the associated value

## conversion
    bool            Return the value as an boolean
    char            Return the value as a Unicode character
    double          Return the value as an 64-bit floating-point
    float           Return the value as an 32-bit floating-point
    int             Return the value as an integer
    int8            Return the value as an 8-bit integer
    int16           Return the value as an 16-bit integer
    int32           Return the value as an 32-bit integer
    int64           Return the value as an 64-bit integer
    string          Return the value as a string
    uint            Return the value as an unsigned integer
    uint8           Return the value as an unsigned 8-bit integer
    uint16          Return the value as an unsigned 16-bit integer
    uint32          Return the value as an unsigned 32-bit integer
    uint64          Return the value as an unsigned 64-bit integer

## directory
    chdir           Change the current directory
    chroot          Change the root directory
    closedir        Close directory handle
    getcwd          Return the current working directory
    opendir         Open a directory handle
    readdir         Return the next entry in the directory
    scandir         List files and directories

## file
    chmod           Return file mode bits
    chown           Return ownership
    close           Close a file
    open            Open a file
    read            Read data from a file
    remove          Remove a file
    rename          Rename a file
    readline        Return one line from a file
    seek            Set the file position
    rewind          Set the file position to the beginning
    tell            Return the current file position
    write           Write to a file

## io
    input           Read input from stdin
    inputf          Read formatted input from stdin
    print           Send output to stdout
    printf          Send formatted output to stdout

## math
    abs             Return the absolute value
    avg             Return the average of two numbers
    acos            Return the arc cosine
    acosh           Return the hyperbolic arc cosine
    asin            Return the arc sine
    asinh           Return the arc sine arc sine
    atan            Return the arc tangent
    atan2           Return the arc tangent of its arguments
    atanh           Return the hyperbolic arc tangent
    cbrt            Return the cube root
    ceil            Return the value of a number rounded up to the nearest integer
    clamp           Clamp a number between two values
    cos             Return the cosine
    cosh            Return the hyperbolic cosine
    deg             Convert from radians to degrees
    exp             Return the exponent of e
    floor           Return the value of a number rounded down to the nearest integer
    frexp           Return the mantissa and exponent of a number
    hypot           Return the length of the hypotenuse
    ilerp           Return where a value lies between two points
    ldexp           Return a number multiplied by 2 raised to the power of exponent
    lerp            Interpolate between two numbers
    log             Return the natural logarithm
    log10           Return the base-10 logarithm
    log2            Return the base-2 logarithm
    max             Return the maximum of two numbers
    min             Return the minimum of two numbers
    pi              Return the value of PI (3.14159265, etc.)
    rad             Convert from degrees to radians
    rand            Generate a random number
    round           Return the value of a number rounded to n precision
    sign            Return the sign of a number
    sin             Return the cosine
    sinh            Return the hyperbolic cosine
    sum             Return the sum of one or more numbers
    sqrt            Return the square root
    tan             Return the tangent
    tanh            Return the hyperbolic tangent

## sequence
    range           Generate a sequence of numbers
    zip             Zip sequences into a single iterator

## system
    endianess       Return the endianess of the system
    exit            Terminate the current process

## time
    date            Format a timestamp
    sleep           Pause execution for n seconds
    time            Return the current timestamp
    timer           Call a function after n seconds
