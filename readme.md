# Overview
Make sure to disable smep/smap.

You need to manually get the address of modprobe.

Recommend using Ubuntu 16.04 and then recompiling the kernel with the following configurations
- nosmap
- nosmep
- nokaslr
- CONFIG_MODULES=y

# Build

To build the kernel module
```
make
```

To clean
```
make clean
```

To install the kernel module
```
make install
```

To remove the kernel module
```
make remove
```

To build the exploit
```
make build_exploit
```

To exploit
```
make exploit
```
