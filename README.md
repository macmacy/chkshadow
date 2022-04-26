# chkshadow

## Note

The check this program performs is purely heuristic. There is no guarantee that the method used here will work 100%, or
even if it is correct at all. Use at your own risk.

## Description

Simple program that heuristically checks whether given Windows session is being shadowed.

In Window, you can connect to other user session on the same Terminal Server with `mstsc.exe /shadow:N` command, where
"N" is Windows session identifier.

When you accept the request, your session is viewed (or controlled) by other user as long as shadow session is not terminated.
I was found, however, no easy way to verify whether the session is being shadowed.

I observed, that when shadow is in progress, there is `RdpSa.exe` program running in the session being shadowed.
Checking if this program is running, however, is not enough to tell whether your session is being shadowed.
This is because, it keeps running even if you reject shadow request.

I noticed that when shadow-request is accepted, RdpSa.exe program creates named system semaphore which name begins with `RDPSchedulerEvent`.
I wrote this simple program that checks for this system object using NtOpenDirectoryObject and NtQueryDirectoryObject Win API functions.


## Building

Run Visual Studio Command Prompt (I tried with 2010 and 2015) and simply run:

`cl chkshadow.cpp`

## Sample Usage

Perform the check for current windows session:

`chkshadow`

Check session number 7:

`chkshadow 7`

Same as above, but with more print-outs:

`chkshadow 7 v`
