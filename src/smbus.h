/*
    smbus.h - A Linux module for reading sensor data.
    Copyright (c) 1998  Frodo Looijaard <frodol@dds.nl>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef SENSORS_SMBUS_H
#define SENSORS_SMBUS_H

/* This file must interface with Simon Vogl's i2c driver. Version 19981006 is
   OK, earlier versions are not; later versions will probably give problems
   too. 
*/
#ifdef I2C
#include "i2c/i2c.h"
#else /* def I2C */
#include <linux/i2c.h>
#endif /* def I2C */

#include <asm/types.h>

/* SPINLOCK is defined in i2c.h. */
#ifdef SPINLOCK
#include <asm/spinlock.h>
#else
#include <asm/semaphore.h>
#endif

/* Declarations, to keep the compiler happy */
struct smbus_driver;
struct smbus_client;
struct smbus_algorithm;
struct smbus_adapter;
union smbus_data;

/* A driver tells us how we should handle a specific kind of chip.
   A specific instance of such a chip is called a client.  
   This structure is essentially the same as i2c_driver. */
struct smbus_driver {
  char name[32];
  int id;
  unsigned int flags;
  int (* attach_adapter) (struct smbus_adapter *);
  int (* detach_client) (struct smbus_client *);
  int (* command) (struct smbus_client *, unsigned int cmd, void *arg);
  void (* inc_use) (struct smbus_client *);
  void (* dec_use) (struct smbus_client *);
};

/* A client is a specifc instance of a chip: for each detected chip, there will
   be a client. Its operation is controlled by a driver.
   This structure is essentially the same as i2c_client. */
struct smbus_client {
  char name[32];
  int id;
  unsigned int flags;
  unsigned char addr;
  struct smbus_adapter *adapter;
  struct smbus_driver *driver;
  void *data;
};

/* An algorithm describes how a certain class of busses can be accessed. 
   A specific instance of sucj a bus is called an adapter. 
   This structure is essentially the same as i2c_adapter. */
struct smbus_algorithm {
  char name[32];
  unsigned int id;
  int (* master_xfer) (struct smbus_adapter *adap, struct i2c_msg msgs[],
                       int num);
  int (* slave_send) (struct smbus_adapter *,char *, int);
  int (* slave_recv) (struct smbus_adapter *,char *, int);
  int (* algo_control) (struct smbus_adapter *, unsigned int, unsigned long);
  int (* client_register) (struct smbus_client *);
  int (* client_unregister) (struct smbus_client *);
};

/* An adapter is a specifc instance of a bus: for each detected bus, there will
   be an adapter. Its operation is controlled by an algorithm. 
   SPINLOCK must be the same as declared in i2c.h.
   This structure is an extension of i2c_algorithm. */
struct smbus_adapter {
  char name[32];
  unsigned int id;
  struct smbus_algorithm *algo;
  void *data;
#ifdef SPINLOCK
  spinlock_t lock;
  unsigned long lockflags;
#else
  struct semaphore lock;
#endif
  unsigned int flags;
  struct smbus_client *clients[I2C_CLIENT_MAX];
  int client_count;
  int timeout;
  int retries;

  /* Here ended i2c_algorithm */
  s32 (* smbus_access) (__u8 addr, char read_write,
                        __u8 command, int size, union smbus_data * data);
};

/* We need to mark SMBus algorithms in the algorithm structure. 
   Note that any and all adapters using a non-i2c driver use in this
   setup ALGO_SMBUS. Adapters define their own smbus access routine. 
   This also means that adapter->smbus_access is only available if
   this flag is set! */
#define ALGO_SMBUS 0x40000

/* This union is used within smbus_access routines */
union smbus_data { 
        __u8 byte;
        __u16 word;
        __u8 block[32];
};

/* smbus_access read or write markers */
#define SMBUS_READ      1
#define SMBUS_WRITE     0

/* SMBus transaction types (size parameter in the above functions) 
   Note: these no longer correspond to the (arbitrary) PIIX4 internal codes! */
#define SMBUS_QUICK      0
#define SMBUS_BYTE       1
#define SMBUS_BYTE_DATA  2 
#define SMBUS_WORD_DATA  3
#define SMBUS_PROC_CALL  4
#define SMBUS_BLOCK_DATA 5

/* Declare an algorithm structure. All SMBus derived adapters should use this
   algorithm! */
