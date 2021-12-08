# psp2rela

The module relocation config optimization tool.

Optimize the relocation config contained in modules created with the Vita SDK or any SDK derived from them, or any other SDK.

# Build

## Requires

- Ubuntu or WSL2 or a machine that can build this project

- Install zlib library

When you have everything you need, run `./build.sh` and wait for the build to complete.

# How to use

Specify the `-src` and `-dst` arguments on the command line.

- `-src=`:input module path. As elf/self format.

- `-dst=`:output module path. Specifying `-dst=` is optional, and if only `-src=` is used, only the conversion log will be output and the converted module will not be saved.

Example : `./psp2rela -src=./input_module.self -dst=./output_module.self`

You can specify the flag with `-flag=`.

- `v`:Show more logs depending on the number. `1:error/2:warn/3:info/4:debug/5:trace`

- `f`:Save the log to a file. The log file path can be specified with `-log_dst=`

- `s`:Parses the rel config of the module specified by src and outputs it to the log. Also with this option, `-dst=` is ignored and only log output is available.

Example : `./psp2rela -src=./input_module.self -dst=./output_module.self -flag=vvvf -log_dst=./log.txt`

# Information

A. VitaSDK uses type0 only.

B. Official external SDK uses only type0 and type1.

C. Official internal SDK uses all.

type0 is 12-bytes, type1 is 8-bytes.

This tool should work well with the A and B SDKs and reduce the module size.

The ones created in the C SDK are already fully optimized because they are already using all types, but this tool may be able to further compress the rel config.

### TODO

- [ ] Check the validity of the module more.
- [x] Optimize code more

It has been tested with some modules, but it may still be buggy and not working. In that case, please open issues.
