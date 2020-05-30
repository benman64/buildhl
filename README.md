# buildhl

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