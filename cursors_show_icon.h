/*
    File:       ShowInitIcon.h

    Contains:   

    Written by: Peter N Lewis, Jim Walker and Francois Pottier  

    Copyright:  Copyright © 1995-1999 by Apple Computer, Inc., All Rights Reserved.

                You may incorporate this Apple sample source code into your program(s) without
                restriction. This Apple sample source code has been provided "AS IS" and the
                responsibility for its operation is yours. You are not permitted to redistribute
                this Apple sample source code as "Apple sample source code" after having made
                changes. If you're going to re-distribute the source, we require that you make
                it clear in the source that the code was descended from Apple sample source
                code, but that you've made changes.

    Change History (most recent first):
                8/10/1999   Karl Groethe    Updated for Metrowerks Codewarror Pro 2.1

*/
#ifndef __ShowInitIcon__
#define __ShowInitIcon__

#include <Types.h>

// Usage: pass the ID of your icon family (ICN#/icl4/icl8) to have it drawn in the right spot.
// If 'advance' is true, the next INIT icon will be drawn to the right of your icon. If it is false, the next INIT icon will overwrite
// yours. You can use it to create animation effects by calling ShowInitIcon several times with 'advance' set to false.

#ifdef __cplusplus
extern "C" {
#endif

pascal void ShowInitIcon (short iconFamilyID, Boolean advance);

#ifdef __cplusplus
}
#endif

#endif /* __ShowInitIcon__ */