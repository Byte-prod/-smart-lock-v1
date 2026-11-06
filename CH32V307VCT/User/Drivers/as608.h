#ifndef __AS608_H
#define __AS608_H

/* AS608 Fingerprint Module Driver API */

void ps_getimage(void);
void ps_genchar(u8 buffer);
void ps_regmodel(void);
void ps_storeChar(u8 addr);
void ps_search(void);
void PS_Empty(void);

void ps_wait(void);      /* wait for sensor response */
void as608_init(void);   /* GPIO init (PA1 touch) */
void as608_task(void);   /* scheduler task */

#endif
