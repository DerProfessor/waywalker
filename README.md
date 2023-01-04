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

This will produce a binary named `mcwayface`, this is your compositor. Run it
without any arguments.
