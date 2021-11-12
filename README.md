# buildhl

A tool that highlights builds and can rewrite output for better CTRL+CLICK
for IDE's that support it in terminals.


supports windows, mac, linux

# Install

## Homebrew

1 line:

```
brew install benman64/tap/buildhl
```

or you can also add the tap & install

```
brew tap benman64/tap
brew install buildhl
```

## From source

```
mkdir build
cd build
cmake ../
make
make install
```

# Examples

example usage

```
my-build-command | buildhl -
```

If it's a cmake project you can just run


```
buildhl

# run a target
buildhl my-target
buildhl test
```

The command will create a `build/release` folder. You can do debug by specifying
debug like so

```
buildhl debug
# a specific target
buildhl debug my-target
buildhl debug test
```
