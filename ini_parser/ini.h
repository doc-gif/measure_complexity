#include <stdio.h>

typedef struct dictionary {
    unsigned        n ;
    size_t          size ;
    char        **  val ;
    char        **  key ;
} dictionary ;

void dictionary_del(dictionary * vd);
