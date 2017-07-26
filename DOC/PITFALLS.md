Pitfalls and things not to assume
-----------------------------------

Posix
------

This is not POSIX compliant. It is not intended to be, since this
contorts the shell into working in a certain manner, and in practice,
/bin/sh usually means /bin/bash.

Long story short, use your /bin/sh interpreter for POSIX compliance.
This isn't intended to fill that role.

Subcommands
------------

Subcommands may very well look like the subshell "$()" / "`" operator
in bash. They are not.

Commands are resolved in dependency order until no more replacements are
needed. This is contrary to how bash, zsh, etc work; they literally
spawn another shell to handle anything within. To explain with graphs:

bash: echo $(echo hello $(echo world) ho)

echo $
    echo hello $ ho
        echo world

> hello world ho

ysh:  echo {echo hello {echo world} ho}

echo {echo hello $ ho}
    echo world
echo $
    echo hello world ho

> hello world ho

This behavior has a few important things to note about it.

 1) Subcommands inherit the environment of the shell, not
    a "parent" subcommand.

 2) Subcommands do not spawn another shell, or if they are
    builtin, they do not even spawn another process.

 3) Due to points 1 and 2 above, subcommand environment must be
    carefully handled since unless you have created a subcommand
    like {ysh -c "command"} it will NOT explicitly spawn another
    process.

 4) Output from subcommands are stripped of the last trailing '\n'
    to prevent odd formatting.

Variables
----------

* NOTE: Subject to change, since this feature is not fully implemented
        yet.

Variables (starting with the $ character) are resolved after subcommands
and double-quoted strings.
Consider the following:

= NAME 42
echo "${echo NAME}"

So first, echo NAME is evaluated. The contents of the double-quoted
string are then evaluated, leaving a string named "$NAME". This is then
scanned for variables, resulting in the replacement of $NAME -> 42, and
thus the output is 42.
