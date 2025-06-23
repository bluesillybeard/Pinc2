# ext directory

This directory is added to the include path of the Pinc library. It contains headers for external libraries - the headers are here so they don't have to be installed or managed externally.

This ext folder is added as an include path when building Pinc.

This markdown document contains technical and legal information (mostly legal information) regarding the use and distribution of these headers.

## SDL2

SDL2 headers - SDL2 is ABI stable and is actually heading towards deprecation, so these should never need to be updated.

Modifications from base SDL2 headers:

ADHERENCE TO SDL2 LICENSE: SDL2 is licensed the same way Pinc is, which means it is fully compatible with the Pinc project. This is not legal advice, but it's probably safe to treat SDL2 headers here as part of Pinc itself.

