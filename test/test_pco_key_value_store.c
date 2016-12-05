/******************************************************************************
 * Project         persistence key value store
 * (c) copyright   2014
 * Company         XS Embedded GmbH
 *****************************************************************************/
/******************************************************************************
 * This Source Code Form is subject to the terms of the
 * Mozilla Public License, v. 2.0. If a  copy of the MPL was not distributed
 * with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
******************************************************************************/
/**
* @file           persistence_common_object_test.c
* @ingroup        persistency
* @author         Simon Disch
* @brief          test of persistence key value store
* @see
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>     /* exit */
#include <time.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <dlt/dlt.h>
#include <dlt/dlt_common.h>
#include <../inc/protected/persComRct.h>
#include <../inc/protected/persComDbAccess.h>
//#include <../test/pers_com_test_base.h>
//#include <../test/pers_com_check.h>
#include <check.h>
#include <sys/wait.h>

#define BUF_SIZE     64
#define NUM_OF_FILES 3
#define READ_SIZE    1024
#define MaxAppNameLen 256

/// application id
char gTheAppId[MaxAppNameLen] = { 0 };

// definition of weekday
char* dayOfWeek[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };



void data_setup(void)
{
   //ssd
   DLT_REGISTER_APP("PCOt", "tests the persistence common object library");
}

void data_teardown(void)
{
   DLT_UNREGISTER_APP();
}



START_TEST(test_OpenLocalDB)
{
   int k=0, handle =  0;
   int databases   = 20;
   char path[128];
   int handles[100] = { 0 };

   persComDbgetMaxKeyValueSize();

   //Cleaning up testdata folder
   remove("/tmp/open-localdb.db");
   remove("/tmp/open-write-cached.db");
   remove("/tmp/open-consecutive.db");
   remove("/tmp/open-write-through.db");

   int ret = 0;
   int ret2 = 0;

   ret = persComDbOpen("/tmp/open-localdb.db", 0x0); //Do not create test.db / only open if present (cached)
   fail_unless(ret < 0, "Open open-localdb.db works, but should fail: retval: [%d]", ret);

   ret = persComDbOpen("/tmp/open-localdb.db", 0x0); //Do not create test.db / only open if present
   fail_unless(ret < 0, "Open open-localdb.db works, but should fail: retval: [%d]", ret);

   ret = persComDbOpen("/tmp/open-localdb.db", 0x1); //create test.db if not present (cached)
   fail_unless(ret >= 0, "Failed to create non existent lDB: retval: [%d]", ret);

   ret = persComDbClose(ret);
   if (ret != 0)
   {
      printf("persComDbClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close database: retval: [%d]", ret);

   //test to use more than 16 static handles
   for (k = 0; k < databases; k++)
   {
      snprintf(path, 128, "/tmp/handletest-%d.db", k);
      //Cleaning up testdata folder
      remove(path);
   }

   for (k = 0; k < databases; k++)
   {
      snprintf(path, 128, "/tmp/handletest-%d.db", k);
      handle = persComDbOpen(path, 0x1); //create test.db if not present
      handles[k] = handle;
      fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);
   }

   //printf("closing! \n");
   for (k = 0; k < databases; k++)
   {
      ret = persComDbClose(handles[k]);
      if (ret != 0)
      {
         printf("persComDbClose() failed: [%d] \n", ret);
      }
      fail_unless(ret == 0, "Failed to close database with number %d: retval: [%d]", k, ret);
   }

   //Test two consecutive open calls
   ret =  persComDbOpen("/tmp/open-consecutive.db", 0x1); //create test.db if not present (cached)
   //printf("TEST handle first: %d \n", ret);
   fail_unless(ret >= 0, "Failed to create non existent lDB: retval: [%d]", ret);

   ret2 = persComDbOpen("/tmp/open-consecutive.db", 0x1); //create test.db if not present (cached)
   //printf("TEST handle second: %d \n", ret2);
   fail_unless(ret2 >= 0, "Failed at consecutive open: retval: [%d]", ret2);

   ret = persComDbClose(ret);
   if (ret != 0)
   {
      printf("persComDbClose() 1 failed: [%d] \n", ret);
   }
   ret = persComDbClose(ret2);
   if (ret != 0)
   {
      printf("persComDbClose() 2 failed: [%d] \n", ret);
   }


   //############# test write through #########################
   //first create database file
   ret2 = persComDbOpen("/tmp/open-write-through.db", 0x3); //write through and create test.db if not present
   //printf("handle 2: %d \n", ret2);
   fail_unless(ret2 >= 0, "Failed at write through open: retval: [%d]", ret);

   ret = persComDbClose(ret2);
   if (ret != 0)
   {
      printf("persComDbClose() cached failed: [%d] \n", ret);
   }

   //then open existing file in write through mode
   ret2 = persComDbOpen("/tmp/open-write-through.db", 0x2); //write through open
   //printf("handle 2: %d \n", ret2);
   fail_unless(ret2 >= 0, "Failed at write through / open existing: retval: [%d]", ret);

   ret = persComDbClose(ret2);
   if (ret != 0)
   {
      printf("persComDbClose() write through / open existing failed: [%d] \n", ret);
   }
   //###########################################################




   //############# test cached with no creation forced #########
   //first create the database
   ret2 = persComDbOpen("/tmp/open-write-cached.db", 0x1); //write cached create database
   //printf("handle 2: %d \n", ret2);
   fail_unless(ret2 >= 0, "Failed at write cached / create open: retval: [%d]", ret);

   ret = persComDbClose(ret2);
   if (ret != 0)
   {
      printf("persComDbClose() cached / create failed: [%d] \n", ret);
   }
   //then try to open existing with cached mode
   ret =  persComDbOpen("/tmp/open-write-cached.db", 0x0); //cached and DO NOT create database
   //printf("handle: %d \n", ret);
   fail_unless(ret >= 0, "Failed at open cached / no database creatio: retval: [%d]", ret); //fail if open works, but should fail

   ret = persComDbClose(ret);
   if (ret != 0)
      printf("persComDbClose() with cached / no database creation ->  failed: [%d] \n", ret);
   //#########################################################





   //try to close a non existent database handle
   ret = persComDbClose(15);
   fail_unless(ret < 0, "Database closing works, but should not: retval: [%d]", ret);

}
END_TEST

//START_TEST(test_OpenLocalDB)
//{
//   int handle =  0;
//   int ret = 0;
//
//   char write2[READ_SIZE] = { 0 };
//   char key[128] = { 0 };
//   int i = 0;
//
//   //Cleaning up testdata folder
//   remove("/tmp/open-localdb3.db");
//   handle = persComDbOpen("/tmp/open-localdb3.db", 0x1); //create test.db if not present
//   fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", handle);
//   //write keys to cache
//   for(i=0; i< 10; i++)
//   {
//      snprintf(key, 128, "Key%d",i);
//      memset(write2, 0, sizeof(write2));
//      snprintf(write2, 128, "DATA-%d-%d",i,i*i );
//      ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
//      fail_unless(ret == strlen(write2), "Wrong write size");
//   }
//   handle = persComDbClose(handle);
//   if (handle != 0)
//   {
//      printf("persComDbClose() failed: [%d] \n", handle);
//   }
//   fail_unless(handle == 0, "Failed to close database: retval: [%d]", handle);
//
//
//   handle = persComDbOpen("/tmp/open-localdb3.db", 0x1); //create test.db if not present
//   fail_unless(handle >= 0, "Failed to create non existent lDB  2nd time: retval: [%d]", handle);
//
//   handle = persComDbClose(handle);
//   if (handle != 0)
//   {
//      printf("persComDbClose()  2nd time failed: [%d] \n", handle);
//   }
//   fail_unless(handle == 0, "Failed to close database 2nd time: retval: [%d]", handle);
//
//}
//END_TEST


START_TEST(test_OpenRCT)
{
   //Cleaning up testdata folder
   remove("/tmp/open-rct.db");

   int ret1 = 0;
   int ret2 = 0;
   ret1 = persComRctOpen("/tmp/open-rct.db", 0x0); //Do not create rct.db / only open if present
   fail_unless(ret1 < 0, "Open open-rct.db works, but should fail: retval: [%d]", ret1);

   ret2 = persComRctOpen("/tmp/open-rct.db", 0x1); //create test.db if not present (cached)
   fail_unless(ret2 >= 0, "Failed to create non existent rct: retval: [%d]", ret2);

   ret1 = persComRctClose(ret1);
   ret2 = persComRctClose(ret2);
   if (ret2 != 0)
   {
      printf("persComRctClose() failed: [%d] \n", ret2);
   }
   fail_unless(ret2 == 0, "Failed to close RCT database: retval: [%d]", ret2);

}
END_TEST



/*
 * Test if a database can be opened in readonly mode
 * First, a valid database file gets written. Then the database is opened in readonly mode
 * Write to the readonly opened database must fail.
 * After reopening, the reading of keys tried to write in readonly mode must fail.
 *
 */
START_TEST(test_ReadOnlyDatabase)
{
   int ret = 0;
   int handle = 0;
   char write2[READ_SIZE] = { 0 };
   char read[READ_SIZE] = { 0 };
   char key[128] = { 0 };
   char sysTimeBuffer[256];
   struct tm* locTime;
   int i =0;

   //Cleaning up testdata folder
   remove("/tmp/open-readonly.db");

#if 1
   time_t t = time(0);
   locTime = localtime(&t);

   // write data
   snprintf(sysTimeBuffer, 128, "\"%s %d.%d.%d - %d:%.2d:%.2d Uhr\"", dayOfWeek[locTime->tm_wday], locTime->tm_mday,
            locTime->tm_mon, (locTime->tm_year + 1900), locTime->tm_hour, locTime->tm_min, locTime->tm_sec);

   handle = persComDbOpen("/tmp/open-readonly.db", 0x1); //create initial database with read write access
   fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);

   snprintf(write2, 128, "%s %s", "/key_70", sysTimeBuffer);

   //write to cache
   for(i=0; i< 300; i++)
   {
      snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
      ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
      fail_unless(ret == strlen(write2) , "Wrong write size while inserting in cache");
   }

   //read from cache
   for(i=0; i< 300; i++)
   {
      snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
      ret = persComDbReadKey(handle, key, (char*) read, strlen(write2));
      fail_unless(ret == strlen(write2), "Wrong read size while reading from cache");
   }

   //printf("read from cache ok \n");

   //persist data in cache to file
   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);


   handle = persComDbOpen("/tmp/open-readonly.db", 0x4); //open existing database in readonly mode
   fail_unless(handle >= 0, "Failed to reopen existing lDB: retval: [%d]", ret);

   //printf("open in readonly ok \n");

   //read from database file
   for(i=0; i< 300; i++)
   {
      snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
      memset(read, 0, 1024);
      ret = persComDbReadKey(handle, key, (char*) read, strlen(write2));
      //printf("read: %d returns: %s \n", i, read);
      fail_unless(ret == strlen(write2), "Wrong read size");
   }

   //write and delete must fail
   ret = persComDbWriteKey(handle, "SHOULD_NOT_BE_PERSISTED", (char*) write2, strlen(write2));
   fail_unless(ret < 0, "Writing to readonly opened database worked, but should fail for key: [SHOULD_NOT_BE_PERSISTED] !");

   ret = persComDbDeleteKey(handle, "SHOULD_NOT_BE_PERSISTED");
   fail_unless(ret < 0, "Deletion in readonly opened database worked, but should fail for key: [SHOULD_NOT_BE_PERSISTED] !");

   //writeback should not be invoked because of readonly mode
   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close database file: retval: [%d]", ret);

   handle = persComDbOpen("/tmp/open-readonly.db", 0x4); //open in readonly mode
   fail_unless(handle >= 0, "Failed to reopen existing lDB: retval: [%d]", ret);

   memset(read, 0, 1024);
   ret = persComDbReadKey(handle, "SHOULD_NOT_BE_PERSISTED", (char*) read, strlen(write2));
   //printf("read: %d returns: %s \n", i, read);
   fail_unless(ret < 0, "Reading of key: [SHOULD_NOT_BE_PERSISTED] works, but should fail!");

   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close database file: retval: [%d]", ret);
#endif
}
END_TEST











/*
 * Write data to a key using the key interface in local DB.
 * First write data to different keys and after
 * read the data for verification.
 */
START_TEST(test_SetDataLocalDB)
{
   int ret = 0;
   int handle = 0;
   char write2[READ_SIZE] = { 0 };
   char read[READ_SIZE] = { 0 };
   char key[128] = { 0 };
   char sysTimeBuffer[256];
   struct tm* locTime;
   int i =0;

   //Cleaning up testdata folder
   remove("/tmp/write-localdb.db");

#if 1
   time_t t = time(0);
   locTime = localtime(&t);

   // write data
   snprintf(sysTimeBuffer, 128, "\"%s %d.%d.%d - %d:%.2d:%.2d Uhr\"", dayOfWeek[locTime->tm_wday], locTime->tm_mday,
            locTime->tm_mon, (locTime->tm_year + 1900), locTime->tm_hour, locTime->tm_min, locTime->tm_sec);

   handle = persComDbOpen("/tmp/write-localdb.db", 0x1); //create test.db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);

   snprintf(write2, 128, "%s %s", "/key_70", sysTimeBuffer);

   //write to cache
   for(i=0; i< 300; i++)
   {
      snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
      ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
      fail_unless(ret == strlen(write2) , "Wrong write size while inserting in cache");
   }

   //read from cache
   for(i=0; i< 300; i++)
   {
      snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
      ret = persComDbReadKey(handle, key, (char*) read, strlen(write2));
      fail_unless(ret == strlen(write2), "Wrong read size while reading from cache");
   }

   //printf("read from cache ok \n");

   //persist data in cache to file
   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);


   handle = persComDbOpen("/tmp/write-localdb.db", 0x1); //create test.db if not present
   fail_unless(handle >= 0, "Failed to reopen existing lDB: retval: [%d]", ret);


   //printf("open ok \n");

   //read from database file
   for(i=0; i< 300; i++)
   {
      snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
      memset(read, 0, 1024);
      ret = persComDbReadKey(handle, key, (char*) read, strlen(write2));

      //printf("read: %d returns: %s \n", i, read);
      fail_unless(ret == strlen(write2), "Wrong read size");
   }

   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close database file: retval: [%d]", ret);

#endif
}
END_TEST




/*
 * Write data to a key using the key interface in local DB.
 * First the data gets written to Cache. Then this data is read from the Cache.
 * After that, the database gets closed in order to persist the cached data to the database file
 * The database file is opened again and the keys are read from file for verification
 */
