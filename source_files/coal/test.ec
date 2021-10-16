// 
// Test script for Coal
//

module sys
{
    function print(s : string) = native
}

function nprint(n : float ) = { sys.print("" + n) }
function vprint(v : vector) = { sys.print("" + v) }
function sprint(s : string) = { sys.print(s) }


var jackpot : string = "JACKPOT!"

constant noobvec = '1 2 3'

function foo(a) =
{
    nprint(a)
}

function bar(b) =
{
    var y = 1 | 4 | 16
    b = b & y

    nprint(b)
    if (b <= 4)
        b = 1
    else
        sprint(jackpot)
}

function factorial(n) : float =
{
    if (n <= 1)
        return n

    return factorial(n - 1) * n
}

function main() =
{
    sprint("------------")
    sprint("Hello World!")
    sprint("------------")
    
    nprint(factorial(5))
    nprint(factorial(4) + factorial(3) + factorial(2))

    vprint(noobvec + '4 5 6')
    vprint(noobvec / 4  )

    sprint("")

    var x = 1; repeat { foo(x); x = x + 1; } until (x > 6)

    sprint("")

    x = 1; while (x <= 6) { bar(x); x = x + 1 }
}

