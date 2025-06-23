# MINGW / WIN32

The following section applies to all files within this directory with the file containing this notice.

The files are win32 API and windows corecrt headers because some compilers (notably zig cc without libc) do not have them.

The files were taken from the MinGW project: https://www.mingw-w64.org/ (Specifically release 13.0.0)

The MinGW directory is not a complete collection of the headers, but rather a minimal version of it where any unneeded headers are not included.

This is done by collecting or re-creating the correct MinGW header whenever a missing one is encountered. 

On June 22 2025, this directory had 93 of the 2088 MinGW files, which is 4.5% of the MinGW project by number of files, 1.9% of the MinGW project by number of lines, and 1.7% of MinGW by total file size.

When adding new MinGW headers here, PLEASE be careful of the MinGW licenses. Some of the MinGW code is licensed via LGPL. LGPL code is (to be safe) not allowed to be found within the Pinc repository. MinGW makes use of three licenses:
- LGPL license (Code with this license is not within this repository)
- Zope Public License (ZPL) Version 2.1
- Public Domain + a warranty disclaimer

The licenses are found in DISCLAIMER and DISCLAIMER.PD of this directory, which are copies of the respective files in the MinGW repository.

ADHERENCE TO CLAUSE 1 OF THE ZPL: This readme serves as a documentation of both the origin of the MinGW headers here, as well as their licensing information.

ADHERENCE TO CLAUSE 5 OF THE ZPL; List of modifications from original MinGW sources:
- As stated above, a many of the files are not present to minimize the footprint of Pinc's source code.
- Edits to the contents of a file that are not mentioned here are clearly marked by the string "PINC EDIT:" which can be searched directly.
- Edits to the contents of a file that are marked by the string "PINC EDIT:" are also listed near the start of each file with such edits.
- This includes files licensed under the ZPL as well as public domain, although public domain files are not required to have these markings.

ADDITIONAL NOTE ABOUT THE MINGW LICENSE: the files "AUTHORS", "DISCLAIMER", "DISCLAIMER.PD", and "COPYING" from the relevant release of MinGW are also found within this directory, alongside the headers themselves.
