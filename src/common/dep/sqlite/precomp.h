// sqlite precomp.h nepouziva, ale overime, ze je v projektu nastaven define pro exporty dll
#ifndef SQLITE_API
#pragma message ( __FILE__ "SQLITE_API not defined!" )
#endif
