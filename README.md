# TinySDLogger

This is a logger library for Arduino to write log records on SD card. The main goal for this library is the minimum size of consumed memory (both RAM and CODE).
TinySDLogger library code based on Petit FS by ChaN, but more than 80% of code reworked. Petit FS can't append file, which does not allow to use it for logger functions.
TinySDLogger uses software SPI, because partial writing approach requires hold of CS pin till write 512 bytes (sector size), and it is not possible to share Arduino hardware SPI with other devices. From the another hand, it makes more flexibility to use it in existing projects. 
To minimize code size, library supports only FAT32 filesystem.
Since library does not use in memory sector buffer (512 bytes), it is not possible to make partial changes in sectors. To make possible to append log file and keep FAT consistent, TinySDLogger implements the following approaches:
- Use first 32 bytes (size of file record) of second sector of root directory to store log file
- Use +128 sectors offset to store log file data (512 size of sector / 4 bytes per FAT32 record)
- Use consequent allocation of sectors for log file. Benefit of this approach is that we do not need to search for free sector to append file, and do not do full scan during open file to find its last sector 

TinySDLogger derived from Print class, which allows to use familiar print methods.

NOTE: Do not call close() method after each write operation (unless your logs are too rare and is it crucial to not loose them).
It is better to call close() method by timer (but do not use interrupts for this!), or after write N records, or after end of log session.
Maximum size of log which can be loss (when MCU shutdown without call close() method) - is last 511 bytes. 

# SD Card preparation
Before first usage, SD card must be prepared:
1. Format SD card with FAT32
2. Create 16 empty files (any names)
3. Delete files
These steps must be done only once.

# Consumed memory
Measurement of memory consumption was done by the following steps:
- Use Arduino 1.8.9 + ArduinoNano board
- Simplest sketch with serial print: ROM: 1532 bytes, RAM: 188 bytes
- TinySDLogger without RTC: ROM : 6446 bytes (diff is 4914 bytes), RAM: 261 bytes (diff is 73 bytes)
- TinySDLogger with RTC: ROM : 8298 bytes (diff is 6766 bytes), RAM: 471 bytes (diff is 283 bytes)

Finally, TinySDLogger without RTC requires ~5K code size and 73 bytes of RAM.
with RTC: ~7K code and 283 bytes of RAM. (almost size is because of RTC library itself)

# Performance
Performance measured on ArduinoNano borad.
- TinySDLogger without RTC: 1000 records (39 bytes per record) - 14.5sec
- TinySDLogger with RTC: 1000 records / (59 bytes per record) - 20.5sec

Result writing speed is ~2.7Kb per second.

# Limitations
- Support only SD card with FAT32 filesystem
- Support only one log file
- It is not possible to store any other files on this SD card. (they will be corrupted by logger!)
- Close file method fills remaining bytes (to round up to 512 bytes) with spaces and line feed in the end. This may result in gaps between log sessions

# Test
Verification done using Arduino Nano + Data logging board from Deek-Robot + 4Gb SD card
