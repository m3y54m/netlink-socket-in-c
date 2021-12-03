# Netlink Socket in C


## Build

```console
mkdir build
cd build
cmake ..
make
```

If you want to enable debugging in the build output, use the following command instead of `cmake ..`

```console
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

## Run

To run the output executable file you need root permissions:

```console
sudo ./main
```

## Resources

- [Monitoring Linux networking state using netlink](https://olegkutkov.me/2018/02/14/monitoring-linux-networking-state-using-netlink/)
- [linux process monitoring (exec, fork, exit, set*uid, set*gid)](https://bewareofgeek.livejournal.com/2945.html)
- [The Proc Connector and Socket Filters](https://nick-black.com/dankwiki/index.php/The_Proc_Connector_and_Socket_Filters)
- [Detect new process creation instantly in linux](https://stackoverflow.com/q/26852228/5000746)
- [How do you set GDB debug flag with cmake?](https://stackoverflow.com/questions/10005982/how-do-you-set-gdb-debug-flag-with-cmake)
- [sigaction() — Examine or change a signal action](https://pubs.opengroup.org/onlinepubs/9699919799/functions/sigaction.html)
- [Termination Signals](https://www.gnu.org/software/libc/manual/html_node/Termination-Signals.html)