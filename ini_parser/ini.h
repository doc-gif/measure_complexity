#ifndef INI_H
#define INI_H

#include <stdio.h>

typedef struct dictionary {
    unsigned        n ;
    size_t          size ;
    char        **  val ;
    char        **  key ;
} dictionary ;

void dictionary_del(dictionary * vd);

#endif

