# waywalker
Wayland compositor demo built on wlroots

This demo reuses code from Drew DeVaults project [mcwaface](https://github.com/ddevault/mcwayface/).

## Compiling

```shell
mkdir builddir
cd builddir
meson ..
ninja
```

## Running
This will produce a binary named `waywalker`, this is your compositor. Run it
without any arguments or with `-s command` to specify a startup command like
a terminal emulator.

```shell
./waywalker -s xterm
```

