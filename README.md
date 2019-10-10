# chkshadow
Simple program that heuristically checks whether given Windows session is being shadowed.

In Window, you can connect to other user session on the same Terminal Server with "mstsc.exe /shadow:N" command, where
"N" is Windows session identifier.

When you accept the request, your session is viewed (or controlled) by other user as long as shadow session is not terminated.
I found, however, no easy way to verify the session is being shadowed.

I observed, that when shadow is in progress, there is RdpSa.exe program running in the session being shadowed.
Checking if this program is running, however, is not enough to tell whether your session is being shadowed.
This is because, it keeps running even if you reject shadow request.

I noticed that when shadow-request is accepted, RdpSa.exe program creates named system semaphore which name begins with "RDPSchedulerEvent".
If written a simple program that checks for this system object using NtOpenDirectoryObject and NtQueryDirectoryObject Win API functions.



