# linux-rild
Ril Daemon for cellular module (Quectel EC21E & EC25E)

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
$ sudo ./rild -d /dev/ttyUSB2 -v &
```
**B. Execute example**  
```
$ cd example
$ ./example -n 01012345678
```