START_TEST(test_GetDataLocalDB)
{
   int ret = 0;
   int handle = 0;
   unsigned char readBuffer[READ_SIZE] = { 0 };
   char write2[READ_SIZE] = { 0 };
   char key[128] = { 0 };
   int i = 0;

   //Cleaning up testdata folder
   remove("/tmp/get-localdb.db");


#if 1

   handle = persComDbOpen("/tmp/get-localdb.db", 0x1); //create test.db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);


   //write keys to cache
   for(i=0; i< 300; i++)
   {
      snprintf(key, 128, "Key%d",i);
      memset(write2, 0, sizeof(write2));
      snprintf(write2, 128, "DATA-%d-%d",i,i*i );
      ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
      fail_unless(ret == strlen(write2), "Wrong write size");
   }

   //Read keys from cache
   for(i=0; i< 300; i++)
   {
      snprintf(key, 128, "Key%d",i);
      memset(write2, 0, sizeof(write2));
      snprintf(write2, sizeof(write2), "DATA-%d-%d",i,i*i );
      memset(readBuffer, 0, sizeof(readBuffer));
      ret = persComDbReadKey(handle, key, (char*) readBuffer, sizeof(readBuffer));
      fail_unless(ret == strlen(write2), "Wrong read size");
      fail_unless(memcmp(readBuffer, write2, sizeof(readBuffer)) == 0, "Reading Data from Cache failed: Buffer not correctly read");
   }

   //persist changed data for this lifecycle
   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: Cached Data was not written back: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);

   //open database again
   handle = persComDbOpen("/tmp/get-localdb.db", 0x1); //create test.db if not present
   fail_unless(handle >= 0, "Failed to reopen existing lDB: retval: [%d]", ret);


   //Read keys from database file
   for(i=0; i< 300; i++)
   {
      snprintf(key, 128, "Key%d",i);
      memset(write2, 0, sizeof(write2));
      snprintf(write2, sizeof(write2), "DATA-%d-%d",i,i*i );
      memset(readBuffer, 0, sizeof(readBuffer));
      ret = persComDbReadKey(handle, key, (char*) readBuffer, sizeof(readBuffer));
      fail_unless(ret == strlen(write2), "Wrong read size");
      fail_unless(memcmp(readBuffer, write2, sizeof(readBuffer)) == 0, "Reading Data from File failed: Buffer not correctly read");
   }

   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close database file: retval: [%d]", ret);

#endif
}
END_TEST




/*
 * Get the size from an existing local DB needed to store all already inserted key names in a list
 * First insert 3 different keys then close the database to persist the data an reopen it.
 * Then get the size of all keys separated with '\0'
 * After that a duplicate key gets written to the cache and the size of the list is read again to test if duplicate keys are ignored correctly.
 * Then a new key gets written to the cache and the list size is read again to test if keys in cache and also keys in the database file are counted.
 */
START_TEST(test_GetKeyListSizeLocalDB)
{
   int ret = 0;
   int handle = 0;
   char write1[READ_SIZE] = { 0 };
   char write2[READ_SIZE] = { 0 };
   char sysTimeBuffer[256];
   int listSize = 0;
   char key[8] = { 0 };
   struct tm* locTime;

   //Cleaning up testdata folder
   remove("/tmp/localdb-size-keylist.db");


#if 1
   time_t t = time(0);
   locTime = localtime(&t);

   // write data
   snprintf(sysTimeBuffer, 128, "\"%s %d.%d.%d - %d:%.2d:%.2d Uhr\"", dayOfWeek[locTime->tm_wday], locTime->tm_mday,
            locTime->tm_mon, (locTime->tm_year + 1900), locTime->tm_hour, locTime->tm_min, locTime->tm_sec);

   handle = persComDbOpen("/tmp/localdb-size-keylist.db", 0x1); //create db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB for keylist test: retval: [%d]", ret);


   snprintf(key, 8, "%s", "key_123");
   ret = persComDbWriteKey(handle, key, (char*) sysTimeBuffer, strlen(sysTimeBuffer));
   fail_unless(ret == strlen(sysTimeBuffer), "Wrong write size");


   snprintf(key, 8, "%s", "key_456");
   snprintf(write1, 128, "%s %s", "k_456", sysTimeBuffer);
   ret = persComDbWriteKey(handle, key, (char*) write1, strlen(write1));
   fail_unless(ret == strlen(write1), "Wrong write size");

   snprintf(key, 8, "%s", "key_789");
   snprintf(write2, 128, "%s %s", "k_789", sysTimeBuffer);
   ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
   fail_unless(ret == strlen(write2), "Wrong write size");

   listSize = persComDbGetSizeKeysList(handle);
   //printf("LISTSIZE: %d \n", listSize);
   fail_unless(listSize == 3 * strlen(key) + 3, "Wrong list size read from cache");

   //persist changes in order to read only keys that are in database file
   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);

   handle = persComDbOpen("/tmp/localdb-size-keylist.db", 0x1); //create db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB for keylist test: retval: [%d]", ret);


   //write duplicated key to cache
   snprintf(key, 8, "%s", "key_789");
   snprintf(write2, 128, "%s %s", "k_789", sysTimeBuffer);
   ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
   fail_unless(ret == strlen(write2), "Wrong write size");

   //get needed listsize (looks for keys in cache and in database file) duplicate keys occuring in cache AND in database file are removed
   // listsize here must be 24
   listSize = persComDbGetSizeKeysList(handle);
   //printf("LISTSIZE: %d \n", listSize);
   fail_unless(listSize == 3 * strlen(key) + 3, "Wrong list size read from file");

   //write new key to cache
   snprintf(key, 8, "%s", "key_000");
   snprintf(write2, 128, "%s %s", "k_000", sysTimeBuffer);
   ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));

   //read list size again (must be 32)
   listSize = persComDbGetSizeKeysList(handle);
   //printf("LISTSIZE: %d \n", listSize);
   fail_unless(listSize == 4 * strlen(key) + 4, "Wrong list size read from combined cache / file");

   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: [%d] \n", ret);
   }
   handle = -1;
   fail_unless(ret == 0, "Failed to close database file: retval: [%d]", ret);

   //
   // open again
   //
   handle = persComDbOpen("/tmp/localdb-size-keylist.db", 0x1); //create db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB for keylist test: retval: [%d]", ret);


   //read list size again (must be 32)
   listSize = persComDbGetSizeKeysList(handle);
   //printf("LISTSIZE: %d \n", listSize);
   fail_unless(listSize == 4 * strlen(key) + 4, "Wrong list size read from combined cache / file");

   snprintf(key, 8, "%s", "key_123");
   ret = persComDbDeleteKey(handle, key);
   fail_unless(ret >= 0, "Failed to delete key: %s - %d", key, ret);


   //read list size again (must be 32)
   listSize = persComDbGetSizeKeysList(handle);
   //printf("LISTSIZE: %d \n", listSize);
   fail_unless(listSize == 4 * strlen(key) + 4, "Wrong list size read from combined cache / file");

   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: [%d] \n", ret);
   }
   handle = -1;
   fail_unless(ret == 0, "Failed to close database file: retval: [%d]", ret);


   //
   // open again
   //
   handle = persComDbOpen("/tmp/localdb-size-keylist.db", 0x1); //create db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB for keylist test: retval: [%d]", ret);

   //read list size again (must be 24)
   listSize = persComDbGetSizeKeysList(handle);
   //printf("LISTSIZE: %d \n", listSize);
   fail_unless(listSize == 3 * strlen(key) + 3, "Wrong list size read from combined cache / file");

   snprintf(key, 8, "%s", "key_123");
   ret = persComDbWriteKey(handle, key, (char*) sysTimeBuffer, strlen(sysTimeBuffer));
   fail_unless(ret == strlen(sysTimeBuffer), "Wrong write size");

   //read list size again (must be 32)
   listSize = persComDbGetSizeKeysList(handle);
   //printf("LISTSIZE: %d \n", listSize);
   fail_unless(listSize == 4 * strlen(key) + 4, "Wrong list size read from combined cache / file");

   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: [%d] \n", ret);
   }
   handle = -1;
   fail_unless(ret == 0, "Failed to close database file: retval: [%d]", ret);

#endif

}
END_TEST




/* Get the resource list from an existing local database with already inserted key names
 * Insert some keys then get the list of all keys separated with '\0'
 * Then check if the List returned contains all of the keys inserted before
 */
START_TEST(test_GetKeyListLocalDB)
{
   int ret = 0;
   int handle = 0;
   char write1[READ_SIZE] = { 0 };
   char write2[READ_SIZE] = { 0 };
   char sysTimeBuffer[256];
   char origKeylist[256] = { 0 };
   char key1[8] = { 0 };
   char key2[8] = { 0 };
   char key3[8] = { 0 };
   char key4[8] = { 0 };
   struct tm* locTime;
   char* keyList = NULL;
   int listSize = 0;

   //Cleaning up testdata folder
   remove("/tmp/localdb-keylist.db");

#if 1
   time_t t = time(0);
   locTime = localtime(&t);

   // write data
   snprintf(sysTimeBuffer, 128, "\"%s %d.%d.%d - %d:%.2d:%.2d Uhr\"", dayOfWeek[locTime->tm_wday], locTime->tm_mday,
            locTime->tm_mon, (locTime->tm_year + 1900), locTime->tm_hour, locTime->tm_min, locTime->tm_sec);

   handle = persComDbOpen("/tmp/localdb-keylist.db", 0x1); //create db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB for keylist test: retval: [%d]", ret);

   snprintf(key1, 8, "%s", "key_123");
   ret = persComDbWriteKey(handle, key1, (char*) sysTimeBuffer, strlen(sysTimeBuffer));
   fail_unless(ret == strlen(sysTimeBuffer), "Wrong write size");

   snprintf(key2, 8, "%s", "key_456");
   snprintf(write1, 128, "%s %s", "k_456", sysTimeBuffer);
   ret = persComDbWriteKey(handle, key2, (char*) write1, strlen(write1));
   fail_unless(ret == strlen(write1), "Wrong write size");

   snprintf(key3, 8, "%s", "key_789");
   snprintf(write2, 128, "%s %s", "k_789", sysTimeBuffer);
   ret = persComDbWriteKey(handle, key3, (char*) write2, strlen(write2));
   fail_unless(ret == strlen(write2), "Wrong write size");

   //close database in order to persist the cached keys.
   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);

   handle = persComDbOpen("/tmp/localdb-keylist.db", 0x1); //create db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB for keylist test: retval: [%d]", ret);


   //write to cache
   snprintf(key4, 8, "%s", "key_456");
   snprintf(write1, 128, "%s %s", "k_456", sysTimeBuffer);
   ret = persComDbWriteKey(handle, key4, (char*) write1, strlen(write1));

   //read keys from file and from cache
   listSize = persComDbGetSizeKeysList(handle);
   fail_unless(listSize == 3 * strlen(key1) + 3, "Wrong list size");

   keyList = (char*) malloc(listSize);
   ret = persComDbGetKeysList(handle, keyList, listSize);
   int cmp_result = 0;

   //try all possible key orders in the list
   snprintf(origKeylist, 24, "%s%c%s%c%s", key1, '\0', key2, '\0', key3);
   if( memcmp(keyList, origKeylist, listSize) != 0)
   {
      cmp_result = 1;
      snprintf(origKeylist, 24, "%s%c%s%c%s", key1, '\0', key3, '\0', key2);
      if(memcmp(keyList, origKeylist, listSize) != 0)
      {
         cmp_result = 1;
         snprintf(origKeylist, 24, "%s%c%s%c%s", key2, '\0', key3, '\0', key1);
         if(memcmp(keyList, origKeylist, listSize) != 0)
         {
            cmp_result = 1;
            snprintf(origKeylist, 24, "%s%c%s%c%s", key2, '\0', key1, '\0', key3);
            if(memcmp(keyList, origKeylist, listSize) != 0)
            {
               cmp_result = 1;
               snprintf(origKeylist, 24, "%s%c%s%c%s", key3, '\0', key1, '\0', key2);
               if(memcmp(keyList, origKeylist, listSize) != 0)
               {
                  cmp_result = 1;
                  snprintf(origKeylist, 24, "%s%c%s%c%s", key3, '\0', key2, '\0', key1);
                  if(memcmp(keyList, origKeylist, listSize) != 0)
                  {
                     cmp_result = 1;
                  }
                  else
                  {
                     cmp_result = 0;
                  }
               }
               else
               {
                  cmp_result = 0;
               }
            }
            else
            {
               cmp_result = 0;
            }
         }
         else
         {
            cmp_result = 0;
         }
      }
      else
      {
         cmp_result = 0;
      }
   }
   else
   {
      cmp_result = 0;
   }

   //   printf("original keylist: [%s] \n", origKeylist);
   //   printf("keylist: [%s] \n", keyList);
   free(keyList);
   fail_unless(cmp_result == 0, "List not correctly read");

   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close database file: retval: [%d]", ret);

#endif

}
END_TEST

/*
 * Get the size from an existing RCT database needed to store all already inserted key names in a list
 * Insert some keys then get the size of all keys separated with '\0'. Then close the database and reopen it to read the size again (from file).
 * After that, close and reopen the database, to insert a duplicate key and a new key into cache. Then verify the listsize again.
 */
