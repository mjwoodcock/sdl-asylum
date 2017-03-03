#ifndef _FILE_H_
#define _FILE_H_

#define FIND_DATA_READ_ONLY	0x40
#define FIND_DATA_READ_WRITE	0x80
#define FIND_DATA_APPEND	0xC0

int load_data(char** spaceptr, char* filename, char* path);

#endif

