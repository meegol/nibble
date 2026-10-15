#!/bin/sh
# Scripted code editor tests: runs build/shot on a small cart and compares the edited code.
set -u
tmp=build/editor_test
mkdir -p $tmp
pass=0 fail=0

# t NAME INPUT EXPECTED STEPS...
t() {
    name=$1 input=$2 expected=$3
    shift 3
    printf '%b' "$input" > $tmp/in.nib
    got=$(SHOT_CODE=1 build/shot $tmp/in.nib $tmp/out.png "$@"; echo .)
    got=${got%.}
    want=$(printf '%b.' "$expected")
    want=${want%.}
    if [ "$got" = "$want" ]; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        printf 'FAIL %s\nwant:\n%s\ngot:\n%s\n\n' "$name" "$want" "$got"
    fi
}

t "type" "" "abc" "text:abc"
t "indent after colon" "if a:" "if a:\n  x" key:End key:Return text:x
t "keep indent" "  x = 1" "  x = 1\n  y" key:End key:Return text:y
t "colon with trailing space" "if a: " "if a: \n  x" key:End key:Return text:x
t "backspace dedents" "if a:\n    " "if a:\n  " key:Down key:End key:Backspace
t "backspace char" "abc" "ab" key:End key:Backspace
t "delete" "abc" "bc" key:Delete
t "tab" "x" "  x" key:Tab
t "shift tab" "    x" "  x" shift+key:Tab
t "undo typing" "" "" "text:a" "text:b" ctrl+key:Z
t "redo" "" "ab" "text:a" "text:b" ctrl+key:Z ctrl+key:Y
t "undo newline separately" "" "ab" "text:ab" key:Return ctrl+key:Z
t "select all delete" "a\nb\nc" "" ctrl+key:A key:Backspace
t "shift select" "abcd" "cd" shift+key:Right shift+key:Right key:Delete
t "indent selection" "a\nb" "  a\n  b" ctrl+key:A key:Tab
t "home to indent" "  ab" "  xab" key:End key:Home text:x
t "up keeps column" "abcd\nx\nabcd" "abcd\nx\nabZcd" key:Right key:Right key:Down key:Down text:Z
t "crlf stripped" "a\r\nb" "a\nbX" key:Down key:End text:X
t "click places cursor" "abc\ndef" "abc\ndXef" click:6,15 text:X

# s NAME INPUT EXPECTED STEPS... -- sprite editor; compares the saved data sections
s() {
    name=$1 input=$2 expected=$3
    shift 3
    printf '%b' "$input" > $tmp/in.nib
    SHOT_SAVE=$tmp/saved.nib build/shot $tmp/in.nib $tmp/out.png ctrl+key:Tab "$@"
    got=$(sed -n '/^__/,$p' $tmp/saved.nib | sed 's/0*$//')
    want=$(printf '%b\n' "$expected" | sed 's/0*$//')
    if [ "$got" = "$want" ]; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        printf 'FAIL %s\nwant:\n%s\ngot:\n%s\n\n' "$name" "$want" "$got"
    fi
}

# canvas pixel (x, y) of the 8x8 view is at screen 2+8x, 9+8y; palette color c at 70+14(c%4), 9+8(c/4)
s "paint" "" "__gfx__\n00000000c" click:3,10
s "pick color then paint" "" "__gfx__\n00000000002" click:98,10 click:19,10
s "right click picks color" "__gfx__\n000000000500" "__gfx__\n0000000055" rclick:11,10 click:3,10
s "fill" "" "__gfx__\n00000000aaaaaaaa\n00000000aaaaaaaa\n00000000aaaaaaaa\n00000000aaaaaaaa\n00000000aaaaaaaa\n00000000aaaaaaaa\n00000000aaaaaaaa\n00000000aaaaaaaa" click:98,26 key:F click:30,30
s "flip h" "__gfx__\n000000001" "__gfx__\n0000000000000001" key:H
s "flip v" "__gfx__\n000000001" "__gfx__\n0\n0\n0\n0\n0\n0\n0\n000000001" key:V
s "flag" "" "__gff__\n0004" click:84,53
s "undo" "" "" click:3,10 ctrl+key:Z
s "redo" "" "__gfx__\n00000000c" click:3,10 ctrl+key:Z ctrl+key:Y
s "clear" "__gfx__\n0000000012345678" "" key:Delete
s "copy paste" "__gfx__\n0000000012" "__gfx__\n000000001200000012" ctrl+key:C key:] ctrl+key:V

echo "ok $pass, failed $fail"
[ $fail -eq 0 ]
