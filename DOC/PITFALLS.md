Pitfalls and things not to assume
-----------------------------------

Posix
------

This is not POSIX compliant. It is not intended to be, since this
contorts the shell into working in a certain manner, and in practice,
/bin/sh usually means /bin/bash.

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

xsh:  echo {echo hello {echo world} ho}

echo {echo hello $ ho}
    echo world
echo $
    echo hello world ho

This behavior has a few importan6 things to note about it.
 1) Subcommands inherit the environment of the shell, not
    a "parent" subcommand.
 2) Subcommands do not spawn another shell.

Variables
----------

Variables (starting with the $ character) are resolved after subcommands
and double-quoted strings.
Consider the following:

= NAME 42
echo "${echo NAME}"

So first, echo NAME is evaluated. The contents of the double-quoted string
are then evaluated, leaving a string named "$NAME". This is then interpreted to
be a variable, resulting in the output 42.
