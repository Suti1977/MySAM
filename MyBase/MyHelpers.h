#ifndef MY_HELPERS_H
#define MY_HELPERS_H

//Tomb elemszamait adja vissza
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) 	(sizeof(array) / sizeof(array[0]))
#endif

//Struktura egy elemenek meretet adja vissza
#define SIZEOF_MEMBER(type, member) sizeof(((type *)0)->member)

#endif //MY_HELPERS_H

