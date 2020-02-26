# linux-rild
Ril Daemon for cellular module

# 1. Build ril daemon 
```
$ cd rild
$ make
```
# 2. Build ril library
```
$ cd libs
$ make
$ sudo cp librilso /usr/lib/
```
# 3. Build example
```
$ cd example
$ make
```
# 4. Test
**A. Execute rild daemon**  
```
$ cd rild
$ sudo ./rild -dbg -at /dev/ttyUSB2 &
```
**B. Execute example**  
```
$ cd example
$ ./example
```