extern struct smbus_algorithm smbus_algorithm;

/* This is the very generalized SMBus access routine. You probably do not
   want to use this, though; one of the functions below may be much easier,
   and probably just as fast. */
extern s32 smbus_access (struct smbus_adapter * adapter, u8 addr, 
                         char read_write, u8 command, int size,
                         union smbus_data * data);

/* Now follow the 'nice' access routines. These also document the calling
   conventions of smbus_access. */

extern inline s32 smbus_write_quick(struct smbus_adapter * adapter, u8 addr, 
                                    u8 value)
{
  return smbus_access(adapter,addr,value,0,SMBUS_QUICK,NULL);
}

extern inline s32 smbus_read_byte(struct smbus_adapter * adapter,u8 addr)
{
  union smbus_data data;
  if (smbus_access(adapter,addr,SMBUS_READ,0,SMBUS_BYTE,&data))
    return -1;
  else
    return data.byte;
}

extern inline s32 smbus_write_byte(struct smbus_adapter * adapter, u8 addr, 
                                   u8 value)
{
  return smbus_access(adapter,addr,SMBUS_WRITE,value, SMBUS_BYTE,NULL);
}

extern inline s32 smbus_read_byte_data(struct smbus_adapter * adapter,
                                       u8 addr, u8 command)
{
  union smbus_data data;
  if (smbus_access(adapter,addr,SMBUS_READ,command,SMBUS_BYTE_DATA,&data))
    return -1;
  else
    return data.byte;
}

extern inline s32 smbus_write_byte_data(struct smbus_adapter * adapter,
                                        u8 addr, u8 command, u8 value)
{
  union smbus_data data;
  data.byte = value;
  return smbus_access(adapter,addr,SMBUS_WRITE,command,SMBUS_BYTE_DATA,&data);
}

extern inline s32 smbus_read_word_data(struct smbus_adapter * adapter,
                                       u8 addr, u8 command)
{
  union smbus_data data;
  if (smbus_access(adapter,addr,SMBUS_READ,command,SMBUS_WORD_DATA,&data))
    return -1;
  else
    return data.word;
}

extern inline s32 smbus_write_word_data(struct smbus_adapter * adapter,
                                        u8 addr, u8 command, u16 value)
{
  union smbus_data data;
  data.word = value;
  return smbus_access(adapter,addr,SMBUS_WRITE,command,SMBUS_WORD_DATA,&data);
}

extern inline s32 smbus_process_call(struct smbus_adapter * adapter,
                                     u8 addr, u8 command, u16 value)
{
  union smbus_data data;
  data.word = value;
  if (smbus_access(adapter,addr,SMBUS_WRITE,command,SMBUS_PROC_CALL,&data))
    return -1;
  else
    return data.word;
}

/* Returns the number of read bytes */
extern inline s32 smbus_read_block_data(struct smbus_adapter * adapter,
                                        u8 addr, u8 command, u8 *values)
{
  union smbus_data data;
  int i;
  if (smbus_access(adapter,addr,SMBUS_READ,command,SMBUS_BLOCK_DATA,&data))
    return -1;
  else {
    for (i = 1; i <= data.block[0]; i++)
      values[i-1] = data.block[i];
    return data.block[0];
  }
}

extern inline int smbus_write_block_data(struct smbus_adapter * adapter,
                                         u8 addr, u8 command, u8 length,
                                         u8 *values)
{
  union smbus_data data;
  int i;
  if (length > 32)
    length = 32;
  for (i = 1; i <= length; i++)
    data.block[i] = values[i-1];
  data.block[0] = length;
  return smbus_access(adapter,addr,SMBUS_WRITE,command,SMBUS_BLOCK_DATA,&data);
}

/* Next: define SMBus variants of registering. */
extern int smbus_add_algorithm(struct smbus_algorithm *algorithm);
extern int smbus_del_algorithm(struct smbus_algorithm *algorithm);

extern int smbus_add_adapter(struct smbus_adapter *adapter);
extern int smbus_del_adapter(struct smbus_adapter *adapter);

extern int smbus_add_driver(struct smbus_driver *driver);
extern int smbus_del_driver(struct smbus_driver *driver);

extern int smbus_attach_client(struct smbus_client *client);
extern int smbus_detach_client(struct smbus_client *client);


#endif /* ndef SENSORS_SMBUS_H */

