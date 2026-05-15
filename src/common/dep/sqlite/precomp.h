// sqlite precomp.h is not used, but verify that the project defines the DLL export macro
#ifndef SQLITE_API
#pragma message ( __FILE__ "SQLITE_API not defined!" )
#endif
