#ifndef __KEY_H
#define __KEY_H

void key_init();
u8  key_read();
u8 string_check(u8* string1,u8* string2,u8 len);
u8 rfid_chek();
void string_copy(u8* string1, u8* string2, u8 len);
void key_clear();
void key_task ();


#endif