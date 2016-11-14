vt emulation
------------

* double-height support
* implement blinking cursor
* implement reset and set of cursor color (Cr=\E]112\007, Cs=\E]12;%p1%s\007)

code & interface
----------------

* add a simple way to do multiplexing

drawing
-------
* add diacritics support to xdraws()
	* switch to a suckless font drawing library
* make the font cache simpler
* add better support for brightening of the upper colors

bugs
----

* fix shift up/down (shift selection in emacs)
* remove DEC test sequence when appropriate
* fix "tput flash"

misc
----

    $ grep -nE 'XXX|TODO' st.c

