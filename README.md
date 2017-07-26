ysh
-----

Yeah, it's a shell.

While this is a toy project that's been going on for less than a week, I plan to continue it due to the interesting technical challenges[1] a shell provides in implementation.

Goals of this shell include:

 * Keeping code simple when possible[2]

 * When the above is impossible, documenting well what the heck is going on

 * Allowing to express input in the most compact way possible[3]

 * Being light on memory and not requiring odd libc "features"[4]

The last goal (compactness) is at odds with POSIX-sh-compliance, so this shell makes no attempt to be usable as /bin/sh.

-----------

[1] Subprocess spawning (such as bash's "$(...)" and "`...`" constructs) is one hell of a thing. You'd be surprised at how much something simple like evaluation of strings increases complexity of a parser; or rather, it forces parsing and execution into the same phase.

[2] Litmus test for simple: if you look at the relevant code a week later, do you know what it's doing? If the answer isn't a resounding "YES", it needs comments and potentially documentation. If the code looks like garbage, it needs a refactor. Simple doesn't mean stupid. It means not adding uneeded complexity.

[3] Yes, this is contrary to the goals within the source code of the shell. Shell scripts and commands may end up looking like a clusterfuck, but shorter than POSIX sh equivalents. This is intentional.

[4] Namely, avoidance of glibc-isms. GNU extensions are barred from usage. This shell is tested against musl and glibc but hopefully random embedded libc X will work as well.