START_TEST(test_GetResourceListSizeRct)
{
   int ret = 0;
   int handle = 0;
   char sysTimeBuffer[256];
   char key1[8] = { 0 };
   char key2[8] = { 0 };
   char key3[8] = { 0 };
   char key4[8] = { 0 };
   int listSize = 0;
   struct tm* locTime;

   PersistenceConfigurationKey_s psConfig;
   psConfig.policy = PersistencePolicy_wt;
   psConfig.storage = PersistenceStorage_local;
   psConfig.type = PersistenceResourceType_key;
   psConfig.permission = PersistencePermission_ReadWrite;

   //Cleaning up testdata folder
   remove("/tmp/rct-size-resource-list.db");

#if 1
   time_t t = time(0);
   locTime = localtime(&t);

   // write data
   snprintf(sysTimeBuffer, 64, "\"%s %d.%d.%d - %d:%.2d:%.2d Uhr\"", dayOfWeek[locTime->tm_wday], locTime->tm_mday,
            locTime->tm_mon, (locTime->tm_year + 1900), locTime->tm_hour, locTime->tm_min, locTime->tm_sec);

   handle = persComRctOpen("/tmp/rct-size-resource-list.db", 0x1); //create rct.db if not present
   fail_unless(handle >= 0, "Failed to create non existent rct: retval: [%d]", ret);

   memset(psConfig.custom_name, 0, sizeof(psConfig.custom_name));
   memset(psConfig.customID, 0, sizeof(psConfig.customID));
   memset(psConfig.reponsible, 0, sizeof(psConfig.reponsible));

   psConfig.max_size = 12345;
   char custom_name[PERS_RCT_MAX_LENGTH_CUSTOM_NAME] = "this is the custom name";
   char custom_ID[PERS_RCT_MAX_LENGTH_CUSTOM_ID] = "this is the custom ID";
   char responsible[PERS_RCT_MAX_LENGTH_RESPONSIBLE] = "this is the responsible";

   strncpy(psConfig.custom_name, custom_name, strlen(custom_name));
   strncpy(psConfig.customID, custom_ID, strlen(custom_ID));
   strncpy(psConfig.reponsible, responsible, strlen(responsible));

   snprintf(key1, 8, "%s", "key_123");
   ret = persComRctWrite(handle, key1, &psConfig);
   fail_unless(ret == sizeof(psConfig), "Wrong write size");

   snprintf(key2, 8, "%s", "key_45");
   ret = persComRctWrite(handle, key2, &psConfig);
   fail_unless(ret == sizeof(psConfig), "Wrong write size");

   snprintf(key3, 8, "%s", "key_7");
   ret = persComRctWrite(handle, key3, &psConfig);
   fail_unless(ret == sizeof(psConfig), "Wrong write size");

   //get listsize from cache
   listSize = persComRctGetSizeResourcesList(handle);
   fail_unless(listSize == 3 * strlen(key1), "Read Wrong list size from file and cache");

   //persist cached data
   ret = persComRctClose(handle);
   if (ret != 0)
   {
      printf("persComRctClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);

   handle = persComRctOpen("/tmp/rct-size-resource-list.db", 0x1); //create rct.db if not present
   fail_unless(handle >= 0, "Failed to create non existent rct: retval: [%d]", ret);

   //get listsize from file
   listSize = persComRctGetSizeResourcesList(handle);
   fail_unless(listSize == 3 * strlen(key1), "Read Wrong list size from file and cache");

   ret = persComRctClose(handle);
   if (ret != 0)
   {
      printf("persComRctClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close database: retval: [%d]", ret);

   handle = persComRctOpen("/tmp/rct-size-resource-list.db", 0x1); //create rct.db if not present
   fail_unless(handle >= 0, "Failed to create non existent rct: retval: [%d]", ret);

   //insert duplicate key
   snprintf(key3, 8, "%s", "key_7");
   ret = persComRctWrite(handle, key3, &psConfig);
   fail_unless(ret == sizeof(psConfig), "Wrong write size");

   //insert new key
   snprintf(key4, 8, "%s", "key_new");
   ret = persComRctWrite(handle, key4, &psConfig);
   fail_unless(ret == sizeof(psConfig), "Wrong write size");

   //get listsize if keys are in cache and in file
   listSize = persComRctGetSizeResourcesList(handle);
   fail_unless(listSize == strlen(key1)+ strlen(key2) + strlen(key3) + strlen(key4) + 4, "Read Wrong list size from file and cache");

   ret = persComRctClose(handle);
   if (ret != 0)
   {
      printf("persComRctClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close database file: retval: [%d]", ret);

#endif

}
END_TEST

/*
 * Get the resource list from an existing RCT database with already inserted key names
 * Insert some keys then get the list of all keys separated with '\0'
 * Then check if the List returned contains all of the keys inserted before
 */
START_TEST(test_GetResourceListRct)
{
   int ret = 0;
   int handle = 0;
   char sysTimeBuffer[256];
   char origKeylist[256] = { 0 };
   char* resourceList = NULL;
   int listSize = 0;
   char key1[8] = { 0 };
   char key2[8] = { 0 };
   char key3[8] = { 0 };
   char key4[8] = { 0 };
   struct tm* locTime;

   PersistenceConfigurationKey_s psConfig;
   psConfig.policy = PersistencePolicy_wt;
   psConfig.storage = PersistenceStorage_local;
   psConfig.type = PersistenceResourceType_key;
   psConfig.permission = PersistencePermission_ReadWrite;

   //Cleaning up testdata folder
   remove("/tmp/rct-resource-list.db");

#if 1
   time_t t = time(0);
   locTime = localtime(&t);

   // write data
   snprintf(sysTimeBuffer, 64, "\"%s %d.%d.%d - %d:%.2d:%.2d Uhr\"", dayOfWeek[locTime->tm_wday], locTime->tm_mday,
            locTime->tm_mon, (locTime->tm_year + 1900), locTime->tm_hour, locTime->tm_min, locTime->tm_sec);

   handle = persComRctOpen("/tmp/rct-resource-list.db", 0x1); //create rct.db if not present
   fail_unless(handle >= 0, "Failed to create non existent rct: retval: [%d]", ret);

   memset(psConfig.custom_name, 0, sizeof(psConfig.custom_name));
   memset(psConfig.customID, 0, sizeof(psConfig.customID));
   memset(psConfig.reponsible, 0, sizeof(psConfig.reponsible));

   psConfig.max_size = 12345;
   char custom_name[PERS_RCT_MAX_LENGTH_CUSTOM_NAME] = "this is the custom name";
   char custom_ID[PERS_RCT_MAX_LENGTH_CUSTOM_ID] = "this is the custom ID";
   char responsible[PERS_RCT_MAX_LENGTH_RESPONSIBLE] = "this is the responsible";

   strncpy(psConfig.custom_name, custom_name, strlen(custom_name));
   strncpy(psConfig.customID, custom_ID, strlen(custom_ID));
   strncpy(psConfig.reponsible, responsible, strlen(responsible));

   snprintf(key1, 8, "%s", "key_123");
   ret = persComRctWrite(handle, key1, &psConfig);
   fail_unless(ret == sizeof(psConfig), "Wrong write size");

   snprintf(key2, 8, "%s", "key_456");
   ret = persComRctWrite(handle, key2, &psConfig);
   fail_unless(ret == sizeof(psConfig), "Wrong write size");

   snprintf(key3, 8, "%s", "key_789");
   ret = persComRctWrite(handle, key3, &psConfig);
   fail_unless(ret == sizeof(psConfig), "Wrong write size");

   snprintf(origKeylist, 24, "%s%c%s%c%s", key1, '\0', key3, '\0', key2);

   //persist keys to file
   ret = persComRctClose(handle);
   if (ret != 0)
   {
      printf("persComRctClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);

   handle = persComRctOpen("/tmp/rct-resource-list.db", 0x1); //create rct.db if not present
   fail_unless(handle >= 0, "Failed to create non existent rct: retval: [%d]", ret);

   //write duplicate key to cache
   snprintf(key4, 8, "%s", "key_456");
   ret = persComRctWrite(handle, key4, &psConfig);
   fail_unless(ret == sizeof(psConfig), "Wrong write size");

   //read keys from file and from cache
   listSize = persComRctGetSizeResourcesList(handle);
   fail_unless(listSize == 3 * strlen(key1) + 3, "Wrong list size");

   resourceList = (char*) malloc(listSize);
   ret = persComRctGetResourcesList(handle, resourceList, listSize);

   //compare returned list (unsorted) with original list
   int cmp_result = 0;

   snprintf(origKeylist, 24, "%s%c%s%c%s", key1, '\0', key2, '\0', key3);
   if( memcmp(resourceList, origKeylist, listSize) != 0)
   {
      cmp_result = 1;
      snprintf(origKeylist, 24, "%s%c%s%c%s", key1, '\0', key3, '\0', key2);
      if(memcmp(resourceList, origKeylist, listSize) != 0)
      {
         cmp_result = 1;
         snprintf(origKeylist, 24, "%s%c%s%c%s", key2, '\0', key3, '\0', key1);
         if(memcmp(resourceList, origKeylist, listSize) != 0)
         {
            cmp_result = 1;
            snprintf(origKeylist, 24, "%s%c%s%c%s", key2, '\0', key1, '\0', key3);
            if(memcmp(resourceList, origKeylist, listSize) != 0)
            {
               cmp_result = 1;
               snprintf(origKeylist, 24, "%s%c%s%c%s", key3, '\0', key1, '\0', key2);
               if(memcmp(resourceList, origKeylist, listSize) != 0)
               {
                  cmp_result = 1;
                  snprintf(origKeylist, 24, "%s%c%s%c%s", key3, '\0', key2, '\0', key1);
                  if(memcmp(resourceList, origKeylist, listSize) != 0)
                  {
                     cmp_result = 1;
                  }
                  else
                  {
                     cmp_result = 0;
                  }
               }
               else
               {
                  cmp_result = 0;
               }
            }
            else
            {
               cmp_result = 0;
            }
         }
         else
         {
            cmp_result = 0;
         }
      }
      else
      {
         cmp_result = 0;
      }
   }
   else
   {
      cmp_result = 0;
   }

   //printf("original resourceList: [%s] \n", origKeylist);
   //printf("resourceList: [%s]\n", resourceList);
   free(resourceList);
   fail_unless(cmp_result == 0, "List not correctly read");

   ret = persComRctClose(handle);
   if (ret != 0)
   {
      printf("persComRctClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close database: retval: [%d]", ret);

#endif

}
END_TEST

/*
 * Write data to a key using the key interface for RCT databases
 * First write data to different keys and after that
 * read the data for verification.
 */
START_TEST(test_SetDataRCT)
{
   int ret = 0;
   int handle = 0;
   char sysTimeBuffer[256];
   struct tm* locTime;

   PersistenceConfigurationKey_s psConfig, psConfig_out;
   psConfig.policy = PersistencePolicy_wt;
   psConfig.storage = PersistenceStorage_local;
   psConfig.type = PersistenceResourceType_key;
   psConfig.permission = PersistencePermission_ReadWrite;

#if 1
   time_t t = time(0);
   locTime = localtime(&t);

   // write data
   snprintf(sysTimeBuffer, 64, "\"%s %d.%d.%d - %d:%.2d:%.2d Uhr\"", dayOfWeek[locTime->tm_wday], locTime->tm_mday,
            locTime->tm_mon, (locTime->tm_year + 1900), locTime->tm_hour, locTime->tm_min, locTime->tm_sec);

   //Cleaning up testdata folder
   remove("/tmp/write-rct.db");

   handle = persComRctOpen("/tmp/write-rct.db", 0x1); //create db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);

   memset(psConfig.custom_name, 0, sizeof(psConfig.custom_name));
   memset(psConfig.customID, 0, sizeof(psConfig.customID));
   memset(psConfig.reponsible, 0, sizeof(psConfig.reponsible));

   psConfig.max_size = 12345;
   char custom_name[PERS_RCT_MAX_LENGTH_CUSTOM_NAME] = "this is the custom name";
   char custom_ID[PERS_RCT_MAX_LENGTH_CUSTOM_ID] = "this is the custom ID";
   char responsible[PERS_RCT_MAX_LENGTH_RESPONSIBLE] = "this is the responsible";

   strncpy(psConfig.custom_name, custom_name, strlen(custom_name));
   strncpy(psConfig.customID, custom_ID, strlen(custom_ID));
   strncpy(psConfig.reponsible, responsible, strlen(responsible));


//printf("Custom ID        : %s\n", psConfig.customID );
//printf("Custom Name      : %s\n", psConfig.custom_name );
//printf("reponsible       : %s\n", psConfig.reponsible );
//printf("max_size         : %d\n", psConfig.max_size );
//printf("permission       : %d\n", psConfig.permission );
//printf("type             : %d\n", psConfig.type );
//printf("storage          : %d\n", psConfig.storage );
//printf("policy           : %d\n", psConfig.policy );


   ret = persComRctWrite(handle, "69", &psConfig);
   fail_unless(ret == sizeof(psConfig), "wrong write size \n");
#if 1

   memset(psConfig_out.custom_name, 0, sizeof(psConfig_out.custom_name));
   memset(psConfig_out.customID, 0, sizeof(psConfig_out.customID));
   memset(psConfig_out.reponsible, 0, sizeof(psConfig_out.reponsible));

   //read from cache
   ret = persComRctRead(handle, "69", &psConfig_out);
   fail_unless(ret == sizeof(psConfig), "Wrong read size from cache");


   //persist data in cache to database file
   ret = persComRctClose(handle);
   if (ret != 0)
   {
      printf("persComRctClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);

   //reopen database
   handle = persComRctOpen("/tmp/write-rct.db", 0x1); //create db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);

   /*
    * now read the data written in the previous steps to the keys in RCT
    * and verify data has been written correctly.
    */
   memset(psConfig_out.custom_name, 0, sizeof(psConfig_out.custom_name));
   memset(psConfig_out.customID, 0, sizeof(psConfig_out.customID));
   memset(psConfig_out.reponsible, 0, sizeof(psConfig_out.reponsible));

   //read from file
   ret = persComRctRead(handle, "69", &psConfig_out);
   fail_unless(ret == sizeof(psConfig), "Wrong read size from file");

//printf("Custom ID        : %s\n", psConfig_out.customID );
//printf("Custom Name      : %s\n", psConfig_out.custom_name );
//printf("reponsible       : %s\n", psConfig_out.reponsible );
//printf("max_size         : %d\n", psConfig_out.max_size );
//printf("permission       : %d\n", psConfig_out.permission );
//printf("type             : %d\n", psConfig_out.type );
//printf("storage          : %d\n", psConfig_out.storage );
//printf("policy           : %d\n", psConfig_out.policy );

   fail_unless(strncmp(psConfig.customID, psConfig_out.customID, strlen(psConfig_out.customID)) == 0,
                 "Buffer not correctly read");
   fail_unless(strncmp(psConfig.custom_name, psConfig_out.custom_name, strlen(psConfig_out.custom_name)) == 0,
                 "Buffer not correctly read");
   fail_unless(strncmp(psConfig.reponsible, psConfig_out.reponsible, strlen(psConfig_out.reponsible)) == 0,
                 "Buffer not correctly read");
   fail_unless(psConfig.max_size == psConfig_out.max_size, "Buffer not correctly read");
   fail_unless(psConfig.permission == psConfig_out.permission, "Buffer not correctly read");
   fail_unless(psConfig.policy == psConfig_out.policy, "Buffer not correctly read");
   fail_unless(psConfig.storage == psConfig_out.storage, "Buffer not correctly read");
   fail_unless(psConfig.type == psConfig_out.type, "Buffer not correctly read");

   //persist to database file
   ret = persComRctClose(handle);
   if (ret != 0)
   {
      printf("persComRctClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close database: retval: [%d]", ret);

#endif
#endif

}
END_TEST



/*
 * Test reading of data to a key using the key interface for RCT
 * First write data to cache, then read from cache.
 * Then the database gets closed and reopened.
 * The next read  for verification is done from file.
 */
START_TEST(test_GetDataRCT)
{
   int ret = 0;
   int handle = 0;
   char sysTimeBuffer[256];
   struct tm* locTime;

   PersistenceConfigurationKey_s psConfig, psConfig_out;
   psConfig.policy = PersistencePolicy_wt;
   psConfig.storage = PersistenceStorage_local;
   psConfig.type = PersistenceResourceType_key;
   psConfig.permission = PersistencePermission_ReadWrite;

#if 1
   time_t t = time(0);
   locTime = localtime(&t);

   // write data
   snprintf(sysTimeBuffer, 64, "\"%s %d.%d.%d - %d:%.2d:%.2d Uhr\"", dayOfWeek[locTime->tm_wday], locTime->tm_mday,
            locTime->tm_mon, (locTime->tm_year + 1900), locTime->tm_hour, locTime->tm_min, locTime->tm_sec);

   //Cleaning up testdata folder
   remove("/tmp/get-rct.db");

   handle = persComRctOpen("/tmp/get-rct.db", 0x1); //create db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);

   memset(psConfig.custom_name, 0, sizeof(psConfig.custom_name));
   memset(psConfig.customID, 0, sizeof(psConfig.customID));
   memset(psConfig.reponsible, 0, sizeof(psConfig.reponsible));
   psConfig.max_size = 12345;

   char custom_name[PERS_RCT_MAX_LENGTH_CUSTOM_NAME] = "this is the custom name";
   char custom_ID[PERS_RCT_MAX_LENGTH_CUSTOM_ID] = "this is the custom ID";
   char responsible[PERS_RCT_MAX_LENGTH_RESPONSIBLE] = "this is the responsible";

   strncpy(psConfig.custom_name, custom_name, strlen(custom_name));
   strncpy(psConfig.customID, custom_ID, strlen(custom_ID));
   strncpy(psConfig.reponsible, responsible, strlen(responsible));


   ret = persComRctWrite(handle, "69", &psConfig);
   fail_unless(ret == sizeof(psConfig), "write size wrong");
#if 1


   /*
    * now read the data written in the previous steps to the keys in RCT
    * and verify data has been written correctly.
    */
   memset(psConfig_out.custom_name, 0, sizeof(psConfig_out.custom_name));
   memset(psConfig_out.customID, 0, sizeof(psConfig_out.customID));
   memset(psConfig_out.reponsible, 0, sizeof(psConfig_out.reponsible));

//read from cache
   ret = persComRctRead(handle, "69", &psConfig_out);
   fail_unless(ret == sizeof(psConfig_out), "Wrong read size from cache");

//printf("Custom ID        : %s\n", psConfig_out.customID );
//printf("Custom Name      : %s\n", psConfig_out.custom_name );
//printf("reponsible       : %s\n", psConfig_out.reponsible );
//printf("max_size         : %d\n", psConfig_out.max_size );
//printf("permission       : %d\n", psConfig_out.permission );
//printf("type             : %d\n", psConfig_out.type );
//printf("storage          : %d\n", psConfig_out.storage );
//printf("policy           : %d\n", psConfig_out.policy );



   fail_unless(strncmp(psConfig.customID, psConfig_out.customID, strlen(psConfig_out.customID)) == 0,
                 "Buffer not correctly read");
   fail_unless(strncmp(psConfig.custom_name, psConfig_out.custom_name, strlen(psConfig_out.custom_name)) == 0,
                 "Buffer not correctly read");
   fail_unless(strncmp(psConfig.reponsible, psConfig_out.reponsible, strlen(psConfig_out.reponsible)) == 0,
                 "Buffer not correctly read");
   fail_unless(psConfig.max_size == psConfig_out.max_size, "Buffer not correctly read");
   fail_unless(psConfig.permission == psConfig_out.permission, "Buffer not correctly read");
   fail_unless(psConfig.policy == psConfig_out.policy, "Buffer not correctly read");
   fail_unless(psConfig.storage == psConfig_out.storage, "Buffer not correctly read");
   fail_unless(psConfig.type == psConfig_out.type, "Buffer not correctly read");


   //persist to database file
   ret = persComRctClose(handle);
   if (ret != 0)
   {
      printf("persComRctClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close database: retval: [%d]", ret);

#endif
#endif


   handle = persComRctOpen("/tmp/get-rct.db", 0x1); //create db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);


   memset(psConfig_out.custom_name, 0, sizeof(psConfig_out.custom_name));
   memset(psConfig_out.customID, 0, sizeof(psConfig_out.customID));
   memset(psConfig_out.reponsible, 0, sizeof(psConfig_out.reponsible));


   //read from file
   ret = persComRctRead(handle, "69", &psConfig_out);
   fail_unless(ret == sizeof(psConfig_out), "Wrong read size from file");

//printf("Custom ID        : %s\n", psConfig_out.customID );
//printf("Custom Name      : %s\n", psConfig_out.custom_name );
//printf("reponsible       : %s\n", psConfig_out.reponsible );
//printf("max_size         : %d\n", psConfig_out.max_size );
//printf("permission       : %d\n", psConfig_out.permission );
//printf("type             : %d\n", psConfig_out.type );
//printf("storage          : %d\n", psConfig_out.storage );
//printf("policy           : %d\n", psConfig_out.policy );

   fail_unless(strncmp(psConfig.customID, psConfig_out.customID, strlen(psConfig_out.customID)) == 0,
                 "Buffer not correctly read from file");
   fail_unless(strncmp(psConfig.custom_name, psConfig_out.custom_name, strlen(psConfig_out.custom_name)) == 0,
                 "Buffer not correctly read from file");
   fail_unless(strncmp(psConfig.reponsible, psConfig_out.reponsible, strlen(psConfig_out.reponsible)) == 0,
                 "Buffer not correctly read from file");
   fail_unless(psConfig.max_size == psConfig_out.max_size, "Buffer not correctly read from file");
   fail_unless(psConfig.permission == psConfig_out.permission, "Buffer not correctly read from file");
   fail_unless(psConfig.policy == psConfig_out.policy, "Buffer not correctly read from file");
   fail_unless(psConfig.storage == psConfig_out.storage, "Buffer not correctly read from file");
   fail_unless(psConfig.type == psConfig_out.type, "Buffer not correctly read from file");


   ret = persComRctClose(handle);
   if (ret != 0)
   {
      printf("persComRctClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close database: retval: [%d]", ret);


}
END_TEST



/*
 * Test to get the datasize for a key
 * write a key to cache, then the size of the data to that key from cache
 * Close and reopens the database to read the size to a key from file again.
 */
START_TEST(test_GetDataSize)
{
   char sysTimeBuffer[256];
   int size = 0, ret = 0;
   int handle = 0;
   struct tm* locTime;

   time_t t = time(0);
   locTime = localtime(&t);

   // write data
   snprintf(sysTimeBuffer, 128, "\"%s %d.%d.%d - %d:%.2d:%.2d Uhr\"", dayOfWeek[locTime->tm_wday], locTime->tm_mday,
            locTime->tm_mon, (locTime->tm_year + 1900), locTime->tm_hour, locTime->tm_min, locTime->tm_sec);

   //Cleaning up testdata folder
   remove("/tmp/size-localdb.db");

   handle = persComDbOpen("/tmp/size-localdb.db", 0x1); //create localdb.db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);

   //write key to cache
   ret = persComDbWriteKey(handle, "status/open_document", (char*) sysTimeBuffer, strlen(sysTimeBuffer));
   fail_unless(ret == strlen(sysTimeBuffer), "Wrong write size");

#if 1
   //get keysize from cache
   size = persComDbGetKeySize(handle, "status/open_document");
   //printf("=>=>=>=> soll: %d | ist: %d\n", strlen(sysTimeBuffer), size);
   fail_unless(size == strlen(sysTimeBuffer), "Invalid size read from cache");

   //persist cached data
   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);

   handle = persComDbOpen("/tmp/size-localdb.db", 0x1); //create localdb.db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);

   //get keysize from file
   size = persComDbGetKeySize(handle, "status/open_document");
   //printf("=>=>=>=> soll: %d | ist: %d\n", strlen(sysTimeBuffer), size);
   fail_unless(size == strlen(sysTimeBuffer), "Invalid size read from file");

   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close database: retval: [%d]", ret);

#endif
}
END_TEST

/*
 * Delete a key from local DB using the key value interface.
 * First write some keys, then read the keys. After that the keys get deleted.
 * A further read is performed and must fail.
 * The keys are inserted again and get persisted to file. the database gets reopened and the keys are deleted again.
 * The next read must fail again.
 */
START_TEST(test_DeleteDataLocalDB)
{
   int rval = 0;
   int handle = 0;
   unsigned char buffer[READ_SIZE] = { 0 };
   char write1[READ_SIZE] = { 0 };
   char sysTimeBuffer[256];
   struct tm* locTime;

   char write2[READ_SIZE] = { 0 };
   char key[128] = { 0 };

#if 1

   time_t t = time(0);
   locTime = localtime(&t);

   // write data
   snprintf(sysTimeBuffer, 128, "\"%s %d.%d.%d - %d:%.2d:%.2d Uhr\"", dayOfWeek[locTime->tm_wday], locTime->tm_mday,
            locTime->tm_mon, (locTime->tm_year + 1900), locTime->tm_hour, locTime->tm_min, locTime->tm_sec);

   //Cleaning up testdata folder
   remove("/tmp/delete-localdb.db");

   handle = persComDbOpen("/tmp/delete-localdb.db", 0x1); //create test.db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", handle);

   snprintf(write1, 128, "%s %s", "/70", sysTimeBuffer);

   //write to cache
   int i = 0;
   for(i=0; i< 300; i++)
   {
      snprintf(key, 128, "key%d",i);
      snprintf(write2, 128, "DATA-%d",i );
      rval = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
      fail_unless(rval == strlen(write2) , "Wrong write size while inserting in cache");
   }

   //read from cache must work
   for(i=0; i< 300; i++)
   {
      snprintf(key, 128, "key%d",i);
      snprintf(write2, 128, "DATA-%d",i );
      rval = persComDbReadKey(handle, key, (char*) buffer, strlen(write2));
      fail_unless(rval == strlen(write2), "Wrong read size while reading from cache");
   }

   // mark some data in cache as deleted
   for(i=0; i < 6; i++) //key0 - key5
   {
      snprintf(key, 128, "key%d",i);
      snprintf(write2, 128, "DATA-%d",i );
      rval = persComDbDeleteKey(handle, key);
      fail_unless(rval >= 0, "Failed to delete key: %s", key);
   }

   // after deleting the keys in cache, reading from key0 - key5 must fail now for these keys
   for(i=0; i< 300; i++)
   {
      snprintf(key, 128, "key%d",i);
      snprintf(write2, 128, "DATA-%d",i );
      if(i < 6)
      {
         rval = persComDbReadKey(handle, key, (char*) buffer, READ_SIZE);
         fail_unless(rval < 0, "Read form key [%s] works, but should fail",key);
      }
      else
      {
         rval = persComDbReadKey(handle, key, (char*) buffer, strlen(write2));
         fail_unless(rval == strlen(write2), "Wrong read size while reading from cache");
      }
   }

   //persist data to file (Dlt output must show error for writeback of deleted keys (not found because they do not exist in file yet)
   rval = persComDbClose(handle);
   if (rval != 0)
   {
      printf("persComDbClose() failed: [%d] \n", rval);
   }
   fail_unless(rval == 0, "Failed to close cached database: retval: [%d]", rval);

   //open database again and  write keys to cache that get persisted to file afterwards
   handle = persComDbOpen("/tmp/delete-localdb.db", 0x1); //create test.db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", handle);


   //write data again which gets persisted to file afterwards
   //write data to cache
   for(i=0; i< 300; i++)
   {
      snprintf(key, 128, "key%d",i);
      snprintf(write2, 128, "DATA-%d",i );
      rval = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
      fail_unless(rval == strlen(write2) , "Wrong write size while inserting in cache");
   }

   // read data from cache must work
   for(i=0; i< 300; i++)
   {
      snprintf(key, 128, "key%d",i);
      snprintf(write2, 128, "DATA-%d",i );
      rval = persComDbReadKey(handle, key, (char*) buffer, strlen(write2));
      fail_unless(rval == strlen(write2), "Wrong read size while reading from cache");
   }

   //persist data to file
   rval = persComDbClose(handle);
   if (rval != 0)
   {
      printf("persComDbClose() failed: [%d] \n", rval);
   }
   fail_unless(rval == 0, "Failed to close cached database: retval: [%d]", rval);

   //reopen database and read persisted keys
   handle = persComDbOpen("/tmp/delete-localdb.db", 0x1); //create test.db if not present
   fail_unless(handle >= 0, "Failed to open lDB: retval: [%d]", handle);

   // read data from file must work
   for(i=0; i< 300; i++)
   {
      snprintf(key, 128, "key%d",i);
      snprintf(write2, 128, "DATA-%d",i );
      rval = persComDbReadKey(handle, key, (char*) buffer, strlen(write2));
      fail_unless(rval == strlen(write2), "Wrong read size while reading from file for key: %s", key);
   }

   //delete keys (request to delete gets stored in cache)
   // mark some data in cache as deleted
   for(i=0; i < 6; i++) //key0 - key5
   {
      snprintf(key, 128, "key%d",i);
      snprintf(write2, 128, "DATA-%d",i );
      rval = persComDbDeleteKey(handle, key);
      fail_unless(rval >= 0, "Failed to delete key: %s", key);
   }

   // after deleting the keys in cache, reading from key0 - key5 must fail now for these keys
   for(i=0; i< 300; i++)
   {
      snprintf(key, 128, "key%d",i);
      snprintf(write2, 128, "DATA-%d",i );
      if(i < 6)
      {
         rval = persComDbReadKey(handle, key, (char*) buffer, READ_SIZE);
         fail_unless(rval < 0, "Read key [%s] from cache works, but should fail",key);
      }
      else
      {
         rval = persComDbReadKey(handle, key, (char*) buffer, strlen(write2));
         fail_unless(rval == strlen(write2), "Wrong read size while reading from cache for key: %s", key);
      }
   }


   //delete keys in writeback at close
   rval = persComDbClose(handle);
   if (rval != 0)
   {
      printf("persComDbClose() failed: [%d] \n", rval);
   }
   fail_unless(rval == 0, "Failed to close database: retval: [%d]", rval);

   //reopen database and try to read the deleted keys (must fail now)
   handle = persComDbOpen("/tmp/delete-localdb.db", 0x1); //create test.db if not present
   fail_unless(handle >= 0, "Failed to open lDB: retval: [%d]", handle);


   // after deleting the keys in cache, reading from key0 - key5 must fail now for these keys
   for(i=0; i< 300; i++)
   {
      snprintf(key, 128, "key%d",i);
      snprintf(write2, 128, "DATA-%d",i );
      if(i < 6)
      {
         rval = persComDbReadKey(handle, key, (char*) buffer, READ_SIZE);
         fail_unless(rval < 0, "Read key [%s] from file works, but should fail",key);
      }
      else
      {
         rval = persComDbReadKey(handle, key, (char*) buffer, strlen(write2));
         fail_unless(rval == strlen(write2), "Wrong read size while reading from file");
      }
   }


   rval = persComDbClose(handle);
   if (rval != 0)
   {
      printf("persComDbClose() failed: [%d] \n", rval);
   }
   fail_unless(rval == 0, "Failed to close database: retval: [%d]", rval);

   //reopen the database to write a key again into cache that must reuse the already deleted slot in the file when closing the database
   handle = persComDbOpen("/tmp/delete-localdb.db", 0x1); //create test.db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", handle);

   // write a key again to test if slot in file is reused (CONTENT OF offset == 4  in kissdb.c)
   rval = persComDbWriteKey(handle, "key1", (char*) write1, strlen(write1));
   fail_unless(rval == strlen(write1), "Wrong write size");

   // write a key again to test if slot in file is reused (CONTENT OF offset == 4  in kissdb.c)
   rval = persComDbWriteKey(handle, "key1", (char*) write1, strlen(write1));
   fail_unless(rval == strlen(write1), "Wrong write size");


   rval = persComDbClose(handle);
   if (rval != 0)
   {
      printf("persComDbClose() failed: [%d] \n", rval);
   }
   fail_unless(rval == 0, "Failed to close database: retval: [%d]", rval);

   //reopen the database to read key1 (must work)
   handle = persComDbOpen("/tmp/delete-localdb.db", 0x1); //create test.db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", handle);

   rval = persComDbReadKey(handle, "key1", (char*) buffer, READ_SIZE);
   fail_unless(rval == strlen(write1), "Wrong read size for key1");


   rval = persComDbClose(handle);
   if (rval != 0)
   {
      printf("persComDbClose() failed: [%d] \n", rval);
   }
   fail_unless(rval == 0, "Failed to close last database: retval: [%d]", rval);

   //Cleaning up testdata folder
   remove("/tmp/delete-localdb.db");

   //Open a new empty database and try to delete a non existent key
   handle = persComDbOpen("/tmp/delete-localdb.db", 0x1); //create test.db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", handle);

   //try to delete non existent keys
   for(i=0; i < 6; i++) //key0 - key5
   {
      snprintf(key, 128, "non-existent-key%d",i);
      snprintf(write2, 128, "DATA-%d",i );
      rval = persComDbDeleteKey(handle, key);
      fail_unless(rval < 0, "Deleting key %s works, but should fail!", key);
   }

   rval = persComDbClose(handle);
   if (rval != 0)
   {
      printf("persComDbClose() failed: [%d] \n", rval);
   }
   fail_unless(rval == 0, "Failed to close last database: retval: [%d]", rval);

#endif
}
END_TEST


/*
 * Delete a key from local DB using the key value interface.
 * First read a from a key, the delte the key
 * and then try to read again. The Last read must fail.
 */
START_TEST(test_DeleteDataRct)
{
   int rval = 0;
   int handle = 0;
   char sysTimeBuffer[256];
   struct tm* locTime;
   PersistenceConfigurationKey_s psConfig, psConfig_out;
   psConfig.policy = PersistencePolicy_wt;
   psConfig.storage = PersistenceStorage_local;
   psConfig.type = PersistenceResourceType_key;
   psConfig.permission = PersistencePermission_ReadWrite;

#if 1

   time_t t = time(0);
   locTime = localtime(&t);

   // write data
   snprintf(sysTimeBuffer, 128, "\"%s %d.%d.%d - %d:%.2d:%.2d Uhr\"", dayOfWeek[locTime->tm_wday], locTime->tm_mday,
            locTime->tm_mon, (locTime->tm_year + 1900), locTime->tm_hour, locTime->tm_min, locTime->tm_sec);

   //Cleaning up testdata folder
   remove("/tmp/delete-rct.db");

   handle = persComRctOpen("/tmp/delete-rct.db", 0x1); //create db if not present
   fail_unless(handle >= 0, "Failed to create non existent RCT: retval: [%d]", handle);

   memset(psConfig.custom_name, 0, sizeof(psConfig.custom_name));
   memset(psConfig.customID, 0, sizeof(psConfig.customID));
   memset(psConfig.reponsible, 0, sizeof(psConfig.reponsible));

   psConfig.max_size = 12345;
   char custom_name[PERS_RCT_MAX_LENGTH_CUSTOM_NAME] = "this is the custom name";
   char custom_ID[PERS_RCT_MAX_LENGTH_CUSTOM_ID] = "this is the custom ID";
   char responsible[PERS_RCT_MAX_LENGTH_RESPONSIBLE] = "this is the responsible";

   strncpy(psConfig.custom_name, custom_name, strlen(custom_name));
   strncpy(psConfig.customID, custom_ID, strlen(custom_ID));
   strncpy(psConfig.reponsible, responsible, strlen(responsible));

   //write key to cache
   rval = persComRctWrite(handle, "key_to_delete", &psConfig);
   fail_unless(rval == sizeof(psConfig), "Wrong write size in cache");

   memset(psConfig_out.custom_name, 0, sizeof(psConfig_out.custom_name));
   memset(psConfig_out.customID, 0, sizeof(psConfig_out.customID));
   memset(psConfig_out.reponsible, 0, sizeof(psConfig_out.reponsible));

   //read from cache
   rval = persComRctRead(handle, "key_to_delete", &psConfig_out);
   fail_unless(rval == sizeof(psConfig), "Wrong read size from cache");


//                        printf("Custom ID        : %s\n", psConfig_out.customID );
//                        printf("Custom Name      : %s\n", psConfig_out.custom_name );
//                        printf("reponsible       : %s\n", psConfig_out.reponsible );
//                        printf("max_size         : %d\n", psConfig_out.max_size );
//                        printf("permission       : %d\n", psConfig_out.permission );
//                        printf("type             : %d\n", psConfig_out.type );
//                        printf("storage          : %d\n", psConfig_out.storage );
//                        printf("policy           : %d\n", psConfig_out.policy );


   fail_unless(strncmp(psConfig.customID, psConfig_out.customID, strlen(psConfig_out.customID)) == 0,
                 "Buffer not correctly read");
   fail_unless(strncmp(psConfig.custom_name, psConfig_out.custom_name, strlen(psConfig_out.custom_name)) == 0,
                 "Buffer not correctly read");
   fail_unless(strncmp(psConfig.reponsible, psConfig_out.reponsible, strlen(psConfig_out.reponsible)) == 0,
                 "Buffer not correctly read");
   fail_unless(psConfig.max_size == psConfig_out.max_size, "Buffer not correctly read");
   fail_unless(psConfig.permission == psConfig_out.permission, "Buffer not correctly read");
   fail_unless(psConfig.policy == psConfig_out.policy, "Buffer not correctly read");
   fail_unless(psConfig.storage == psConfig_out.storage, "Buffer not correctly read");
   fail_unless(psConfig.type == psConfig_out.type, "Buffer not correctly read");

   // mark key in cache as deleted
   rval = persComRctDelete(handle, "key_to_delete");
   fail_unless(rval >= 0, "Failed to delete key");

   // after deleting the key, reading from key in cache must fail now!
   rval = persComRctRead(handle, "key_to_delete", &psConfig_out);
   fail_unless(rval < 0, "Read form key [key_to_delete] works, but should fail");

   //write data again to cache
   //write key to cache which already exists in database file
   rval = persComRctWrite(handle, "key_to_delete", &psConfig);
   fail_unless(rval == sizeof(psConfig), "Wrong write size in cache");

   //read from cache must work
   rval = persComRctRead(handle, "key_to_delete", &psConfig_out);
   fail_unless(rval == sizeof(psConfig), "Wrong read size from cache");


   rval = persComRctClose(handle);
   if (rval != 0)
   {
      printf("persComRctClose() failed: [%d] \n", rval);
   }
   fail_unless(rval == 0, "Failed to close database: retval: [%d]", rval);

   handle = persComRctOpen("/tmp/delete-rct.db", 0x1); //create db if not present
   fail_unless(handle >= 0, "Failed to create non existent RCT: retval: [%d]", handle);

   // mark key in cache as deleted
   rval = persComRctDelete(handle, "key_to_delete");
   fail_unless(rval >= 0, "Failed to delete key");

   // after deleting the key, reading from key must fail now!
   rval = persComRctRead(handle, "key_to_delete", &psConfig_out);
   fail_unless(rval < 0, "Read form key [key_to_delete] works, but should fail");

   // try to delete a non existent key (must fail)
   rval = persComRctDelete(handle, "key_to_delete_not_present");
   fail_unless(rval < 0, "Deleting key [key_to_delete_not_present] works, but should fail!");

   // insert this key in cache
   rval = persComRctWrite(handle, "key_to_delete_not_present", &psConfig);
   fail_unless(rval == sizeof(psConfig), "Wrong write size in cache");

   //read from cache must work
   rval = persComRctRead(handle, "key_to_delete_not_present", &psConfig_out);
   fail_unless(rval == sizeof(psConfig), "Wrong read size from cache");

   // try to delete a key in cache that is not present in file
   rval = persComRctDelete(handle, "key_to_delete_not_present");
   fail_unless(rval >= 0, "Deleting key from cache [key_to_delete_not_present] failed!");

   //persist data to file (Dlt output must show error for writeback of deleted keys (not found because they do not exist in file yet)
   rval = persComRctClose(handle);
   if (rval != 0)
   {
      printf("persComRctClose() failed: [%d] \n", rval);
   }
   fail_unless(rval == 0, "Failed to close database: retval: [%d]", rval);


#endif
}
END_TEST



/*
 *
 *
 */
START_TEST(test_CachedConcurrentAccess)
{
   int pid;

   //Cleaning up testdata folder
   remove("/tmp/cached-concurrent.db");

   pid = fork();
   if (pid == 0)
   {
      DLT_REGISTER_APP("PCOt", "tests the persistence common object library");

      /*child*/
      //printf("Started child process with PID: [%d] \n", pid);
      int handle = 0;
      int ret = 0;
      char childSysTimeBuffer[256] = { 0 };
      char key[128] = { 0 };
      char write2[READ_SIZE] = { 0 };
      int i =0;

      snprintf(childSysTimeBuffer, 256, "%s", "1");

      //wait so that father has already opened the db
      sleep(3);

      //open db after father (in order to use the hashtable in shared memory)
      handle = persComDbOpen("/tmp/cached-concurrent.db", 0x0); //open existing test.db if not present
      fail_unless(handle >= 0, "Child failed to create non existent lDB: retval: [%d]", ret);

      //read the new key written by the father from cache
      for(i=0; i< 200; i++)
      {
         snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
         snprintf(write2, 128, "DATA-%d",i );
         ret = persComDbReadKey(handle, key, (char*) childSysTimeBuffer, 256);
         //printf("Child Key Read from Cache: %s ------: %s  -------------- returnvalue: %d\n",key, childSysTimeBuffer, ret);
         fail_unless(ret == strlen(write2), "Child: Wrong read size");
      }

      //write to test if cache can be accessed
      ret = persComDbWriteKey(handle, "write-test-concurrent", "123456", 6);
      //printf("Father persComDbWriteKey: %s  -- returnvalue: %d \n", key, ret);
      fail_unless(ret == 6 , "CHILD: Wrong write size returned: %d", ret);

      //close database for child instance
      ret = persComDbClose(handle);
      if (ret != 0)
      {
         printf("persComDbClose() failed: [%d] \n", ret);
      }
      fail_unless(ret == 0, "Child failed to close database: retval: [%d]", ret);

      DLT_UNREGISTER_APP();
      _exit(EXIT_SUCCESS);
   }
   else if (pid > 0)
   {
      /*parent*/
      //printf("Started father process with PID: [%d] \n", pid);
      int handle = 0;
      int ret = 0;
      char write2[READ_SIZE] = { 0 };
      char key[128] = { 0 };
      char sysTimeBuffer[256] = { 0 };
      int i =0;

      handle = persComDbOpen("/tmp/cached-concurrent.db", 0x1); //create test.db if not present
      fail_unless(handle >= 0, "Father failed to create non existent lDB: retval: [%d]", ret);

      //Write data to cache (cache gets created here)
      for(i=0; i< 200; i++)
      {
         snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
         snprintf(write2, 128, "DATA-%d",i);
         ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
         //printf("Father persComDbWriteKey: %s  -- returnvalue: %d \n", key, ret);
         fail_unless(ret == strlen(write2), "Father: Wrong write size");
      }
      //read data from cache
      for(i=0; i< 200; i++)
      {
         snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
         snprintf(write2, 128, "DATA-%d",i );
         ret = persComDbReadKey(handle, key, (char*) sysTimeBuffer, 256);
         //printf("Father Key Read from key: %s ------: %s  -------------- returnvalue: %d\n",key, sysTimeBuffer, ret);
         fail_unless(ret == strlen(write2), "Father: Wrong read size");
      }

      printf("INFO: Waiting for child process to exit ... \n");

      //wait for child exiting
      int status;
      (void) waitpid(pid, &status, 0);


      //TEST if addresses have changed after child has written his data
      ret = persComDbWriteKey(handle, "write-test-FATHER", "123456", 6);
      //test


      //close database for father instance (closes the cache)
      ret = persComDbClose(handle);
      if (ret != 0)
      {
         printf("persComDbClose() failed: [%d] \n", ret);
      }
      fail_unless(ret == 0, "Father failed to close database: retval: [%d]", ret);

      _exit(EXIT_SUCCESS);
   }
}
END_TEST




/*
 *
 *
 */
START_TEST(test_CachedConcurrentAccess2)
{
   int pid;

   //Cleaning up testdata folder
   remove("/tmp/cached-concurrent2.db");

   pid = fork();
   if (pid == 0)
   {
      DLT_REGISTER_APP("PCOt", "tests the persistence common object library");

      /*child*/
      printf("Started child process with PID: [%d] \n", pid);
      int handle = 0;
      int ret = 0;
      char childSysTimeBuffer[256] = { 0 };
      char key[128] = { 0 };
      char write2[READ_SIZE] = { 0 };
      int i =0;

      snprintf(childSysTimeBuffer, 256, "%s", "1");

      //open database initially (CREATOR)
      handle = persComDbOpen("/tmp/cached-concurrent2.db", 0x1); //create test.db if not present
      fail_unless(handle >= 0, "Child failed to create non existent lDB: retval: [%d]", ret);


      for(i=0; i< 200; i++)
      {
         snprintf(key, 128, "CHILD_Key_%d_%d",i,i*i);
         snprintf(write2, 128, "DATA-%d",i);
         ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
         //printf("Father persComDbWriteKey: %s  -- returnvalue: %d \n", key, ret);
         fail_unless(ret == strlen(write2), "Child: Wrong write size");
      }

      //read the new key written by the father from cache
//      for(i=0; i< 200; i++)
//      {
//         snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
//         snprintf(write2, 128, "DATA-%d",i );
//         ret = persComDbReadKey(handle, key, (char*) childSysTimeBuffer, 256);
//         //printf("Child Key Read from Cache: %s ------: %s  -------------- returnvalue: %d\n",key, childSysTimeBuffer, ret);
//         fail_unless(ret == strlen(write2), "Child: Wrong read size");
//      }

      //write in order to create cache
      ret = persComDbWriteKey(handle, "write-test-concurrent-CHILD", "123456", 6);
      printf("Child persComDbWriteKey: write-test-concurrent-CHILD  -- returnvalue: %d \n", ret);
      fail_unless(ret == 6 , "CHILD: Wrong write size returned: %d", ret);

      //wait until child has also accessed the cache
      sleep(2);

      //close database for child instance (CREATOR but not the last instance using the database)
      printf("Child (Creator) closes database! \n");
      ret = persComDbClose(handle);
      if (ret != 0)
      {
         printf("persComDbClose() failed: [%d] \n", ret);
      }
      fail_unless(ret == 0, "Child failed to close database: retval: [%d]", ret);

      DLT_UNREGISTER_APP();
      _exit(EXIT_SUCCESS);
   }
   else if (pid > 0)
   {
      /*parent*/
      printf("Started father process with PID: [%d] \n", pid);
      int handle = 0;
      int ret = 0;
      char write2[READ_SIZE] = { 0 };
      char key[128] = { 0 };
      char sysTimeBuffer[256] = { 0 };
      int i =0;

      //wait until child (CREATOR) has opened the database
      sleep(1);

      handle = persComDbOpen("/tmp/cached-concurrent2.db", 0x0); //open existing database
      fail_unless(handle >= 0, "Father failed to create non existent lDB: retval: [%d]", ret);


      //Write data to already created cache
      for(i=0; i< 200; i++)
      {
         snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
         snprintf(write2, 128, "DATA-%d",i);
         ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
         //printf("Father persComDbWriteKey: %s  -- returnvalue: %d \n", key, ret);
         fail_unless(ret == strlen(write2), "Father: Wrong write size");
      }
      //read data from cache
      for(i=0; i< 200; i++)
      {
         snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
         snprintf(write2, 128, "DATA-%d",i );
         ret = persComDbReadKey(handle, key, (char*) sysTimeBuffer, 256);
         //printf("Father Key Read from key: %s ------: %s  -------------- returnvalue: %d\n",key, sysTimeBuffer, ret);
         fail_unless(ret == strlen(write2), "Father: Wrong read size");
      }

      printf("INFO: Waiting for child process to exit ... \n");

      //wait for child exiting
      int status;
      (void) waitpid(pid, &status, 0);


      //TEST if addresses have changed after child has written his data
      ret = persComDbWriteKey(handle, "write-test-FATHER", "123456", 6);
      //test


      //close database for father instance that has NOT created the database and cache (last instance that must do the writeback)
      printf("Father (not the Creator) closes database! \n");
      ret = persComDbClose(handle);
      if (ret != 0)
      {
         printf("persComDbClose() failed: [%d] \n", ret);
      }
      fail_unless(ret == 0, "Father failed to close database: retval: [%d]", ret);

      _exit(EXIT_SUCCESS);
   }
}
END_TEST




START_TEST(test_CacheSize)
{
   unsigned char buffer2[PERS_DB_MAX_SIZE_KEY_DATA] = { 1 };
   int handle = 0;
   int i, k, ret = 0;
   int maxKeys = 2169;
   char dataBufer[PERS_DB_MAX_SIZE_KEY_DATA] = { 1 };
   char key[128] = { 0 };
   char path[128] = { 0 };
   int handles[100] = { 0 };
   int writings = 2173;
   int databases = 1;

   for (k = 0; k < databases; k++)
   {
      snprintf(path, 128, "/tmp/cacheSize-%d.db", k);
      //Cleaning up testdata folder
      remove(path);
   }

   //use maximum allowed data size
   for (i = 0; i < PERS_DB_MAX_SIZE_KEY_DATA; i++)
   {
      dataBufer[i] = 'x';
   }
   dataBufer[PERS_DB_MAX_SIZE_KEY_DATA-1] = '\0';

   //fill k databases with i key /value pairs
   for (k = 0; k < databases; k++)
   {
      snprintf(path, 128, "/tmp/cacheSize-%d.db", k);
      handle = persComDbOpen(path, 0x1); //create test.db if not present
      handles[k] = handle;
      fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);

      //write data to cache
      for (i = 0; i < writings; i++)
      {
         snprintf(key, 128, "Key_in_loop_%d_%d", i, i * i);
         ret = persComDbWriteKey(handle, key, (char*) dataBufer, strlen(dataBufer));
         //printf("Writing Key: %s | Retval: %d \n", key, ret);

         if( i >= maxKeys) //write must fail (adapt maxKeySize if cache size is increased or item size gets decreased)
         {
            fail_unless(ret < 0 , "Insert in cache works but should fail!: %d", ret);
         }
         else //write must work
         {
            fail_unless(ret == strlen(dataBufer) , "Wrong write size while inserting in cache");
         }
      }

      //read data from cache
      for (i = 0; i < writings; i++)
      {
         snprintf(key, 128, "Key_in_loop_%d_%d", i, i * i);
         ret = persComDbReadKey(handle, key, (char*) buffer2, PERS_DB_MAX_SIZE_KEY_DATA);
         //printf("read from key: %s | Retval: %d \n", key, ret);

         if( i >= maxKeys) //read must fail
         {
            fail_unless(ret < 0, "Read from cache works, but should fail!: %d", ret);
         }
         else //read must work
         {
            fail_unless(ret == strlen(dataBufer), "Wrong read size while reading from cache");
         }
      }
   }

   //sleep to look for memory consumption of cache
   //sleep(15);
   //Close k databases in order to persist the data
   for (k = 0; k < databases; k++)
   {
      ret = persComDbClose(handles[k]);
      if (ret != 0)
      {
         printf("persComDbClose() failed: [%d] \n", ret);
      }
      fail_unless(ret == 0, "Failed to close database with number %d: retval: [%d]", k, ret);
   }
}
END_TEST




START_TEST(test_BadParameters)
{
   //perscomdbopen
   char* path = NULL;
   int ret = 0;

   //percomDBwrite test
   char buffer[16384] = { 0 };
   char* Badbuffer = NULL;
   char* key = NULL;
   char* data = NULL;

   //rct write
   PersistenceConfigurationKey_s* BadPsConfig = NULL;
   PersistenceConfigurationKey_s  psConfig;

   ret = persComDbOpen("6l3HrKvT9TmXdTbqF4mc3N38llpbKkn2qMhdiLOlwXnY7H09ZewvQG80uyLr8sg0by0oD9UXNOm9OHvXl8zf7vKosu0M90Aau6WFqELDJl6OYr3xPYPH59o7AvixDMQXlNrPUUTdluU24TEFEiTVhcRcWJDoxlL6LHg1u9p3pNURI9GKmAsXDHovXXrvwP3qSjDYB0gMhvEvfpDI5oy8vb3Frz81zZmKuHsx9GQi0xWTB5n6grRH9TvcJW7F1yu7",
                       0x0); //Pathname exceeds 255 chars
   fail_unless(ret < 0, "Open local db with too long path length works, but should fail: retval: [%d]", ret);

   ret = persComDbOpen(path, 0x1); //create test.db if not present
   fail_unless(ret < 0, "Open local db with bad pathname works, but should fail: retval: [%d]", ret);

   ret = persComDbClose(-1);
   fail_unless(ret < 0, "Closing the local database with negative handle works, but should fail: retval: [%d]", ret);

   ret = persComRctOpen("6l3HrKvT9TmXdTbqF4mc3N38llpbKkn2qMhdiLOlwXnY7H09ZewvQG80uyLr8sg0by0oD9UXNOm9OHvXl8zf7vKosu0M90Aau6WFqELDJl6OYr3xPYPH59o7AvixDMQXlNrPUUTdluU24TEFEiTVhcRcWJDoxlL6LHg1u9p3pNURI9GKmAsXDHovXXrvwP3qSjDYB0gMhvEvfpDI5oy8vb3Frz81zZmKuHsx9GQi0xWTB5n6grRH9TvcJW7F1yu7",
                        0x0); //Pathname exceeds 255 chars
   fail_unless(ret < 0, "Open RCT db with too long path length works, but should fail: retval: [%d]", ret);

   ret = persComRctOpen(path, 0x1); //create test.db if not present
   fail_unless(ret < 0, "Open RCT db with bad pathname works, but should fail: retval: [%d]", ret);

   ret = persComRctClose(-1);
   fail_unless(ret < 0, "Closing the RCT database with negative handle works, but should fail: retval: [%d]", ret);

   ret = persComDbWriteKey(1, "CteKh3FTonalS4AlOaEruzUbgAP9fryYJLCykq5tTPQkPrHEcV9p6akxa6TuF9gqnJu5iCEyxMUu17QhTP7sYgFwFKU1qqNMcCmps8WcpWDR2oCnjqdaBtATL2A36q6QV", "data", 4); //key too long
   fail_unless(ret < 0 , "Writing with wrong keylength works, but should fail");

   ret = persComDbWriteKey(-1, "key", "data", 4); //negative handle
   fail_unless(ret < 0 , "Writing with negative handle works, but should fail");

   ret = persComDbWriteKey(1, key, "data", 4); //key is NULL
   fail_unless(ret < 0 , "Writing key that is NULL works, but should fail");

   ret = persComDbWriteKey(1, "key", data, 4); //data is NULL
   fail_unless(ret < 0 , "Writing data that is NULL works, but should fail");

   ret = persComDbWriteKey(1, "key", "data", -1254); //datasize is negative
   fail_unless(ret < 0 , "Writing with negative datasize works, but should fail");

   ret = persComDbWriteKey(1, "key", "data", 0); //datasize is zero
   fail_unless(ret < 0 , "Writing with zero datasize works, but should fail");

   ret = persComDbWriteKey(1, "key", "data", 16385); //datasize too big
   fail_unless(ret < 0 , "Writing with too big datasize works, but should fail");

   ret = persComRctWrite(1, "CteKh3FTonalS4AlOaEruzUbgAP9fryYJLCykq5tTPQkPrHEcV9p6akxa6TuF9gqnJu5iCEyxMUu17QhTP7sYgFwFKU1qqNMcCmps8WcpWDR2oCnjqdaBtATL2A36q6QV", &psConfig); //key too long
   fail_unless(ret < 0 , "Writing RCT with wrong keylength works, but should fail");

   ret = persComRctWrite(-1, "key", &psConfig); //negative handle
   fail_unless(ret < 0 , "Writing RCT with negative handle works, but should fail");

   ret = persComRctWrite(1, key, &psConfig); //key is NULL
   fail_unless(ret < 0 , "Writing RCT key that is NULL works, but should fail");

   ret = persComRctWrite(1, "key", BadPsConfig); //data is NULL
   fail_unless(ret < 0 , "Writing RCT data that is NULL works, but should fail");

   ret = persComDbReadKey(1, "CteKh3FTonalS4AlOaEruzUbgAP9fryYJLCykq5tTPQkPrHEcV9p6akxa6TuF9gqnJu5iCEyxMUu17QhTP7sYgFwFKU1qqNMcCmps8WcpWDR2oCnjqdaBtATL2A36q6QV", "data", 4); //key too long
   fail_unless(ret < 0 , "Reading with wrong keylength works, but should fail");

   ret = persComDbReadKey(-1, "key", buffer, 4); //negative handle
   fail_unless(ret < 0 , "Reading with negative handle works, but should fail");

   ret = persComDbReadKey(1, key, buffer, 4); //key is NULL
   fail_unless(ret < 0 , "Reading key that is NULL works, but should fail");

   ret = persComDbReadKey(1, "key", Badbuffer, 4); //data is NULL
   fail_unless(ret < 0 , "Reading data to buffer that is NULL works, but should fail");

   ret = persComDbReadKey(1, "key", buffer, -1254); //data buffer size is negative
   fail_unless(ret < 0 , "Reading with negative data buffer size works, but should fail");

   ret = persComDbReadKey(1, "key", buffer, 0); //data buffer size is zero
   fail_unless(ret < 0 , "Reading with zero data buffer size works, but should fail");

   ret = persComRctRead(1, "CteKh3FTonalS4AlOaEruzUbgAP9fryYJLCykq5tTPQkPrHEcV9p6akxa6TuF9gqnJu5iCEyxMUu17QhTP7sYgFwFKU1qqNMcCmps8WcpWDR2oCnjqdaBtATL2A36q6QV", &psConfig); //key too long
   fail_unless(ret < 0 , "Reading RCT with wrong keylength works, but should fail");

   ret = persComRctRead(-1, "key", &psConfig); //negative handle
   fail_unless(ret < 0 , "Reading RCT with negative handle works, but should fail");

   ret = persComRctRead(1, key, &psConfig); //key is NULL
   fail_unless(ret < 0 , "Reading RCT key that is NULL works, but should fail");

   ret = persComRctRead(1, "key", BadPsConfig); //data is NULL
   fail_unless(ret < 0 , "Reading RCT data to buffer that is NULL works, but should fail");

   ret = persComDbGetSizeKeysList(-1);
   fail_unless(ret < 0 , "Reading keylist size with negative handle works, but should fail");

   ret = persComRctGetSizeResourcesList(-1);
   fail_unless(ret < 0 , "Reading RCT resourcelist size with negative handle works, but should fail");

   ret = persComDbGetKeySize(1, "CteKh3FTonalS4AlOaEruzUbgAP9fryYJLCykq5tTPQkPrHEcV9p6akxa6TuF9gqnJu5iCEyxMUu17QhTP7sYgFwFKU1qqNMcCmps8WcpWDR2oCnjqdaBtATL2A36q6QV"); //key too long
   fail_unless(ret < 0 , "Reading Size with wrong keylength works, but should fail");

   ret = persComDbGetKeySize(-1, "key"); //negative handle
   fail_unless(ret < 0 , "Reading Size with negative handle works, but should fail");

   ret = persComDbGetKeySize(1, key); //key is NULL
   fail_unless(ret < 0 , "Reading Size from key that is NULL works, but should fail");

   ret = persComDbGetKeysList(-1, buffer, 10); //
   fail_unless(ret < 0 , "Reading key list with negative handle works, but should fail");

   ret = persComDbGetKeysList(1, Badbuffer, 10); //
   fail_unless(ret < 0 , "Reading key list with readbuffer that is NULL works, but should fail");

   ret = persComDbGetKeysList(1, buffer, -1); //
   fail_unless(ret < 0 , "Reading key list with negative buffer size works, but should fail");

   ret = persComDbGetKeysList(1, buffer, 0); //
   fail_unless(ret < 0 , "Reading key list with zero buffer size works, but should fail");

   ret = persComRctGetResourcesList(-1, buffer, 10); //
   fail_unless(ret < 0 , "Reading RCT key list with negative handle works, but should fail");

   ret = persComRctGetResourcesList(1, Badbuffer, 10); //
   fail_unless(ret < 0 , "Reading RCT key list with readbuffer that is NULL works, but should fail");

   ret = persComRctGetResourcesList(1, buffer, -1); //
   fail_unless(ret < 0 , "Reading RCT key list with negative buffer size works, but should fail");

   ret = persComRctGetResourcesList(1, buffer, 0); //
   fail_unless(ret < 0 , "Reading RCT key list with zero buffer size works, but should fail");

   ret = persComDbDeleteKey(1, "CteKh3FTonalS4AlOaEruzUbgAP9fryYJLCykq5tTPQkPrHEcV9p6akxa6TuF9gqnJu5iCEyxMUu17QhTP7sYgFwFKU1qqNMcCmps8WcpWDR2oCnjqdaBtATL2A36q6QV");
   fail_unless(ret < 0 , "Deleting key with wrong keylength works, but should fail");

   ret = persComDbDeleteKey(1, key);
   fail_unless(ret < 0 , "Deleting key with key that is NULL works, but should fail");

   ret = persComDbDeleteKey(-1, "key");
   fail_unless(ret < 0 , "Deleting with negative handle works, but should fail");

   ret = persComRctDelete(1, "CteKh3FTonalS4AlOaEruzUbgAP9fryYJLCykq5tTPQkPrHEcV9p6akxa6TuF9gqnJu5iCEyxMUu17QhTP7sYgFwFKU1qqNMcCmps8WcpWDR2oCnjqdaBtATL2A36q6QV");
   fail_unless(ret < 0 , "Deleting RCT key with wrong keylength works, but should fail");

   ret = persComRctDelete(1, key);
   fail_unless(ret < 0 , "Deleting RCT key with key that is NULL works, but should fail");

   ret = persComRctDelete(-1, "key");
   fail_unless(ret < 0 , "Deleting RCT key with negative handle works, but should fail");
}
END_TEST


/*
 * This test first writes a valid database file.
 * Then the hashtable area of the database file is made corrupt
 * In the last step, the database is reopened again.
 * If the corrupt hashtables are idenfified and resbuilt correctly,
 * all keys that were written in the first step, must be readable.
 */
START_TEST(test_RebuildHashtables)
{
   int ret = 0;
   int handle = 0;
   char write2[READ_SIZE] = { 0 };
   char read[READ_SIZE] = { 0 };
   char key[128] = { 0 };
   int i =0;

   //Cleaning up testdata folder
   remove("/tmp/rebuild-hashtables.db");

#if 1
   memset(write2, 0 , sizeof(write2));
   snprintf(write2, 176 , "%s",
            "/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/");

   handle = persComDbOpen("/tmp/rebuild-hashtables.db", 0x1); //create test.db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);

   //write to cache
   for(i=0; i < 300; i++)
   {
      snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
      ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
      fail_unless(ret == strlen(write2) , "Wrong write size while inserting in cache");
   }

   //read from cache
   for(i=0; i < 300; i++)
   {
      snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
      memset(read, 0 , sizeof(read));
      ret = persComDbReadKey(handle, key, (char*) read, strlen(write2));
      fail_unless(ret == strlen(write2), "Wrong read size while reading from cache");
      fail_unless(memcmp(read, write2, sizeof(write2)) == 0, "Reading Data from Cache failed: Buffer not correctly read");
   }

   //persist data in cache to file
   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);

   //reopen to delete a key
   handle = persComDbOpen("/tmp/rebuild-hashtables.db", 0x1); //create test.db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);

   ret = persComDbDeleteKey(handle, "Key_in_loop_2_4");
   fail_unless(ret >= 0, "Failed to delete key");

   //persist deleted data in cache to file
   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);

   // IF DATABASE HEADER STRUCTURES OR KEY VALUE PAIR STORAGE CHANGES, the seek to offset part must be updated
   //open database and make data corrupt
   int fd;
   FILE* f;
   fd = open("/tmp/rebuild-hashtables.db", O_RDWR , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH  ); //gets closed when f is closed
   f = fdopen(fd, "w+b");

   uint64_t flag = 0x01;

   //seek to close failed flag and set it to 1 (to invoke check of hashtables)
   fseeko(f,16, SEEK_SET);
   fwrite(&flag,sizeof(uint64_t),1, f);

   //seek to data of hashtable area (to destroy a hashtable)
   fseeko(f,4105, SEEK_SET);
   fputc('x',f); //make data corrupt

   //seek to data of hashtable area
   fseeko(f,6802, SEEK_SET);
   fputc('x',f); //make data corrupt

   //seek to data of hashtable area
   fseeko(f,15995, SEEK_SET);
   fputc('x',f); //make data corrupt

   //destroy delimiters of a hashtable
   // destroy start delimiter of a hashtable - 655352
   fseeko(f,655352, SEEK_SET);
   fputc('x',f);






   //just make block B data corrupt- -> block A must be used for recovery
   fseeko(f,4841626, SEEK_SET);
   fputc('x',f);

   //destroy one  delimiter of datablock A --> Key_in_loop_222_49284 --> block A can be used for recovery if data is valid
   fseeko(f,1024002, SEEK_SET);
   fputc('x',f);

   //Destroy data of block A --> Key_in_loop_153_23409  --> block B must be used for recovery
   fseeko(f,16407, SEEK_SET);
   fputc('x',f); //just make block A data corrupt

   //destroy both delimiters of datablock A --> Key_in_loop_101_10201 --> block B must be used for recovery
   fseeko(f,827394, SEEK_SET);
   fputc('x',f);
   fseeko(f,835577, SEEK_SET);
   fputc('x',f);

   //also destroy both delimiters of last datablock A in file --> Key_in_loop_4_16  --> block B must be used for recovery
   fseeko(f,4964353, SEEK_SET);
   fputc('x',f);
   fseeko(f,4972537, SEEK_SET);
   fputc('x',f);

   //make block A and block B data corrupt --> Key_in_loop_31_961 --> recovery not possible
   fseeko(f,3834005, SEEK_SET);
   fputc('x',f);
   fseeko(f,3842201, SEEK_SET);
   fputc('x',f);

   //test with start AND end delimiter of hashtable destroyed --> recovery not possible
//   fseeko(f,4098, SEEK_SET);
//   fputc('y',f);
//   fseeko(f,16377, SEEK_SET);
//   fputc('y',f);

   fclose(f);

   //printf("reopen destroyed database\n");

   handle = persComDbOpen("/tmp/rebuild-hashtables.db", 0x1); //reopen database with corrupted hashtables and corrupted datablocks
   fail_unless(handle >= 0, "Failed to reopen existing lDB: retval: [%d]", ret);
   memset(read, 0 , sizeof(read));
   //read from database file must work if rebuild of hashtables was successful
   for(i=0; i < 300; i++)
   {
      //printf("read verification \n");
      if (i != 2 && i != 31) //do not expect successful read for deleted data or not recoverable data
      {
         snprintf(key, 128, "Key_in_loop_%d_%d", i, i * i);  //Key_in_loop_0_0
         memset(read, 0, sizeof(read));
         ret = persComDbReadKey(handle, key, (char*) read, strlen(write2));
         //printf("read key: %s returnval: %d Data: %s \n",key, ret, read);
         memset(read, 0, sizeof(read));
         fail_unless(ret == strlen(write2), "Wrong read size returned for key: %s \n", key);
      }
      else //expect read fail for unrecoverable data ( key_31) and for deleted data(
      {
         snprintf(key, 128, "Key_in_loop_%d_%d", i, i * i);  //Key_in_loop_0_0
         memset(read, 0, sizeof(read));
         ret = persComDbReadKey(handle, key, (char*) read, strlen(write2));
         memset(read, 0, sizeof(read));
         fail_unless(ret < 0 , "Key: <%s> could be read, but should not be readable!\n", key);
      }
   }

   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close database file: retval: [%d]", ret);

#endif
}
END_TEST



/*
 * In this test, the recovery of corrupted datablocks is tested.
 * First, a a valid database file gets written.
 * Then some datablocks in the data area of the database file is made corrupt.
 * In the last step, the database is reopened again.
 * If the corrupt data gets idenfified and recovered correctly,
 * all key value pairs that were written in the first step, must be readable.
 */
START_TEST(test_RecoverDatablocks)
{
   int ret = 0;
   int handle = 0;
   char write2[READ_SIZE] = { 0 };
   char read[READ_SIZE] = { 0 };
   char key[128] = { 0 };
   int i =0;

   //Cleaning up testdata folder
   remove("/tmp/recover-datablocks.db");

#if 1
   memset(write2, 0 , sizeof(write2));
   snprintf(write2, 176 , "%s",
            "/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/");


   handle = persComDbOpen("/tmp/recover-datablocks.db", 0x1); //create test.db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);


   //write to cache
   for(i=0; i < 300; i++)
   {
      snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
      ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
      fail_unless(ret == strlen(write2) , "Wrong write size while inserting in cache");
   }

   //read from cache
   for(i=0; i < 300; i++)
   {
      snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
      memset(read, 0 , sizeof(read));
      ret = persComDbReadKey(handle, key, (char*) read, strlen(write2));
      fail_unless(ret == strlen(write2), "Wrong read size while reading from cache");
      fail_unless(memcmp(read, write2, sizeof(write2)) == 0, "Reading Data from Cache failed: Buffer not correctly read");
   }

   //persist data in cache to file
   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);


   //printf("Database created, now destroying datablocks.....\n");

   // IF DATABASE HEADER STRUCTURES OR KEY VALUE PAIR STORAGE CHANGES, the seek to offset part must be updated
   //open database and make data corrupt
   int fd;
   FILE* f;
   fd = open("/tmp/recover-datablocks.db", O_RDWR , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH  ); //gets closed when f is closed
   f = fdopen(fd, "w+b");
   uint64_t flag = 0x01;

   //seek to close failed flag and set it to 1
   fseeko(f,16, SEEK_SET);
   fwrite(&flag,sizeof(uint64_t),1, f);

   //seek to data block A of key  Key_in_loop_153_23409
   fseeko(f,16407, SEEK_SET);
   fputc('x',f); //make key corrupt

   //seek to data block B of key: Key_in_loop_285_81225
   fseeko(f,356559, SEEK_SET);
   fputc('x',f); //make data corrupt

   //seek to data block B of key: Key_in_loop_125_15625
   fseeko(f,1212614, SEEK_SET);
   fputc('x',f); //make data corrupt

   //make both blocks corrupt of key: Key_in_loop_48_2304 --> DLT_LOG must show -> datablock recovery impossible -> both datablocks are invalid!

   //block A Key_in_loop_48_2304
   fseeko(f,51066, SEEK_SET);
   fputc('x',f); //make data corrupt

   //block B Key_in_loop_48_2304
   fseeko(f,61009, SEEK_SET);
   fputc('x',f); //make data corrupt

   fclose(f);

   handle = persComDbOpen("/tmp/recover-datablocks.db", 0x1); //reopen database with corrupted hashtables
   fail_unless(handle >= 0, "Failed to reopen existing lDB: retval: [%d]", ret);
   memset(read, 0 , sizeof(read));
   //read from database file must work if rebuild of hashtables was successful

   //printf("opening database finished, start reading to verify...\n");
   for(i=0; i < 300; i++)
   {
      snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);  //Key_in_loop_0_0
      memset(read, 0 , sizeof(read));
      ret = persComDbReadKey(handle, key, (char*) read, strlen(write2));
      //printf("read for key: %s returnval: %d Data: %s \n", key, ret, read);
      if(i == 48)
      {
         //printf("48: --> ret: %d \n",ret); //ret must be negative -> key 48 and 41 have same hash value
         fail_unless(ret != strlen(write2), "Read was possible but should not because both datablocks are corrupt!");
      }
      else
      {
         memset(read, 0 , sizeof(read));
         fail_unless(ret == strlen(write2), "Wrong read size");
      }
   }

   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close database file: retval: [%d]", ret);

#endif
}
END_TEST





/*
 * In this test, the access to databases through symlinks is tested
 * the symlink named "/tmp/symlink" points to the folder "/tmp"
 * The database file gets created under the path: "/tmp/symlink/symlink-localdb.db"
 * Keys get written and must also be readable again if the database under the symlink is reopened.
 */
START_TEST(test_LinkedDatabase)
{
   int ret = 0;
   int handle = 0;
   unsigned char readBuffer[READ_SIZE] = { 0 };
   char write2[READ_SIZE] = { 0 };
   char key[128] = { 0 };
   int i = 0;

   //Cleaning up testdata folder
   remove("/tmp/symlink-localdb.db");
   remove("/tmp/symlink");

   ret = symlink("/tmp","/tmp/symlink");
   fail_unless(ret == 0, "Failed to create symlink /tmp/symlink: [%s]", strerror(errno));

   handle = persComDbOpen("/tmp/symlink/symlink-localdb.db", 0x1); //create test.db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);

   //write keys to cache
   for(i=0; i< 50; i++)
   {
      snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
      memset(write2, 0, sizeof(write2));
      snprintf(write2, 128, "DATA-%d-%d",i,i*i );
      ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
      fail_unless(ret == strlen(write2), "Wrong write size");
   }

   //Read keys from cache
   for(i=0; i< 50; i++)
   {
      snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
      memset(write2, 0, sizeof(write2));
      snprintf(write2, sizeof(write2), "DATA-%d-%d",i,i*i );
      memset(readBuffer, 0, sizeof(readBuffer));
      ret = persComDbReadKey(handle, key, (char*) readBuffer, sizeof(readBuffer));
      fail_unless(ret == strlen(write2), "Wrong read size");
      fail_unless(memcmp(readBuffer, write2, sizeof(readBuffer)) == 0, "Reading Data from Cache failed: Buffer not correctly read");
   }

   //persist changed data for this lifecycle
   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: Cached Data was not written back: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);

   //open database again
   handle = persComDbOpen("/tmp/symlink/symlink-localdb.db", 0x1); //create test.db if not present
   fail_unless(handle >= 0, "Failed to reopen existing lDB: retval: [%d]", ret);

   //Read keys from database file
   for(i=0; i< 50; i++)
   {
      snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
      memset(write2, 0, sizeof(write2));
      snprintf(write2, sizeof(write2), "DATA-%d-%d",i,i*i );
      memset(readBuffer, 0, sizeof(readBuffer));
      ret = persComDbReadKey(handle, key, (char*) readBuffer, sizeof(readBuffer));
      fail_unless(ret == strlen(write2), "Wrong read size");
      fail_unless(memcmp(readBuffer, write2, sizeof(readBuffer)) == 0, "Reading Data from File failed: Buffer not correctly read");
   }

   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close database file: retval: [%d]", ret);

}
END_TEST




/*
 * Write a key value pair into the cache multiple times with same key but different data
 * Then close the database in order to persist the data.
 * After reopening the database, the last data written to cache must be returned for the key
 */
START_TEST(test_MultipleWrites)
{
   int ret = 0;
   int handle = 0;
   char write1[READ_SIZE] = { 0 };
   char write2[READ_SIZE] = { 0 };
   char read[READ_SIZE] = { 0 };
   char key[128] = { 0 };
   int i =0;

   //Cleaning up testdata folder
   remove("/tmp/multiple-writes.db");

#if 1

   handle = persComDbOpen("/tmp/multiple-writes.db", 0x1); //create test.db if not present
   fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);

   //use same key for all writes and reads
   snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
   snprintf(write1, 128, "%s", "1234567890");
   snprintf(write2, 128, "%s", "12345");

   //write 10 bytes to cache
   ret = persComDbWriteKey(handle, key, (char*) write1, strlen(write1));
   fail_unless(ret == strlen(write1) , "Wrong write size while inserting in cache");
   //write 5 bytes to cache
   ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
   fail_unless(ret == strlen(write2) , "Wrong write size while inserting in cache");

   //read must return 5 bytes from cache
   ret = persComDbReadKey(handle, key, (char*) read, READ_SIZE);
   //printf("Read from cache write 2: %s \n", read);
   fail_unless(ret == strlen(write2), "Wrong read size while reading from cache!");
   fail_unless(memcmp(read, write2, sizeof(write2)) == 0, "Reading Data from Cache failed: Buffer not correctly read!");

   //write 10 bytes to cache
   ret = persComDbWriteKey(handle, key, (char*) write1, strlen(write1));
   fail_unless(ret == strlen(write1) , "Wrong write size while inserting in cache");

   //read must return 10 bytes from cache
   ret = persComDbReadKey(handle, key, (char*) read, READ_SIZE);
   //printf("Read from cache write 1: %s \n", read);
   fail_unless(ret == strlen(write1), "Wrong read size while reading from cache!");
   fail_unless(memcmp(read, write1, sizeof(write1)) == 0, "Reading Data from Cache failed: Buffer not correctly read!");


   //persist data in cache to file
   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);


   handle = persComDbOpen("/tmp/multiple-writes.db", 0x1); //create test.db if not present
   fail_unless(handle >= 0, "Failed to reopen existing lDB: retval: [%d]", ret);
   //printf("open ok \n");

   //read from database file
   memset(read, 0, 1024);
   ret = persComDbReadKey(handle, key, (char*) read, strlen(write1));
   //printf("read: %d returns: %s \n", i, read);
   fail_unless(ret == strlen(write1), "Wrong read size");
   fail_unless(memcmp(read, write1, sizeof(write1)) == 0, "Reading Data from File failed: Buffer not correctly read");



   ret = persComDbClose(handle);
   if (ret != 0)
   {
      printf("persComDbClose() failed: [%d] \n", ret);
   }
   fail_unless(ret == 0, "Failed to close database file: retval: [%d]", ret);

#endif
}
END_TEST



START_TEST(test_WriteThrough)
{
   int pid;

      //Cleaning up testdata folder
      remove("/tmp/writethrough-concurrent.db");

      pid = fork();
      if (pid == 0)
      {
         DLT_REGISTER_APP("PCOt", "tests the persistence common object library");

         /*child*/
         //printf("Started child process with PID: [%d] \n", pid);
         int handle = 0;
         int ret = 0;
         char childSysTimeBuffer[256] = { 0 };
         char key[128] = { 0 };
         char write2[READ_SIZE] = { 0 };
         int i =0;

         snprintf(childSysTimeBuffer, 256, "%s", "1");

         //wait so that father has already opened the db
         //sleep(3);

         //open db after father (in order to use the hashtable in shared memory)
         handle = persComDbOpen("/tmp/writethrough-concurrent.db", 0x3); //create test.db if not present and writethrough mode
         fail_unless(handle >= 0, "Child failed to create non existent lDB: retval: [%d]", ret);

         //read the new key written by the father from cache
         for(i=0; i< 200; i++)
         {
            snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
            snprintf(write2, 128, "DATA-%d",i );
            ret = persComDbReadKey(handle, key, (char*) childSysTimeBuffer, 256);
            //printf("Child Key Read from Cache: %s ------: %s  -------------- returnvalue: %d\n",key, childSysTimeBuffer, ret);
            //fail_unless(ret == strlen(write2), "Child: Wrong read size");
         }

         //write to test if cache can be accessed
         ret = persComDbWriteKey(handle, "writethrough-test-concurrent", "123456", 6);
         //printf("Father persComDbWriteKey: %s  -- returnvalue: %d \n", key, ret);
         fail_unless(ret == 6 , "CHILD: Wrong write size returned: %d", ret);

         //close database for child instance
         ret = persComDbClose(handle);
         if (ret != 0)
         {
            printf("persComDbClose() failed: [%d] \n", ret);
         }
         fail_unless(ret == 0, "Child failed to close database: retval: [%d]", ret);

         DLT_UNREGISTER_APP();
         _exit(EXIT_SUCCESS);
      }
      else if (pid > 0)
      {
         /*parent*/
         //printf("Started father process with PID: [%d] \n", pid);
         int handle = 0;
         int ret = 0;
         char write2[READ_SIZE] = { 0 };
         char key[128] = { 0 };
         char sysTimeBuffer[256] = { 0 };
         int i =0;

         handle = persComDbOpen("/tmp/writethrough-concurrent.db", 0x3); //create test.db if not present and writethrough mode
         fail_unless(handle >= 0, "Father failed to create non existent lDB: retval: [%d]", ret);

         //Write data to cache (cache gets created here)
         for(i=0; i< 200; i++)
         {
            snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
            snprintf(write2, 128, "DATA-%d",i);
            ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
            //printf("Father persComDbWriteKey: %s  -- returnvalue: %d \n", key, ret);
            fail_unless(ret == strlen(write2), "Father: Wrong write size");
         }
         //read data from cache
         for(i=0; i< 200; i++)
         {
            snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
            snprintf(write2, 128, "DATA-%d",i );
            ret = persComDbReadKey(handle, key, (char*) sysTimeBuffer, 256);
            //printf("Father Key Read from key: %s ------: %s  -------------- returnvalue: %d\n",key, sysTimeBuffer, ret);
            //fail_unless(ret == strlen(write2), "Father: Wrong read size");
         }

         printf("INFO: Waiting for child process to exit ... \n");

         //wait for child exiting
         int status;
         (void) waitpid(pid, &status, 0);


         //TEST if addresses have changed after child has written his data
         ret = persComDbWriteKey(handle, "write-test-FATHER", "123456", 6);
         //test


         //close database for father instance (closes the cache)
         ret = persComDbClose(handle);
         if (ret != 0)
         {
            printf("persComDbClose() failed: [%d] \n", ret);
         }
         fail_unless(ret == 0, "Father failed to close database: retval: [%d]", ret);

         _exit(EXIT_SUCCESS);
      }


}
END_TEST



static Suite* persistenceCommonLib_suite()
{
   Suite* s = suite_create("Persistence-common-object-test");

   TCase* tc_persOpenLocalDB = tcase_create("OpenlocalDB");
   tcase_add_test(tc_persOpenLocalDB, test_OpenLocalDB);

   TCase* tc_persOpenRCT = tcase_create("OpenRCT");
   tcase_add_test(tc_persOpenRCT, test_OpenRCT);

   TCase* tc_persSetDataLocalDB = tcase_create("SetDataLocalDB");
   tcase_add_test(tc_persSetDataLocalDB, test_SetDataLocalDB);

   TCase* tc_persGetDataLocalDB = tcase_create("GetDataLocalDB");
   tcase_add_test(tc_persGetDataLocalDB, test_GetDataLocalDB);

   TCase* tc_persSetDataRCT = tcase_create("SetDataRCT");
   tcase_add_test(tc_persSetDataRCT, test_SetDataRCT);

   TCase* tc_persGetDataRCT = tcase_create("GetDataRCT");
   tcase_add_test(tc_persGetDataRCT, test_GetDataRCT);

   TCase* tc_persGetDataSize = tcase_create("GetDataSize");
   tcase_add_test(tc_persGetDataSize, test_GetDataSize);

   TCase* tc_persDeleteDataLocalDB = tcase_create("DeleteDataLocalDB");
   tcase_add_test(tc_persDeleteDataLocalDB, test_DeleteDataLocalDB);

   TCase* tc_persDeleteDataRct = tcase_create("DeleteDataRct");
   tcase_add_test(tc_persDeleteDataRct, test_DeleteDataRct);

   TCase* tc_persGetKeyListSizeLocalDB = tcase_create("GetKeyListSizeLocalDb");
   tcase_add_test(tc_persGetKeyListSizeLocalDB, test_GetKeyListSizeLocalDB);

   TCase* tc_persGetKeyListLocalDB = tcase_create("GetKeyListLocalDb");
   tcase_add_test(tc_persGetKeyListLocalDB, test_GetKeyListLocalDB);

   TCase* tc_persGetResourceListSizeRct = tcase_create("GetResourceListSizeRct");
   tcase_add_test(tc_persGetResourceListSizeRct, test_GetResourceListSizeRct);

   TCase* tc_persGetResourceListRct = tcase_create("GetResourceListRct");
   tcase_add_test(tc_persGetResourceListRct, test_GetResourceListRct);

   TCase* tc_persCacheSize = tcase_create("CacheSize");
   tcase_add_test(tc_persCacheSize, test_CacheSize);
   tcase_set_timeout(tc_persCacheSize, 20);

   TCase* tc_persCachedConcurrentAccess = tcase_create("CachedConcurrentAccess");
   tcase_add_test(tc_persCachedConcurrentAccess, test_CachedConcurrentAccess);
   tcase_set_timeout(tc_persCachedConcurrentAccess, 20);

   TCase* tc_persCachedConcurrentAccess2 = tcase_create("CachedConcurrentAccess2");
   tcase_add_test(tc_persCachedConcurrentAccess2, test_CachedConcurrentAccess2);
   tcase_set_timeout(tc_persCachedConcurrentAccess2, 20);

   TCase* tc_BadParameters = tcase_create("BadParameters");
   tcase_add_test(tc_BadParameters, test_BadParameters);

   TCase* tc_RebuildHashtables = tcase_create("RebuildHashtables");
   tcase_add_test(tc_RebuildHashtables, test_RebuildHashtables);

   TCase* tc_RecoverDatablocks = tcase_create("RecoverDatablocks");
   tcase_add_test(tc_RecoverDatablocks, test_RecoverDatablocks);

   TCase* tc_LinkedDatabase = tcase_create("LinkedDatabase");
   tcase_add_test(tc_LinkedDatabase, test_LinkedDatabase);

   TCase* tc_ReadOnlyDatabase = tcase_create("ReadOnlyDatabase");
   tcase_add_test(tc_ReadOnlyDatabase, test_ReadOnlyDatabase);

   TCase* tc_MultipleWrites = tcase_create("MultipleWrites");
   tcase_add_test(tc_MultipleWrites, test_MultipleWrites);

   TCase* tc_WriteThrough = tcase_create("WriteThrough");
   tcase_add_test(tc_WriteThrough, test_WriteThrough);
   tcase_set_timeout(tc_WriteThrough, 20);



   suite_add_tcase(s, tc_persOpenLocalDB);
   tcase_add_checked_fixture(tc_persOpenLocalDB, data_setup, data_teardown);

   suite_add_tcase(s, tc_persOpenRCT);
   tcase_add_checked_fixture(tc_persOpenRCT, data_setup, data_teardown);

   suite_add_tcase(s, tc_persSetDataLocalDB);
   tcase_add_checked_fixture(tc_persSetDataLocalDB, data_setup, data_teardown);

   suite_add_tcase(s, tc_persGetDataLocalDB);
   tcase_add_checked_fixture(tc_persGetDataLocalDB, data_setup, data_teardown);

   suite_add_tcase(s, tc_persSetDataRCT);
   tcase_add_checked_fixture(tc_persSetDataRCT, data_setup, data_teardown);

   suite_add_tcase(s, tc_persGetDataRCT);
   tcase_add_checked_fixture(tc_persGetDataRCT, data_setup, data_teardown);

   suite_add_tcase(s, tc_persGetDataSize);
   tcase_add_checked_fixture(tc_persGetDataSize, data_setup, data_teardown);

   suite_add_tcase(s, tc_persDeleteDataLocalDB);
   tcase_add_checked_fixture(tc_persDeleteDataLocalDB, data_setup, data_teardown);

   suite_add_tcase(s, tc_persDeleteDataRct);
   tcase_add_checked_fixture(tc_persDeleteDataRct, data_setup, data_teardown);

   suite_add_tcase(s, tc_persGetKeyListSizeLocalDB);
   tcase_add_checked_fixture(tc_persGetKeyListSizeLocalDB, data_setup, data_teardown);

   suite_add_tcase(s, tc_persGetKeyListLocalDB);
   tcase_add_checked_fixture(tc_persGetKeyListLocalDB, data_setup, data_teardown);

   suite_add_tcase(s, tc_persGetResourceListSizeRct);
   tcase_add_checked_fixture(tc_persGetResourceListSizeRct, data_setup, data_teardown);

   suite_add_tcase(s, tc_persGetResourceListRct);
   tcase_add_checked_fixture(tc_persGetResourceListRct, data_setup, data_teardown);

   suite_add_tcase(s, tc_persCacheSize);     //do not run when using writethrough
   tcase_add_checked_fixture(tc_persCacheSize, data_setup, data_teardown);

   suite_add_tcase(s, tc_persCachedConcurrentAccess);
   tcase_add_checked_fixture(tc_persCachedConcurrentAccess, data_setup, data_teardown);
   //suite_add_tcase(s, tc_persCachedConcurrentAccess2);

   suite_add_tcase(s, tc_BadParameters);
   tcase_add_checked_fixture(tc_BadParameters, data_setup, data_teardown);

   suite_add_tcase(s, tc_RebuildHashtables);
   tcase_add_checked_fixture(tc_RebuildHashtables, data_setup, data_teardown);

   suite_add_tcase(s, tc_RecoverDatablocks); //add test for writethrough (writeback order is different when using cache)
   tcase_add_checked_fixture(tc_RecoverDatablocks, data_setup, data_teardown);

   suite_add_tcase(s, tc_LinkedDatabase);
   tcase_add_checked_fixture(tc_LinkedDatabase, data_setup, data_teardown);

   suite_add_tcase(s, tc_ReadOnlyDatabase);
   tcase_add_checked_fixture(tc_ReadOnlyDatabase, data_setup, data_teardown);

   suite_add_tcase(s, tc_MultipleWrites);
   tcase_add_checked_fixture(tc_MultipleWrites, data_setup, data_teardown);

   suite_add_tcase(s, tc_WriteThrough);
   tcase_add_checked_fixture(tc_WriteThrough, data_setup, data_teardown);

   return s;
}


int main(int argc, char* argv[])
{
   int nr_failed = 0, nr_run = 0, i = 0;
   TestResult** tResult;

#if 1
   Suite* s = persistenceCommonLib_suite();
   SRunner* sr = srunner_create(s);
   srunner_set_xml(sr, "/tmp/persistenceCommonObjectTest.xml");
   srunner_set_log(sr, "/tmp/persistenceCommonObjectTest.log");
   srunner_run_all(sr, /*CK_NORMAL*/CK_VERBOSE);

   nr_failed = srunner_ntests_failed(sr);
   nr_run = srunner_ntests_run(sr);

   tResult = srunner_results(sr);
   for (i = 0; i < nr_run; i++)
   {
      (void) tr_rtype(tResult[i]);  // get status of each test
   }

   srunner_free(sr);
#endif

   dlt_free();
   return (0 == nr_failed) ? EXIT_SUCCESS : EXIT_FAILURE;
}

