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
#include <../test/pers_com_test_base.h>
#include <../test/pers_com_check.h>
#include <sys/wait.h>

#define BUF_SIZE     64
#define NUM_OF_FILES 3
#define READ_SIZE    1024
#define MaxAppNameLen 256

/// application id
char gTheAppId[MaxAppNameLen] = { 0 };

// definition of weekday
char* dayOfWeek[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };


START_TEST(test_OpenLocalDB)
{
   X_TEST_REPORT_TEST_NAME("persistence_common_object_test");
   X_TEST_REPORT_COMP_NAME("libpersistence_common_object_library");
   X_TEST_REPORT_REFERENCE("NONE");
   X_TEST_REPORT_DESCRIPTION("Test of open localdb");
   X_TEST_REPORT_TYPE(GOOD);

   int k=0, handle =0;
   int databases = 20;
   char path[128];
   int handles[100] = { 0 };

   //Cleaning up testdata folder
   if (remove("/tmp/open-localdb.db") == 0)
      printf("File %s  deleted.\n", "/tmp/open-localdb.db");
   else
      fprintf(stderr, "Warning:  Could not delete file [%s].\n", "/tmp/open-localdb.db");

//   if(remove("/tmp/open-uncached.db") == 0)
//      printf("File %s  deleted.\n", "/tmp/open-uncached.db");
//   else
//      fprintf(stderr, "Warning:  Could not delete file [%s].\n", "/tmp/open-uncached.db");


   if(remove("/tmp/open-cached.db") == 0)
      printf("File %s  deleted.\n", "/tmp/open-cached.db");
   else
      fprintf(stderr, "Warning:  Could not delete file [%s].\n", "/tmp/open-cached.db");


   int ret = 0;
   int ret2 = 0;
   ret = persComDbOpen("/tmp/open-localdb.db", 0x0); //Do not create test.db / only open if present
   x_fail_unless(ret < 0, "Open open-localdb.db works, but should fail: retval: [%d]", ret);

   ret = persComDbOpen("/tmp/open-localdb.db", 0x1); //create test.db if not present
   x_fail_unless(ret >= 0, "Failed to create non existent lDB: retval: [%d]", ret);

   ret = persComDbClose(ret);
   if (ret != 0)
      printf("persComDbClose() failed: [%d] \n", ret);
   x_fail_unless(ret == 0, "Failed to close database: retval: [%d]", ret);

   //test to use more than 16 static handles
   for (k = 0; k < databases; k++)
   {
      snprintf(path, 128, "/tmp/handletest-%d.db", k);
      //Cleaning up testdata folder
      if (remove(path) == 0)
         printf("File %s  deleted.\n", path);
      else
         fprintf(stderr, "Warning:  Could not delete file [%s].\n", path);
   }

   for (k = 0; k < databases; k++)
   {
      snprintf(path, 128, "/tmp/handletest-%d.db", k);
      handle = persComDbOpen(path, 0x1); //create test.db if not present
      handles[k] = handle;
      x_fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);
   }

   //printf("closing! \n");
   for (k = 0; k < databases; k++)
   {
      ret = persComDbClose(handles[k]);
      if (ret != 0)
         printf("persComDbClose() failed: [%d] \n", ret);
      x_fail_unless(ret == 0, "Failed to close database with number %d: retval: [%d]", k, ret);
   }


   //Test two consecutive open calls
   ret =  persComDbOpen("/tmp/open-consecutive.db", 0x1); //create test.db if not present
   //printf("TEST handle first: %d \n", ret);
   x_fail_unless(ret >= 0, "Failed to create non existent lDB: retval: [%d]", ret);

   ret2 = persComDbOpen("/tmp/open-consecutive.db", 0x1); //create test.db if not present
   //printf("TEST handle second: %d \n", ret2);
   x_fail_unless(ret2 >= 0, "Failed at consecutive open: retval: [%d]", ret2);


   ret = persComDbClose(ret);
   if (ret != 0)
      printf("persComDbClose() 1 failed: [%d] \n", ret);


   ret = persComDbClose(ret2);
   if (ret != 0)
      printf("persComDbClose() 2 failed: [%d] \n", ret);



   //Test chached uncached flag
   ret =  persComDbOpen("/tmp/open-uncached.db", 0x2); //cached and DO NOT create test.db -> no close needed
   //printf("handle 1: %d \n", ret);
   x_fail_unless(ret < 0, "Failed at uncached open: retval: [%d]", ret); //fail if open works, but should fail

   ret2 = persComDbOpen("/tmp/open-cached.db", 0x3); //cached and create test.db if not present
   //printf("handle 2: %d \n", ret2);
   x_fail_unless(ret2 >= 0, "Failed at cached open: retval: [%d]", ret);


//   ret = persComDbClose(ret);
//   if (ret != 0)
//      printf("persComDbClose() uncached failed: [%d] \n", ret);


   ret = persComDbClose(ret2);
   if (ret != 0)
      printf("persComDbClose() cached failed: [%d] \n", ret);


}
END_TEST



START_TEST(test_OpenRCT)
{

   X_TEST_REPORT_TEST_NAME("persistence_common_object_test");
   X_TEST_REPORT_COMP_NAME("libpersistence_common_object_library");
   X_TEST_REPORT_REFERENCE("NONE");
   X_TEST_REPORT_DESCRIPTION("Test of open rct");
   X_TEST_REPORT_TYPE(GOOD);

   //Cleaning up testdata folder
   if (remove("/tmp/open-rct.db") == 0)
      printf("File %s  deleted.\n", "/tmp/open-rct.db");
   else
      fprintf(stderr, "Warning:  Could not delete file [%s].\n", "/tmp/open-rct.db");

   int ret = 0;
   ret = persComRctOpen("/tmp/open-rct.db", 0x0); //Do not create rct.db / only open if present
   x_fail_unless(ret < 0, "Open open-rct.db works, but should fail: retval: [%d]", ret);

   ret = persComRctOpen("/tmp/open-rct.db", 0x1); //create test.db if not present
   x_fail_unless(ret >= 0, "Failed to create non existent rct: retval: [%d]", ret);

   ret = persComRctClose(ret);
   if (ret != 0)
      printf("persComRctClose() failed: [%d] \n", ret);
   x_fail_unless(ret == 0, "Failed to close RCT database: retval: [%d]", ret);

}
END_TEST

/*
 * Write data to a key using the key interface in local DB.
 * First write data to different keys and after
 * read the data for verification.
 */
START_TEST(test_SetDataLocalDB)
{
   X_TEST_REPORT_TEST_NAME("persistence_common_object_test");
   X_TEST_REPORT_COMP_NAME("libpers_common");
   X_TEST_REPORT_REFERENCE("NONE");
   X_TEST_REPORT_DESCRIPTION("Test of set data local DB");
   X_TEST_REPORT_TYPE(GOOD);

   int ret = 0;
   int handle = 0;
         char write2[READ_SIZE] = { 0 };
   char read[READ_SIZE] = { 0 };
   char key[128] = { 0 };
   char sysTimeBuffer[256];
   struct tm *locTime;
   int i =0;

   //Cleaning up testdata folder
   if (remove("/tmp/write-localdb.db") == 0)
      printf("File %s  deleted.\n", "/tmp/write-localdb.db");
   else
      fprintf(stderr, "Warning:  Could not delete file [%s].\n", "/tmp/write-localdb.db");


#if 1
time_t t = time(0);
locTime = localtime(&t);

// write data
snprintf(sysTimeBuffer, 128, "\"%s %d.%d.%d - %d:%.2d:%.2d Uhr\"", dayOfWeek[locTime->tm_wday], locTime->tm_mday,
      locTime->tm_mon, (locTime->tm_year + 1900), locTime->tm_hour, locTime->tm_min, locTime->tm_sec);

handle = persComDbOpen("/tmp/write-localdb.db", 0x1); //create test.db if not present
x_fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);


snprintf(write2, 128, "%s %s", "/key_70", sysTimeBuffer);

//write to cache
for(i=0; i< 300; i++)
{
   snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
   ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
   x_fail_unless(ret == strlen(write2) , "Wrong write size while inserting in cache");
}

//read from cache
for(i=0; i< 300; i++)
{
   snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
   ret = persComDbReadKey(handle, key, (char*) read, strlen(write2));
   x_fail_unless(ret == strlen(write2), "Wrong read size while reading from cache");
}

//printf("read from cache ok \n");

//persist data in cache to file
ret = persComDbClose(handle);
if (ret != 0)
   printf("persComDbClose() failed: [%d] \n", ret);
x_fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);


handle = persComDbOpen("/tmp/write-localdb.db", 0x1); //create test.db if not present
x_fail_unless(handle >= 0, "Failed to reopen existing lDB: retval: [%d]", ret);


//printf("open ok \n");

//read from database file
for(i=0; i< 300; i++)
{
   snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
   ret = persComDbReadKey(handle, key, (char*) read, strlen(write2));
   //printf("read: %d returns: %s \n", i, read);
   x_fail_unless(ret == strlen(write2), "Wrong read size");
}

ret = persComDbClose(handle);
if (ret != 0)
   printf("persComDbClose() failed: [%d] \n", ret);
x_fail_unless(ret == 0, "Failed to close database file: retval: [%d]", ret);

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
   X_TEST_REPORT_TEST_NAME("persistence_common_object_test");
   X_TEST_REPORT_COMP_NAME("libpers_common");
   X_TEST_REPORT_REFERENCE("NONE");
   X_TEST_REPORT_DESCRIPTION("Test of get data local DB");
   X_TEST_REPORT_TYPE(GOOD);

   int ret = 0;
   int handle = 0;
   unsigned char readBuffer[READ_SIZE] = { 0 };
   char write2[READ_SIZE] = { 0 };
   char key[128] = { 0 };
   int i = 0;

   //Cleaning up testdata folder
   if (remove("/tmp/get-localdb.db") == 0)
      printf("File %s  deleted.\n", "/tmp/get-localdb.db");
   else
      fprintf(stderr, "Warning:  Could not delete file [%s].\n", "/tmp/get-localdb.db");


#if 1

handle = persComDbOpen("/tmp/get-localdb.db", 0x1); //create test.db if not present
x_fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);


//write keys to cache
for(i=0; i< 50; i++)
{
   snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
   memset(write2, 0, sizeof(write2));
   snprintf(write2, 128, "DATA-%d-%d",i,i*i );
   ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
   x_fail_unless(ret == strlen(write2), "Wrong write size");
}

//Read keys from cache
for(i=0; i< 50; i++)
{
   snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
   memset(write2, 0, sizeof(write2));
   snprintf(write2, sizeof(write2), "DATA-%d-%d",i,i*i );
   memset(readBuffer, 0, sizeof(readBuffer));
   ret = persComDbReadKey(handle, key, (char*) readBuffer, sizeof(readBuffer));
   x_fail_unless(ret == strlen(write2), "Wrong read size");
   x_fail_unless(memcmp(readBuffer, write2, sizeof(readBuffer)) == 0, "Reading Data from Cache failed: Buffer not correctly read");
}

//persist changed data for this lifecycle
ret = persComDbClose(handle);
if (ret != 0)
   printf("persComDbClose() failed: Cached Data was not written back: [%d] \n", ret);
x_fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);

//open database again
handle = persComDbOpen("/tmp/get-localdb.db", 0x1); //create test.db if not present
x_fail_unless(handle >= 0, "Failed to reopen existing lDB: retval: [%d]", ret);


//Read keys from database file
for(i=0; i< 50; i++)
{
   snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
   memset(write2, 0, sizeof(write2));
   snprintf(write2, sizeof(write2), "DATA-%d-%d",i,i*i );
   memset(readBuffer, 0, sizeof(readBuffer));
   ret = persComDbReadKey(handle, key, (char*) readBuffer, sizeof(readBuffer));
   x_fail_unless(ret == strlen(write2), "Wrong read size");
   x_fail_unless(memcmp(readBuffer, write2, sizeof(readBuffer)) == 0, "Reading Data from File failed: Buffer not correctly read");
}

ret = persComDbClose(handle);
if (ret != 0)
   printf("persComDbClose() failed: [%d] \n", ret);
x_fail_unless(ret == 0, "Failed to close database file: retval: [%d]", ret);

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
   X_TEST_REPORT_TEST_NAME("persistence_common_object_test");
   X_TEST_REPORT_COMP_NAME("libpers_common");
   X_TEST_REPORT_REFERENCE("NONE");
   X_TEST_REPORT_DESCRIPTION("Test of get key list size local DB");
   X_TEST_REPORT_TYPE(GOOD);

   int ret = 0;
   int handle = 0;
      char write1[READ_SIZE] = { 0 };
   char write2[READ_SIZE] = { 0 };
   char sysTimeBuffer[256];
      int listSize = 0;
   char key[8] = { 0 };
   struct tm *locTime;

   //Cleaning up testdata folder
   if (remove("/tmp/localdb-size-keylist.db") == 0)
      printf("File %s  deleted.\n", "/tmp/localdb-size-keylist.db");
   else
      fprintf(stderr, "Warning:  Could not delete file [%s].\n", "/tmp/localdb-size-keylist.db");

#if 1
   time_t t = time(0);
   locTime = localtime(&t);

   // write data
   snprintf(sysTimeBuffer, 128, "\"%s %d.%d.%d - %d:%.2d:%.2d Uhr\"", dayOfWeek[locTime->tm_wday], locTime->tm_mday,
         locTime->tm_mon, (locTime->tm_year + 1900), locTime->tm_hour, locTime->tm_min, locTime->tm_sec);

   handle = persComDbOpen("/tmp/localdb-size-keylist.db", 0x1); //create db if not present
   x_fail_unless(handle >= 0, "Failed to create non existent lDB for keylist test: retval: [%d]", ret);


   snprintf(key, 8, "%s", "key_123");
   ret = persComDbWriteKey(handle, key, (char*) sysTimeBuffer, strlen(sysTimeBuffer));
   x_fail_unless(ret == strlen(sysTimeBuffer), "Wrong write size");


   snprintf(key, 8, "%s", "key_456");
   snprintf(write1, 128, "%s %s", "k_456", sysTimeBuffer);
   ret = persComDbWriteKey(handle, key, (char*) write1, strlen(write1));
   x_fail_unless(ret == strlen(write1), "Wrong write size");

   snprintf(key, 8, "%s", "key_789");
   snprintf(write2, 128, "%s %s", "k_789", sysTimeBuffer);
   ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
   x_fail_unless(ret == strlen(write2), "Wrong write size");


   listSize = persComDbGetSizeKeysList(handle);
   //printf("LISTSIZE: %d \n", listSize);
   x_fail_unless(listSize == 3 * strlen(key) + 3, "Wrong list size read from cache");



   //persist changes in order to read only keys that are in database file
   ret = persComDbClose(handle);
   if (ret != 0)
      printf("persComDbClose() failed: [%d] \n", ret);
   x_fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);

   handle = persComDbOpen("/tmp/localdb-size-keylist.db", 0x1); //create db if not present
   x_fail_unless(handle >= 0, "Failed to create non existent lDB for keylist test: retval: [%d]", ret);


   //write duplicated key to cache
   snprintf(key, 8, "%s", "key_789");
   snprintf(write2, 128, "%s %s", "k_789", sysTimeBuffer);
   ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
   x_fail_unless(ret == strlen(write2), "Wrong write size");

   //get needed listsize (looks for keys in cache and in database file) duplicate keys occuring in cache AND in database file are removed
   // listsize here must be 24
   listSize = persComDbGetSizeKeysList(handle);
   //printf("LISTSIZE: %d \n", listSize);
   x_fail_unless(listSize == 3 * strlen(key) + 3, "Wrong list size read from file");

   //write new key to cache
   snprintf(key, 8, "%s", "key_000");
   snprintf(write2, 128, "%s %s", "k_000", sysTimeBuffer);
   ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));

   //read list size again (must be 32)
   listSize = persComDbGetSizeKeysList(handle);
   //printf("LISTSIZE: %d \n", listSize);
   x_fail_unless(listSize == 4 * strlen(key) + 4, "Wrong list size read from combined cache / file");

   ret = persComDbClose(handle);
   if (ret != 0)
      printf("persComDbClose() failed: [%d] \n", ret);
   x_fail_unless(ret == 0, "Failed to close database file: retval: [%d]", ret);

#endif

}
END_TEST




/* Get the resource list from an existing local database with already inserted key names
 * Insert some keys then get the list of all keys separated with '\0'
 * Then check if the List returned contains all of the keys inserted before
 */
START_TEST(test_GetKeyListLocalDB)
{
   X_TEST_REPORT_TEST_NAME("persistence_common_object_test");
   X_TEST_REPORT_COMP_NAME("libpers_common");
   X_TEST_REPORT_REFERENCE("NONE");
   X_TEST_REPORT_DESCRIPTION("Test of get key list local DB");
   X_TEST_REPORT_TYPE(GOOD);

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
   struct tm *locTime;
   char* keyList = NULL;
   int listSize = 0;

   //Cleaning up testdata folder
   if (remove("/tmp/localdb-keylist.db") == 0)
      printf("File %s  deleted.\n", "/tmp/localdb-keylist.db");
   else
      fprintf(stderr, "Warning:  Could not delete file [%s].\n", "/tmp/localdb-keylist.db");

#if 1
   time_t t = time(0);
   locTime = localtime(&t);

   // write data
   snprintf(sysTimeBuffer, 128, "\"%s %d.%d.%d - %d:%.2d:%.2d Uhr\"", dayOfWeek[locTime->tm_wday], locTime->tm_mday,
         locTime->tm_mon, (locTime->tm_year + 1900), locTime->tm_hour, locTime->tm_min, locTime->tm_sec);

   handle = persComDbOpen("/tmp/localdb-keylist.db", 0x1); //create db if not present
   x_fail_unless(handle >= 0, "Failed to create non existent lDB for keylist test: retval: [%d]", ret);

   /**
    * Logical DB
    */
   snprintf(key1, 8, "%s", "key_123");
   ret = persComDbWriteKey(handle, key1, (char*) sysTimeBuffer, strlen(sysTimeBuffer));
   x_fail_unless(ret == strlen(sysTimeBuffer), "Wrong write size");

   /**
    * Logical DB
    */
   snprintf(key2, 8, "%s", "key_456");
   snprintf(write1, 128, "%s %s", "k_456", sysTimeBuffer);
   ret = persComDbWriteKey(handle, key2, (char*) write1, strlen(write1));
   x_fail_unless(ret == strlen(write1), "Wrong write size");

   /**
    * Logical DB
    */
   snprintf(key3, 8, "%s", "key_789");
   snprintf(write2, 128, "%s %s", "k_789", sysTimeBuffer);
   ret = persComDbWriteKey(handle, key3, (char*) write2, strlen(write2));
   x_fail_unless(ret == strlen(write2), "Wrong write size");



   //close database in order to persist the cached keys.
   ret = persComDbClose(handle);
   if (ret != 0)
      printf("persComDbClose() failed: [%d] \n", ret);
   x_fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);

   handle = persComDbOpen("/tmp/localdb-keylist.db", 0x1); //create db if not present
   x_fail_unless(handle >= 0, "Failed to create non existent lDB for keylist test: retval: [%d]", ret);


   //write to cache
   snprintf(key4, 8, "%s", "key_456");
   snprintf(write1, 128, "%s %s", "k_456", sysTimeBuffer);
   ret = persComDbWriteKey(handle, key4, (char*) write1, strlen(write1));

   //read keys from file and from cache
   listSize = persComDbGetSizeKeysList(handle);
   x_fail_unless(listSize == 3 * strlen(key1) + 3, "Wrong list size");

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
                     cmp_result = 0;
               }
               else
                  cmp_result = 0;
            }
            else
               cmp_result = 0;
         }
         else
            cmp_result = 0;
      }
      else
         cmp_result = 0;
   }
   else
      cmp_result = 0;

//   printf("original keylist: [%s] \n", origKeylist);
//   printf("keylist: [%s] \n", keyList);
   free(keyList);
   x_fail_unless(cmp_result == 0, "List not correctly read");

   ret = persComDbClose(handle);
   if (ret != 0)
      printf("persComDbClose() failed: [%d] \n", ret);
   x_fail_unless(ret == 0, "Failed to close database file: retval: [%d]", ret);

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
   X_TEST_REPORT_TEST_NAME("persistence_common_object_test");
   X_TEST_REPORT_COMP_NAME("libpers_common");
   X_TEST_REPORT_REFERENCE("NONE");
   X_TEST_REPORT_DESCRIPTION("Test of get resource size RCT");
   X_TEST_REPORT_TYPE(GOOD);

   int ret = 0;
   int handle = 0;
   char sysTimeBuffer[256];
   char key1[8] = { 0 };
   char key2[8] = { 0 };
   char key3[8] = { 0 };
   char key4[8] = { 0 };
   int listSize = 0;
   struct tm *locTime;

   PersistenceConfigurationKey_s psConfig;
   psConfig.policy = PersistencePolicy_wt;
   psConfig.storage = PersistenceStorage_local;
   psConfig.type = PersistenceResourceType_key;
   psConfig.permission = PersistencePermission_ReadWrite;

   //Cleaning up testdata folder
   if (remove("/tmp/rct-size-resource-list.db") == 0)
      printf("File %s  deleted.\n", "/tmp/rct-size-resource-list.db");
   else
      fprintf(stderr, "Warning:  Could not delete file [%s].\n", "/tmp/rct-size-resource-list.db");

#if 1
   time_t t = time(0);
   locTime = localtime(&t);

   // write data
   snprintf(sysTimeBuffer, 64, "\"%s %d.%d.%d - %d:%.2d:%.2d Uhr\"", dayOfWeek[locTime->tm_wday], locTime->tm_mday,
         locTime->tm_mon, (locTime->tm_year + 1900), locTime->tm_hour, locTime->tm_min, locTime->tm_sec);

   handle = persComRctOpen("/tmp/rct-size-resource-list.db", 0x1); //create rct.db if not present
   x_fail_unless(handle >= 0, "Failed to create non existent rct: retval: [%d]", ret);

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
   x_fail_unless(ret == sizeof(psConfig), "Wrong write size");

   snprintf(key2, 8, "%s", "key_45");
   ret = persComRctWrite(handle, key2, &psConfig);
   x_fail_unless(ret == sizeof(psConfig), "Wrong write size");

   snprintf(key3, 8, "%s", "key_7");
   ret = persComRctWrite(handle, key3, &psConfig);
   x_fail_unless(ret == sizeof(psConfig), "Wrong write size");

   //get listsize from cache
   listSize = persComRctGetSizeResourcesList(handle);
   x_fail_unless(listSize == 3 * strlen(key1), "Read Wrong list size from file and cache");

   //persist cached data
   ret = persComRctClose(handle);
   if (ret != 0)
   printf("persComRctClose() failed: [%d] \n", ret);
   x_fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);

   handle = persComRctOpen("/tmp/rct-size-resource-list.db", 0x1); //create rct.db if not present
   x_fail_unless(handle >= 0, "Failed to create non existent rct: retval: [%d]", ret);

   //get listsize from file
   listSize = persComRctGetSizeResourcesList(handle);
   x_fail_unless(listSize == 3 * strlen(key1), "Read Wrong list size from file and cache");

   ret = persComRctClose(handle);
   if (ret != 0)
   printf("persComRctClose() failed: [%d] \n", ret);
   x_fail_unless(ret == 0, "Failed to close database: retval: [%d]", ret);

   handle = persComRctOpen("/tmp/rct-size-resource-list.db", 0x1); //create rct.db if not present
   x_fail_unless(handle >= 0, "Failed to create non existent rct: retval: [%d]", ret);

   //insert duplicate key
   snprintf(key3, 8, "%s", "key_7");
   ret = persComRctWrite(handle, key3, &psConfig);
   x_fail_unless(ret == sizeof(psConfig), "Wrong write size");

   //insert new key
   snprintf(key4, 8, "%s", "key_new");
   ret = persComRctWrite(handle, key4, &psConfig);
   x_fail_unless(ret == sizeof(psConfig), "Wrong write size");

   //get listsize if keys are in cache and in file
   listSize = persComRctGetSizeResourcesList(handle);
   x_fail_unless(listSize == strlen(key1)+ strlen(key2) + strlen(key3) + strlen(key4) + 4, "Read Wrong list size from file and cache");

   ret = persComRctClose(handle);
   if (ret != 0)
      printf("persComRctClose() failed: [%d] \n", ret);
   x_fail_unless(ret == 0, "Failed to close database file: retval: [%d]", ret);

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
   X_TEST_REPORT_TEST_NAME("persistence_common_object_test");
   X_TEST_REPORT_COMP_NAME("libpers_common");
   X_TEST_REPORT_REFERENCE("NONE");
   X_TEST_REPORT_DESCRIPTION("Test of get resource list RCT");
   X_TEST_REPORT_TYPE(GOOD);

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
   struct tm *locTime;

   PersistenceConfigurationKey_s psConfig;
   psConfig.policy = PersistencePolicy_wt;
   psConfig.storage = PersistenceStorage_local;
   psConfig.type = PersistenceResourceType_key;
   psConfig.permission = PersistencePermission_ReadWrite;

   //Cleaning up testdata folder
   if (remove("/tmp/rct-resource-list.db") == 0)
      printf("File %s  deleted.\n", "/tmp/rct-resource-list.db");
   else
      fprintf(stderr, "Warning:  Could not delete file [%s].\n", "/tmp/rct-resource-list.db");

#if 1
   time_t t = time(0);
   locTime = localtime(&t);

   // write data
   snprintf(sysTimeBuffer, 64, "\"%s %d.%d.%d - %d:%.2d:%.2d Uhr\"", dayOfWeek[locTime->tm_wday], locTime->tm_mday,
         locTime->tm_mon, (locTime->tm_year + 1900), locTime->tm_hour, locTime->tm_min, locTime->tm_sec);

   handle = persComRctOpen("/tmp/rct-resource-list.db", 0x1); //create rct.db if not present
   x_fail_unless(handle >= 0, "Failed to create non existent rct: retval: [%d]", ret);

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
   x_fail_unless(ret == sizeof(psConfig), "Wrong write size");

   snprintf(key2, 8, "%s", "key_456");
   ret = persComRctWrite(handle, key2, &psConfig);
   x_fail_unless(ret == sizeof(psConfig), "Wrong write size");

   snprintf(key3, 8, "%s", "key_789");
   ret = persComRctWrite(handle, key3, &psConfig);
   x_fail_unless(ret == sizeof(psConfig), "Wrong write size");

   snprintf(origKeylist, 24, "%s%c%s%c%s", key1, '\0', key3, '\0', key2);

   //persist keys to file
   ret = persComRctClose(handle);
   if (ret != 0)
      printf("persComRctClose() failed: [%d] \n", ret);
   x_fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);

   handle = persComRctOpen("/tmp/rct-resource-list.db", 0x1); //create rct.db if not present
   x_fail_unless(handle >= 0, "Failed to create non existent rct: retval: [%d]", ret);

   //write duplicate key to cache
   snprintf(key4, 8, "%s", "key_456");
   ret = persComRctWrite(handle, key4, &psConfig);
   x_fail_unless(ret == sizeof(psConfig), "Wrong write size");

   //read keys from file and from cache
   listSize = persComRctGetSizeResourcesList(handle);
   x_fail_unless(listSize == 3 * strlen(key1) + 3, "Wrong list size");

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
                     cmp_result = 0;
               }
               else
                  cmp_result = 0;
            }
            else
               cmp_result = 0;
         }
         else
            cmp_result = 0;
      }
      else
         cmp_result = 0;
   }
   else
      cmp_result = 0;

   //printf("original resourceList: [%s] \n", origKeylist);
   //printf("resourceList: [%s]\n", resourceList);
   free(resourceList);
   x_fail_unless(cmp_result == 0, "List not correctly read");

   ret = persComRctClose(handle);
   if (ret != 0)
      printf("persComRctClose() failed: [%d] \n", ret);
   x_fail_unless(ret == 0, "Failed to close database: retval: [%d]", ret);

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
   X_TEST_REPORT_TEST_NAME("persistence_common_object_test");
   X_TEST_REPORT_COMP_NAME("libpers_common");
   X_TEST_REPORT_REFERENCE("NONE");
   X_TEST_REPORT_DESCRIPTION("Test of set data in RCT");
   X_TEST_REPORT_TYPE(GOOD);

   int ret = 0;
   int handle = 0;
   char sysTimeBuffer[256];
   struct tm *locTime;

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
if (remove("/tmp/write-rct.db") == 0)
   printf("File %s  deleted.\n", "/tmp/write-rct.db");
else
   fprintf(stderr, "Warning:  Could not delete file [%s].\n", "/tmp/write-rct.db");

handle = persComRctOpen("/tmp/write-rct.db", 0x1); //create db if not present
x_fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);

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
x_fail_unless(ret == sizeof(psConfig), "wrong write size \n");
#if 1

memset(psConfig_out.custom_name, 0, sizeof(psConfig_out.custom_name));
memset(psConfig_out.customID, 0, sizeof(psConfig_out.customID));
memset(psConfig_out.reponsible, 0, sizeof(psConfig_out.reponsible));

//read from cache
ret = persComRctRead(handle, "69", &psConfig_out);
x_fail_unless(ret == sizeof(psConfig), "Wrong read size from cache");


//persist data in cache to database file
ret = persComRctClose(handle);
if (ret != 0)
   printf("persComRctClose() failed: [%d] \n", ret);
x_fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);

//reopen database
handle = persComRctOpen("/tmp/write-rct.db", 0x1); //create db if not present
x_fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);

/*
 * now read the data written in the previous steps to the keys in RCT
 * and verify data has been written correctly.
 */
memset(psConfig_out.custom_name, 0, sizeof(psConfig_out.custom_name));
memset(psConfig_out.customID, 0, sizeof(psConfig_out.customID));
memset(psConfig_out.reponsible, 0, sizeof(psConfig_out.reponsible));

//read from file
ret = persComRctRead(handle, "69", &psConfig_out);
x_fail_unless(ret == sizeof(psConfig), "Wrong read size from file");

//printf("Custom ID        : %s\n", psConfig_out.customID );
//printf("Custom Name      : %s\n", psConfig_out.custom_name );
//printf("reponsible       : %s\n", psConfig_out.reponsible );
//printf("max_size         : %d\n", psConfig_out.max_size );
//printf("permission       : %d\n", psConfig_out.permission );
//printf("type             : %d\n", psConfig_out.type );
//printf("storage          : %d\n", psConfig_out.storage );
//printf("policy           : %d\n", psConfig_out.policy );

x_fail_unless(strncmp(psConfig.customID, psConfig_out.customID, strlen(psConfig_out.customID)) == 0,
      "Buffer not correctly read");
x_fail_unless(strncmp(psConfig.custom_name, psConfig_out.custom_name, strlen(psConfig_out.custom_name)) == 0,
      "Buffer not correctly read");
x_fail_unless(strncmp(psConfig.reponsible, psConfig_out.reponsible, strlen(psConfig_out.reponsible)) == 0,
      "Buffer not correctly read");
x_fail_unless(psConfig.max_size == psConfig_out.max_size, "Buffer not correctly read");
x_fail_unless(psConfig.permission == psConfig_out.permission, "Buffer not correctly read");
x_fail_unless(psConfig.policy == psConfig_out.policy, "Buffer not correctly read");
x_fail_unless(psConfig.storage == psConfig_out.storage, "Buffer not correctly read");
x_fail_unless(psConfig.type == psConfig_out.type, "Buffer not correctly read");

//persist to database file
ret = persComRctClose(handle);
if (ret != 0)
   printf("persComRctClose() failed: [%d] \n", ret);
x_fail_unless(ret == 0, "Failed to close database: retval: [%d]", ret);

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
   X_TEST_REPORT_TEST_NAME("persistence_common_object_test");
   X_TEST_REPORT_COMP_NAME("libpers_common");
   X_TEST_REPORT_REFERENCE("NONE");
   X_TEST_REPORT_DESCRIPTION("Test of get data in RCT");
   X_TEST_REPORT_TYPE(GOOD);

   int ret = 0;
   int handle = 0;
   char sysTimeBuffer[256];
   struct tm *locTime;

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
if (remove("/tmp/get-rct.db") == 0)
   printf("File %s  deleted.\n", "/tmp/get-rct.db");
else
   fprintf(stderr, "Warning:  Could not delete file [%s].\n", "/tmp/get-rct.db");


handle = persComRctOpen("/tmp/get-rct.db", 0x1); //create db if not present
x_fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);

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
x_fail_unless(ret == sizeof(psConfig), "write size wrong");
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
x_fail_unless(ret == sizeof(psConfig_out), "Wrong read size from cache");

//printf("Custom ID        : %s\n", psConfig_out.customID );
//printf("Custom Name      : %s\n", psConfig_out.custom_name );
//printf("reponsible       : %s\n", psConfig_out.reponsible );
//printf("max_size         : %d\n", psConfig_out.max_size );
//printf("permission       : %d\n", psConfig_out.permission );
//printf("type             : %d\n", psConfig_out.type );
//printf("storage          : %d\n", psConfig_out.storage );
//printf("policy           : %d\n", psConfig_out.policy );



x_fail_unless(strncmp(psConfig.customID, psConfig_out.customID, strlen(psConfig_out.customID)) == 0,
      "Buffer not correctly read");
x_fail_unless(strncmp(psConfig.custom_name, psConfig_out.custom_name, strlen(psConfig_out.custom_name)) == 0,
      "Buffer not correctly read");
x_fail_unless(strncmp(psConfig.reponsible, psConfig_out.reponsible, strlen(psConfig_out.reponsible)) == 0,
      "Buffer not correctly read");
x_fail_unless(psConfig.max_size == psConfig_out.max_size, "Buffer not correctly read");
x_fail_unless(psConfig.permission == psConfig_out.permission, "Buffer not correctly read");
x_fail_unless(psConfig.policy == psConfig_out.policy, "Buffer not correctly read");
x_fail_unless(psConfig.storage == psConfig_out.storage, "Buffer not correctly read");
x_fail_unless(psConfig.type == psConfig_out.type, "Buffer not correctly read");


//persist to database file
ret = persComRctClose(handle);
if (ret != 0)
   printf("persComRctClose() failed: [%d] \n", ret);
x_fail_unless(ret == 0, "Failed to close database: retval: [%d]", ret);

#endif
#endif


handle = persComRctOpen("/tmp/get-rct.db", 0x1); //create db if not present
x_fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);


memset(psConfig_out.custom_name, 0, sizeof(psConfig_out.custom_name));
memset(psConfig_out.customID, 0, sizeof(psConfig_out.customID));
memset(psConfig_out.reponsible, 0, sizeof(psConfig_out.reponsible));


//read from file
ret = persComRctRead(handle, "69", &psConfig_out);
x_fail_unless(ret == sizeof(psConfig_out), "Wrong read size from file");

//printf("Custom ID        : %s\n", psConfig_out.customID );
//printf("Custom Name      : %s\n", psConfig_out.custom_name );
//printf("reponsible       : %s\n", psConfig_out.reponsible );
//printf("max_size         : %d\n", psConfig_out.max_size );
//printf("permission       : %d\n", psConfig_out.permission );
//printf("type             : %d\n", psConfig_out.type );
//printf("storage          : %d\n", psConfig_out.storage );
//printf("policy           : %d\n", psConfig_out.policy );

x_fail_unless(strncmp(psConfig.customID, psConfig_out.customID, strlen(psConfig_out.customID)) == 0,
      "Buffer not correctly read from file");
x_fail_unless(strncmp(psConfig.custom_name, psConfig_out.custom_name, strlen(psConfig_out.custom_name)) == 0,
      "Buffer not correctly read from file");
x_fail_unless(strncmp(psConfig.reponsible, psConfig_out.reponsible, strlen(psConfig_out.reponsible)) == 0,
      "Buffer not correctly read from file");
x_fail_unless(psConfig.max_size == psConfig_out.max_size, "Buffer not correctly read from file");
x_fail_unless(psConfig.permission == psConfig_out.permission, "Buffer not correctly read from file");
x_fail_unless(psConfig.policy == psConfig_out.policy, "Buffer not correctly read from file");
x_fail_unless(psConfig.storage == psConfig_out.storage, "Buffer not correctly read from file");
x_fail_unless(psConfig.type == psConfig_out.type, "Buffer not correctly read from file");


ret = persComRctClose(handle);
if (ret != 0)
   printf("persComRctClose() failed: [%d] \n", ret);
x_fail_unless(ret == 0, "Failed to close database: retval: [%d]", ret);


}
END_TEST



/*
 * Test to get the datasize for a key
 * write a key to cache, then the size of the data to that key from cache
 * Close and reopens the database to read the size to a key from file again.
 */
START_TEST(test_GetDataSize)
{
   X_TEST_REPORT_TEST_NAME("persistence_common_object_test");
   X_TEST_REPORT_COMP_NAME("libpers_common");
   X_TEST_REPORT_REFERENCE("NONE");
   X_TEST_REPORT_DESCRIPTION("Test of get data size");
   X_TEST_REPORT_TYPE(GOOD);

   char sysTimeBuffer[256];
   int size = 0, ret = 0;
   int handle = 0;
   struct tm *locTime;

   time_t t = time(0);
   locTime = localtime(&t);

   // write data
   snprintf(sysTimeBuffer, 128, "\"%s %d.%d.%d - %d:%.2d:%.2d Uhr\"", dayOfWeek[locTime->tm_wday], locTime->tm_mday,
         locTime->tm_mon, (locTime->tm_year + 1900), locTime->tm_hour, locTime->tm_min, locTime->tm_sec);

   //Cleaning up testdata folder
   if (remove("/tmp/size-localdb.db") == 0)
      printf("File %s  deleted.\n", "/tmp/size-localdb.db");
   else
      fprintf(stderr, "Warning:  Could not delete file [%s].\n", "/tmp/size-localdb.db");


   handle = persComDbOpen("/tmp/size-localdb.db", 0x1); //create localdb.db if not present
   x_fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);

   //write key to cache
   ret = persComDbWriteKey(handle, "status/open_document", (char*) sysTimeBuffer, strlen(sysTimeBuffer));
   x_fail_unless(ret == strlen(sysTimeBuffer), "Wrong write size");

#if 1
   //get keysize from cache
   size = persComDbGetKeySize(handle, "status/open_document");
   //printf("=>=>=>=> soll: %d | ist: %d\n", strlen(sysTimeBuffer), size);
   x_fail_unless(size == strlen(sysTimeBuffer), "Invalid size read from cache");

   //persist cached data
   ret = persComDbClose(handle);
   if (ret != 0)
      printf("persComDbClose() failed: [%d] \n", ret);
   x_fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);

   handle = persComDbOpen("/tmp/size-localdb.db", 0x1); //create localdb.db if not present
   x_fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);

   //get keysize from file
   size = persComDbGetKeySize(handle, "status/open_document");
   //printf("=>=>=>=> soll: %d | ist: %d\n", strlen(sysTimeBuffer), size);
   x_fail_unless(size == strlen(sysTimeBuffer), "Invalid size read from cache");

   ret = persComDbClose(handle);
   if (ret != 0)
      printf("persComDbClose() failed: [%d] \n", ret);
   x_fail_unless(ret == 0, "Failed to close database: retval: [%d]", ret);

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
   X_TEST_REPORT_TEST_NAME("persistence_common_object_test");
   X_TEST_REPORT_COMP_NAME("libpers_common");
   X_TEST_REPORT_REFERENCE("NONE");
   X_TEST_REPORT_DESCRIPTION("Test of delete data local DB");
   X_TEST_REPORT_TYPE(GOOD);

   int rval = 0;
   int handle = 0;
   unsigned char buffer[READ_SIZE] = { 0 };
   char write1[READ_SIZE] = { 0 };
   char sysTimeBuffer[256];
   struct tm *locTime;

#if 1

time_t t = time(0);
locTime = localtime(&t);

// write data
snprintf(sysTimeBuffer, 128, "\"%s %d.%d.%d - %d:%.2d:%.2d Uhr\"", dayOfWeek[locTime->tm_wday], locTime->tm_mday,
      locTime->tm_mon, (locTime->tm_year + 1900), locTime->tm_hour, locTime->tm_min, locTime->tm_sec);


////Cleaning up testdata folder
if (remove("/tmp/delete-localdb.db") == 0)
   printf("File %s  deleted.\n", "/tmp/delete-localdb.db");
else
   fprintf(stderr, "Warning:  Could not delete file [%s].\n", "/tmp/delete-localdb.db");


handle = persComDbOpen("/tmp/delete-localdb.db", 0x1); //create test.db if not present
x_fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", handle);

snprintf(write1, 128, "%s %s", "/70", sysTimeBuffer);


//write data to cache
rval = persComDbWriteKey(handle, "key1", (char*) write1, strlen(write1));
x_fail_unless(rval == strlen(write1), "Wrong write size");
rval = persComDbWriteKey(handle, "key2", (char*) write1, strlen(write1));
x_fail_unless(rval == strlen(write1), "Wrong write size");
rval = persComDbWriteKey(handle, "key3", (char*) write1, strlen(write1));
x_fail_unless(rval == strlen(write1), "Wrong write size");
rval = persComDbWriteKey(handle, "key4", (char*) write1, strlen(write1));
x_fail_unless(rval == strlen(write1), "Wrong write size");
rval = persComDbWriteKey(handle, "key5", (char*) write1, strlen(write1));
x_fail_unless(rval == strlen(write1), "Wrong write size");
rval = persComDbWriteKey(handle, "key6", (char*) write1, strlen(write1));
x_fail_unless(rval == strlen(write1), "Wrong write size");


// read data from cache
rval = persComDbReadKey(handle, "key1", (char*) buffer, READ_SIZE);
x_fail_unless(rval == strlen(write1), "Wrong read size");
rval = persComDbReadKey(handle, "key2", (char*) buffer, READ_SIZE);
x_fail_unless(rval == strlen(write1), "Wrong read size");
rval = persComDbReadKey(handle, "key3", (char*) buffer, READ_SIZE);
x_fail_unless(rval == strlen(write1), "Wrong read size");
rval = persComDbReadKey(handle, "key4", (char*) buffer, READ_SIZE);
x_fail_unless(rval == strlen(write1), "Wrong read size");
rval = persComDbReadKey(handle, "key5", (char*) buffer, READ_SIZE);
x_fail_unless(rval == strlen(write1), "Wrong read size");
rval = persComDbReadKey(handle, "key6", (char*) buffer, READ_SIZE);
x_fail_unless(rval == strlen(write1), "Wrong read size");



// mark data in cache as deleted
rval = persComDbDeleteKey(handle, "key1");
x_fail_unless(rval >= 0, "Failed to delete key");

rval = persComDbDeleteKey(handle, "key2");
x_fail_unless(rval >= 0, "Failed to delete key");

rval = persComDbDeleteKey(handle, "key3");
x_fail_unless(rval >= 0, "Failed to delete key");

rval = persComDbDeleteKey(handle, "key4");
x_fail_unless(rval >= 0, "Failed to delete key");

rval = persComDbDeleteKey(handle, "key5");
x_fail_unless(rval >= 0, "Failed to delete key");

rval = persComDbDeleteKey(handle, "key6");
x_fail_unless(rval >= 0, "Failed to delete key");


// after deleting the keys in cache, reading from key must fail now!
rval = persComDbReadKey(handle, "key1", (char*) buffer, READ_SIZE);
x_fail_unless(rval < 0, "Read form key [key1] works, but should fail");

rval = persComDbReadKey(handle, "key2", (char*) buffer, READ_SIZE);
x_fail_unless(rval < 0, "Read form key [key2] works, but should fail");

rval = persComDbReadKey(handle, "key3", (char*) buffer, READ_SIZE);
x_fail_unless(rval < 0, "Read form key [key3] works, but should fail");

rval = persComDbReadKey(handle, "key4", (char*) buffer, READ_SIZE);
x_fail_unless(rval < 0, "Read form key [key4] works, but should fail");

rval = persComDbReadKey(handle, "key5", (char*) buffer, READ_SIZE);
x_fail_unless(rval < 0, "Read form key [key5] works, but should fail");

rval = persComDbReadKey(handle, "key6", (char*) buffer, READ_SIZE);
x_fail_unless(rval < 0, "Read form key [key6] works, but should fail");





//write data again which gets persisted to file afterwards
//write data to cache
rval = persComDbWriteKey(handle, "key1", (char*) write1, strlen(write1));
x_fail_unless(rval == strlen(write1), "Wrong write size");
rval = persComDbWriteKey(handle, "key2", (char*) write1, strlen(write1));
x_fail_unless(rval == strlen(write1), "Wrong write size");
rval = persComDbWriteKey(handle, "key3", (char*) write1, strlen(write1));
x_fail_unless(rval == strlen(write1), "Wrong write size");
rval = persComDbWriteKey(handle, "key4", (char*) write1, strlen(write1));
x_fail_unless(rval == strlen(write1), "Wrong write size");
rval = persComDbWriteKey(handle, "key5", (char*) write1, strlen(write1));
x_fail_unless(rval == strlen(write1), "Wrong write size");
rval = persComDbWriteKey(handle, "key6", (char*) write1, strlen(write1));
x_fail_unless(rval == strlen(write1), "Wrong write size");


// read data from cache
rval = persComDbReadKey(handle, "key1", (char*) buffer, READ_SIZE);
x_fail_unless(rval == strlen(write1), "Wrong read size");
rval = persComDbReadKey(handle, "key2", (char*) buffer, READ_SIZE);
x_fail_unless(rval == strlen(write1), "Wrong read size");
rval = persComDbReadKey(handle, "key3", (char*) buffer, READ_SIZE);
x_fail_unless(rval == strlen(write1), "Wrong read size");
rval = persComDbReadKey(handle, "key4", (char*) buffer, READ_SIZE);
x_fail_unless(rval == strlen(write1), "Wrong read size");
rval = persComDbReadKey(handle, "key5", (char*) buffer, READ_SIZE);
x_fail_unless(rval == strlen(write1), "Wrong read size");
rval = persComDbReadKey(handle, "key6", (char*) buffer, READ_SIZE);
x_fail_unless(rval == strlen(write1), "Wrong read size");



//persist data to file
rval = persComDbClose(handle);
if (rval != 0)
   printf("persComDbClose() failed: [%d] \n", rval);
x_fail_unless(rval == 0, "Failed to close cached database: retval: [%d]", rval);

//reopen database
handle = persComDbOpen("/tmp/delete-localdb.db", 0x1); //create test.db if not present
x_fail_unless(handle >= 0, "Failed to open lDB: retval: [%d]", handle);


//delete keys
rval = persComDbDeleteKey(handle, "key1");
x_fail_unless(rval >= 0, "Failed to delete key");

rval = persComDbDeleteKey(handle, "key2");
x_fail_unless(rval >= 0, "Failed to delete key");

rval = persComDbDeleteKey(handle, "key3");
x_fail_unless(rval >= 0, "Failed to delete key");

rval = persComDbDeleteKey(handle, "key4");
x_fail_unless(rval >= 0, "Failed to delete key");

rval = persComDbDeleteKey(handle, "key5");
x_fail_unless(rval >= 0, "Failed to delete key");

rval = persComDbDeleteKey(handle, "key6");
x_fail_unless(rval >= 0, "Failed to delete key");



// after deleting the keys, reading from key must fail now!
rval = persComDbReadKey(handle, "key1", (char*) buffer, READ_SIZE);
x_fail_unless(rval < 0, "Read form key [key1] in file works, but should fail");


rval = persComDbReadKey(handle, "key2", (char*) buffer, READ_SIZE);
x_fail_unless(rval < 0, "Read form key [key2] in file works, but should fail");

rval = persComDbReadKey(handle, "key3", (char*) buffer, READ_SIZE);
x_fail_unless(rval < 0, "Read form key [key3] in file works, but should fail");

rval = persComDbReadKey(handle, "key4", (char*) buffer, READ_SIZE);
x_fail_unless(rval < 0, "Read form key [key4] in file works, but should fail");

rval = persComDbReadKey(handle, "key5", (char*) buffer, READ_SIZE);
x_fail_unless(rval < 0, "Read form key [key5] in file works, but should fail");

rval = persComDbReadKey(handle, "key6", (char*) buffer, READ_SIZE);
x_fail_unless(rval < 0, "Read form key [key6] in file works, but should fail");


rval = persComDbClose(handle);
if (rval != 0)
   printf("persComDbClose() failed: [%d] \n", rval);
x_fail_unless(rval == 0, "Failed to close database: retval: [%d]", rval);

//reopen the database to write a key again into cache that must reuse the already deleted slot in the file when closing the database
handle = persComDbOpen("/tmp/delete-localdb.db", 0x1); //create test.db if not present
x_fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", handle);

// write a key again to test if slot in file is reused (CONTENT OF offset == 4  in kissdb.c)
rval = persComDbWriteKey(handle, "key1", (char*) write1, strlen(write1));
x_fail_unless(rval == strlen(write1), "Wrong write size");


// write a key again to test if slot in file is reused (CONTENT OF offset == 4  in kissdb.c)
rval = persComDbWriteKey(handle, "key1", (char*) write1, strlen(write1));
x_fail_unless(rval == strlen(write1), "Wrong write size");


rval = persComDbClose(handle);
if (rval != 0)
   printf("persComDbClose() failed: [%d] \n", rval);
x_fail_unless(rval == 0, "Failed to close database: retval: [%d]", rval);



//reopen the database to read key1
handle = persComDbOpen("/tmp/delete-localdb.db", 0x1); //create test.db if not present
x_fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", handle);

rval = persComDbReadKey(handle, "key1", (char*) buffer, READ_SIZE);
x_fail_unless(rval == strlen(write1), "Wrong read size for key1");








rval = persComDbClose(handle);
if (rval != 0)
   printf("persComDbClose() failed: [%d] \n", rval);
x_fail_unless(rval == 0, "Failed to close last database: retval: [%d]", rval);






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
   X_TEST_REPORT_TEST_NAME("persistence_common_object_test");
   X_TEST_REPORT_COMP_NAME("libpers_common");
   X_TEST_REPORT_REFERENCE("NONE");
   X_TEST_REPORT_DESCRIPTION("Test of delete data RCT");
   X_TEST_REPORT_TYPE(GOOD);

   int rval = 0;
   int handle = 0;
   char sysTimeBuffer[256];
   struct tm *locTime;
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
if (remove("/tmp/delete-rct.db") == 0)
   printf("File %s  deleted.\n", "/tmp/delete-rct.db");
else
   fprintf(stderr, "Warning:  Could not delete file [%s].\n", "/tmp/delete-rct.db");

handle = persComRctOpen("/tmp/delete-rct.db", 0x1); //create db if not present
x_fail_unless(handle >= 0, "Failed to create non existent RCT: retval: [%d]", handle);

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
x_fail_unless(rval == sizeof(psConfig), "Wrong write size in cache");

memset(psConfig_out.custom_name, 0, sizeof(psConfig_out.custom_name));
memset(psConfig_out.customID, 0, sizeof(psConfig_out.customID));
memset(psConfig_out.reponsible, 0, sizeof(psConfig_out.reponsible));

//read from cache
rval = persComRctRead(handle, "key_to_delete", &psConfig_out);
x_fail_unless(rval == sizeof(psConfig), "Wrong read size from cache");


//                        printf("Custom ID        : %s\n", psConfig_out.customID );
//                        printf("Custom Name      : %s\n", psConfig_out.custom_name );
//                        printf("reponsible       : %s\n", psConfig_out.reponsible );
//                        printf("max_size         : %d\n", psConfig_out.max_size );
//                        printf("permission       : %d\n", psConfig_out.permission );
//                        printf("type             : %d\n", psConfig_out.type );
//                        printf("storage          : %d\n", psConfig_out.storage );
//                        printf("policy           : %d\n", psConfig_out.policy );


x_fail_unless(strncmp(psConfig.customID, psConfig_out.customID, strlen(psConfig_out.customID)) == 0,
      "Buffer not correctly read");
x_fail_unless(strncmp(psConfig.custom_name, psConfig_out.custom_name, strlen(psConfig_out.custom_name)) == 0,
      "Buffer not correctly read");
x_fail_unless(strncmp(psConfig.reponsible, psConfig_out.reponsible, strlen(psConfig_out.reponsible)) == 0,
      "Buffer not correctly read");
x_fail_unless(psConfig.max_size == psConfig_out.max_size, "Buffer not correctly read");
x_fail_unless(psConfig.permission == psConfig_out.permission, "Buffer not correctly read");
x_fail_unless(psConfig.policy == psConfig_out.policy, "Buffer not correctly read");
x_fail_unless(psConfig.storage == psConfig_out.storage, "Buffer not correctly read");
x_fail_unless(psConfig.type == psConfig_out.type, "Buffer not correctly read");

// mark key in cache as deleted
rval = persComRctDelete(handle, "key_to_delete");
x_fail_unless(rval >= 0, "Failed to delete key");


// after deleting the key, reading from key must fail now!
rval = persComRctRead(handle, "key_to_delete", &psConfig_out);
x_fail_unless(rval < 0, "Read form key [key_to_delete] works, but should fail");


//write data again to cache
//write key to cache which already exists in database file
rval = persComRctWrite(handle, "key_to_delete", &psConfig);
x_fail_unless(rval == sizeof(psConfig), "Wrong write size in cache");

//read from cache
rval = persComRctRead(handle, "key_to_delete", &psConfig_out);
x_fail_unless(rval == sizeof(psConfig), "Wrong read size from cache");


rval = persComRctClose(handle);
if (rval != 0)
   printf("persComRctClose() failed: [%d] \n", rval);
x_fail_unless(rval == 0, "Failed to close database: retval: [%d]", rval);

handle = persComRctOpen("/tmp/delete-rct.db", 0x1); //create db if not present
x_fail_unless(handle >= 0, "Failed to create non existent RCT: retval: [%d]", handle);


// mark key in cache as deleted
rval = persComRctDelete(handle, "key_to_delete");
x_fail_unless(rval >= 0, "Failed to delete key");

// after deleting the key, reading from key must fail now!
rval = persComRctRead(handle, "key_to_delete", &psConfig_out);
x_fail_unless(rval < 0, "Read form key [key_to_delete] works, but should fail");


rval = persComRctClose(handle);
if (rval != 0)
   printf("persComRctClose() failed: [%d] \n", rval);
x_fail_unless(rval == 0, "Failed to close database: retval: [%d]", rval);


#endif
}
END_TEST




START_TEST(test_CachedConcurrentAccess)
{
   int pid;

   //Cleaning up testdata folder
   if (remove("/tmp/cached-concurrent.db") == 0)
      printf("File %s  deleted.\n", "/tmp/cached-concurrent.db");
   else
      fprintf(stderr, "Warning:  Could not delete file [%s].\n", "/tmp/cached-concurrent.db");

   pid = fork();
   if (pid == 0)
   { /*child*/
      printf("Started child process with PID: [%d] \n", pid);
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
      handle = persComDbOpen("/tmp/cached-concurrent.db", 0x0); //create test.db if not present
      x_fail_unless(handle >= 0, "Child failed to create non existent lDB: retval: [%d]", ret);

      //read the new key written by the father from cache
      for(i=0; i< 200; i++)
      {
         snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
         snprintf(write2, 128, "DATA-%d",i );
         ret = persComDbReadKey(handle, key, (char*) childSysTimeBuffer, 256);
         //printf("Child Key Read from Cache: %s ------: %s  -------------- returnvalue: %d\n",key, childSysTimeBuffer, ret);
         x_fail_unless(ret == strlen(write2), "Child: Wrong read size");
      }

      //close database for child instance
      ret = persComDbClose(handle);
      if (ret != 0)
         printf("persComDbClose() failed: [%d] \n", ret);
      x_fail_unless(ret == 0, "Child failed to close database: retval: [%d]", ret);


      _exit(EXIT_SUCCESS);
   }
   else if (pid > 0)
   { /*parent*/
      printf("Started father process with PID: [%d] \n", pid);
      int handle = 0;
      int ret = 0;
      char write2[READ_SIZE] = { 0 };
      char key[128] = { 0 };
      char sysTimeBuffer[256] = { 0 };
      int i =0;

      handle = persComDbOpen("/tmp/cached-concurrent.db", 0x1); //create test.db if not present
      x_fail_unless(handle >= 0, "Father failed to create non existent lDB: retval: [%d]", ret);

      //Write data to cache
      for(i=0; i< 200; i++)
      {
         snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
         snprintf(write2, 128, "DATA-%d",i);
         ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
         //printf("Father persComDbWriteKey: %s  -- returnvalue: %d \n", key, ret);
         x_fail_unless(ret == strlen(write2), "Father: Wrong write size");
      }

      //read data from cache
      for(i=0; i< 200; i++)
      {
         snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
         snprintf(write2, 128, "DATA-%d",i );
         ret = persComDbReadKey(handle, key, (char*) sysTimeBuffer, 256);
         //printf("Father Key Read from key: %s ------: %s  -------------- returnvalue: %d\n",key, sysTimeBuffer, ret);
         x_fail_unless(ret == strlen(write2), "Father: Wrong read size");
      }

      printf("INFO: Waiting for child process to exit ... \n");

      //wait for child exiting
      int status;
      (void) waitpid(pid, &status, 0);

      //close database for father instance (closes the cache)
      ret = persComDbClose(handle);
      if (ret != 0)
         printf("persComDbClose() failed: [%d] \n", ret);
      x_fail_unless(ret == 0, "Father failed to close database: retval: [%d]", ret);

      _exit(EXIT_SUCCESS);
   }
}
END_TEST





/*
 * tests the remapping of the shared hashtable. An application A has opened the shared hashtable that holds for example 100 entires.
 * If application B has also access to the database and iserts further keys, a second hashtable gets added.
 * Now application A must remap the shared memory used to hold the hashtable to its process space with the new size.lll
 */
//START_TEST(test_RemapHashtableHeader)
//{
//   int pid;
//
//   //Cleaning up testdata folder
//   if (remove("/tmp/cached-remap1.db") == 0)
//      printf("File %s  deleted.\n", "/tmp/cached-remap1.db");
//   else
//      fprintf(stderr, "Warning:  Could not delete file [%s].\n", "/tmp/cached-remap1.db");
//
//   pid = fork();
//   if (pid == 0)
//   { /*child*/
//      printf("Started child process with PID: [%d] \n", pid);
//      int handle = 0;
//      int ret = 0;
//      char childSysTimeBuffer[256] = { 0 };
//      char key[128] = { 0 };
//      char write2[READ_SIZE] = { 0 };
//      int i =0;
//      snprintf(childSysTimeBuffer, 256, "%s", "1");
//
//      //wait so that father has already opened the db
//      sleep(2);
//
//      //open db after father (in order to use the hashtable in shared memory)
//      handle = persComDbOpen("/tmp/cached-remap1.db", 0x0); //create test.db if not present
//      x_fail_unless(handle >= 0, "Child failed to create non existent lDB: retval: [%d]", ret);
//
//      //read the new key written by the father from cache
//      for(i=0; i< 300; i++)
//      {
//         snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
//         snprintf(write2, 128, "DATA-%d",i );
//         ret = persComDbReadKey(handle, key, (char*) childSysTimeBuffer, 256);
//         //printf("Child Key Read from Cache: %s ------: %s  -------------- returnvalue: %d\n",key, childSysTimeBuffer, ret);
//         x_fail_unless(ret == strlen(write2), "Child: Wrong read size");
//      }
//
//      //try to read a key which is not in cache in order to force a read from file and provoke a remap of the hashtable
//      snprintf(key, 128, "Not-in-Cache-Key_in_loop_%d_%d",i,i*i);
//      snprintf(write2, 128, "DATA-%d",i );
//      ret = persComDbReadKey(handle, key, (char*) childSysTimeBuffer, 256);
//
//
//
//
//      //write more keys
//      for(i=0; i< 500; i++)
//      {
//         memset(key, 0, 128);
//         snprintf(key, 128, "CHILD_Key_in_loop_%d_%d",i,i*i);
//         snprintf(write2, 128, "Child-DATA-%d",i);
//         ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
//         //printf("Father persComDbWriteKey: %s  -- returnvalue: %d \n", key, ret);
//         x_fail_unless(ret == strlen(write2), "Father: Wrong write size");
//      }
//
//      //sleep(1);
//
//      //close database for child instance
//      ret = persComDbClose(handle);
//      if (ret != 0)
//         printf("persComDbClose() failed: [%d] \n", ret);
//      x_fail_unless(ret == 0, "Child failed to close database: retval: [%d]", ret);
//
//
//      _exit(EXIT_SUCCESS);
//   }
//   else if (pid > 0)
//   { /*parent*/
//      printf("Started father process with PID: [%d] \n", pid);
//      int handle = 0;
//      int ret = 0;
//      char write2[READ_SIZE] = { 0 };
//      char key[128] = { 0 };
//      char sysTimeBuffer[256] = { 0 };
//      int i =0;
//
//      handle = persComDbOpen("/tmp/cached-remap1.db", 0x1); //create test.db if not present
//      x_fail_unless(handle >= 0, "Father failed to create non existent lDB: retval: [%d]", ret);
//
//      //Write data to cache
//      for(i=0; i< 300; i++)
//      {
//         snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
//         snprintf(write2, 128, "DATA-%d",i);
//         ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
//         //printf("Father persComDbWriteKey: %s  -- returnvalue: %d \n", key, ret);
//         x_fail_unless(ret == strlen(write2), "Father: Wrong write size");
//      }
//
//      //read data from cache
//      for(i=0; i< 300; i++)
//      {
//         snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
//         snprintf(write2, 128, "DATA-%d",i );
//         ret = persComDbReadKey(handle, key, (char*) sysTimeBuffer, 256);
//         //printf("Father Key Read from key: %s ------: %s  -------------- returnvalue: %d\n",key, sysTimeBuffer, ret);
//         x_fail_unless(ret == strlen(write2), "Father: Wrong read size");
//      }
//
//      //wait so that child has written new hashtables
//      sleep(3);
//
//      //Now read the keys written by the child
//      for(i=0; i< 500; i++)
//      {
//         memset(key, 0, 128);
//         snprintf(key, 128, "CHILD_Key_in_loop_%d_%d",i,i*i);
//         snprintf(write2, 128, "Child-DATA-%d",i);
//         ret = persComDbReadKey(handle, key, (char*) sysTimeBuffer, 256);
//         //printf("Father Key Read from Cache 2: %s ------: %s  -------------- returnvalue: %d\n",key, sysTimeBuffer, ret);
//         //x_fail_unless(ret == strlen(write2), "Father 2: Wrong read size");
//      }
//
//      printf("INFO: Waiting for child process to exit ... \n");
//      int status;
//      (void) waitpid(pid, &status, 0);
//
//      //close database for father instance (closes the cache)
//      ret = persComDbClose(handle);
//      if (ret != 0)
//         printf("persComDbClose() failed: [%d] \n", ret);
//      x_fail_unless(ret == 0, "Father failed to close database: retval: [%d]", ret);
//
//      _exit(EXIT_SUCCESS);
//   }
//}
//END_TEST
//
//







/*
 * check if shared memory is correctly resized if new hastable is appended
 */
//START_TEST(test_SharedHeaderResize)
//{
//
//   int pid;
//
//   //Cleaning up testdata folder
//   if (remove("/tmp/resize-localdb.db") == 0)
//      printf("File %s  deleted.\n", "/tmp/resize-localdb.db");
//   else
//      fprintf(stderr, "Warning:  Could not delete file [%s].\n", "/tmp/resize-localdb.db");
//
//   pid = fork();
//
//   if (pid == 0)
//   { /*child*/
//      unsigned char buffer[READ_SIZE] = { 0 };
//      int handle = 0;
//      int ret = 0;
//      char childSysTimeBuffer[256];
//      char childWrite[256];
//      snprintf(childSysTimeBuffer, 128, "%s", "CONCURRENT_TEST_DATA"), snprintf(childWrite, 128, "%s",
//            "CHILD_DATA_WRITTEN"),
//
//      //wait so that father has already opened the db
//      sleep(1);
//
//      //open db after father
//      handle = persComDbOpen("/tmp/resize-localdb.db", 0x0); //create test.db if not present
//      printf("child persComDbOpen returnvalue: %d \n", handle);
//      x_fail_unless(handle >= 0, "Child failed to create non existent lDB: retval: [%d]", ret);
//
//      //wait until father has written a new key
//      sleep(5);
//
//
//      //read the new key written by the father
//      ret = persComDbReadKey(handle, "69", buffer, READ_SIZE);
//      printf("child persComDbReadKey returnvalue: %d \n", ret);
//      x_fail_unless(strncmp((char* )buffer, childSysTimeBuffer, strlen(childSysTimeBuffer)) == 0,
//            "Buffer not correctly read");
//      x_fail_unless(ret == strlen(childSysTimeBuffer), "Wrong read size");
//
//
//      ret = persComDbReadKey(handle, "70", buffer, READ_SIZE);
//      printf("child persComDbReadKey returnvalue: %d \n", ret);
//      x_fail_unless(strncmp((char* )buffer, childSysTimeBuffer, strlen(childSysTimeBuffer)) == 0,
//            "Buffer not correctly read");
//      x_fail_unless(ret == strlen(childSysTimeBuffer), "Wrong read size");
//
//
//
//      ret = persComDbReadKey(handle, "71", buffer, READ_SIZE);
//      printf("child persComDbReadKey returnvalue: %d \n", ret);
//      x_fail_unless(strncmp((char* )buffer, childSysTimeBuffer, strlen(childSysTimeBuffer)) == 0,
//            "Buffer not correctly read");
//      x_fail_unless(ret == strlen(childSysTimeBuffer), "Wrong read size");
//
//
//
//      ret = persComDbClose(handle);
//      if (ret != 0)
//         printf("persComDbClose() failed: [%d] \n", ret);
//
//
//      printf("############################## Child: TEST OK ##########################\n");
//
//      _exit(EXIT_SUCCESS);
//   }
//   else if (pid > 0)
//   { /*parent*/
//      int handle = 0;
//      int ret = 0;
//      unsigned char buffer[READ_SIZE] = { 0 };
//      char write1[READ_SIZE] = { 0 };
//      char write2[READ_SIZE] = { 0 };
//      char sysTimeBuffer[256];
//      char fatherRead[256];
//
//      snprintf(sysTimeBuffer, 128, "%s", "CONCURRENT_TEST_DATA"),
//
//      handle = persComDbOpen("/tmp/resize-localdb.db", 0x1); //create test.db if not present
//      printf("father persComDbOpen returnvalue: %d \n", handle);
//      x_fail_unless(handle >= 0, "Father failed to create non existent lDB: retval: [%d]", ret);
//
//      /**
//      * delay writing so that the child opens the db which does not already
//      * contain the new keys written by the father (testing if shared header works)
//      **/
//      sleep(3);
//
//      // write data
//      ret = persComDbWriteKey(handle, "69", (char*) sysTimeBuffer, strlen(sysTimeBuffer));
//      printf("father persComDbWriteKey returnvalue: %d \n", ret);
//      x_fail_unless(ret == strlen(sysTimeBuffer), "Wrong write size");
//
//      ret = persComDbWriteKey(handle, "70", (char*) sysTimeBuffer, strlen(sysTimeBuffer));
//      printf("father persComDbWriteKey returnvalue: %d \n", ret);
//      x_fail_unless(ret == strlen(sysTimeBuffer), "Wrong write size");
//
//      ret = persComDbWriteKey(handle, "71", (char*) sysTimeBuffer, strlen(sysTimeBuffer));
//      printf("father persComDbWriteKey returnvalue: %d \n", ret);
//      x_fail_unless(ret == strlen(sysTimeBuffer), "Wrong write size");
//
//
//      int status;
//      (void) waitpid(pid, &status, 0); //wait for child exiting
//
//      ret = persComDbClose(handle);
//      if (ret != 0)
//         printf("persComDbClose() failed: [%d] \n", ret);
//
//
//      printf("############################## Father: TEST OK ##########################\n");
//      _exit(EXIT_SUCCESS);
//   }
//}
//END_TEST



START_TEST(test_CacheSize)
   {

      unsigned char buffer2[PERS_DB_MAX_SIZE_KEY_DATA] = { 1 };
      int handle = 0;
      int i, k, ret = 0;
      char dataBufer[PERS_DB_MAX_SIZE_KEY_DATA] = { 1 };
      char key[128] = { 0 };
      char path[128] = { 0 };
      int handles[100] = { 0 };
      int writings = 300;
      int databases = 1;

      for (k = 0; k < databases; k++)
      {
         snprintf(path, 128, "/tmp/cacheSize-%d.db", k);
         //Cleaning up testdata folder
         if (remove(path) == 0)
            printf("File %s  deleted.\n", path);
         else
            fprintf(stderr, "Warning:  Could not delete file [%s].\n", path);
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
         x_fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);

         //write data to cache
         for (i = 0; i < writings; i++)
         {
            snprintf(key, 128, "Key_in_loop_%d_%d", i, i * i);
            ret = persComDbWriteKey(handle, key, (char*) dataBufer, strlen(dataBufer));
            //printf("Writing Key: %s | Retval: %d \n", key, ret);
            x_fail_unless(ret == strlen(dataBufer) , "Wrong write size while inserting in cache");
         }
         //read data from cache
         for (i = 0; i < writings; i++)
         {
            snprintf(key, 128, "Key_in_loop_%d_%d", i, i * i);
            ret = persComDbReadKey(handle, key, (char*) buffer2, PERS_DB_MAX_SIZE_KEY_DATA);
            //printf("read from key: %s | Retval: %d \n", key, ret);
            x_fail_unless(ret == strlen(dataBufer), "Wrong read size while reading from cache");
         }
      }

      //sleep to look for memory consumption of cache
      //sleep(20);
      //Close k databases in order to persist the data
      for (k = 0; k < databases; k++)
      {
         ret = persComDbClose(handles[k]);
         if (ret != 0)
            printf("persComDbClose() failed: [%d] \n", ret);
         x_fail_unless(ret == 0, "Failed to close database with number %d: retval: [%d]", k, ret);
      }
}
END_TEST





///*
// *
// * Write data to a key using the key interface in local DB.
// * First write data to different keys and after
// * read the data for verification.
// */
//START_TEST(test_SupportedChars)
//{
//   X_TEST_REPORT_TEST_NAME("persistence_common_object_test");
//   X_TEST_REPORT_COMP_NAME("libpers_common");
//   X_TEST_REPORT_REFERENCE("NONE");
//   X_TEST_REPORT_DESCRIPTION("Test of supported characters in local DB");
//   X_TEST_REPORT_TYPE(GOOD);
//
//   int ret = 0;
//   int handle = 0;
//   char write2[READ_SIZE] = { 0 };
//   char read[READ_SIZE] = { 0 };
//   char key[128] = { 0 };
//   char value[128] = { 0 };
//   char sysTimeBuffer[256];
//   struct tm *locTime;
//   unsigned int i =0;
//
//
//   //Cleaning up testdata folder
//   if (remove("/tmp/chars-localdb2.db") == 0)
//      printf("File %s  deleted.\n", "/tmp/chars-localdb2.db");
//   else
//      fprintf(stderr, "Warning:  Could not delete file [%s].\n", "/tmp/chars-localdb2.db");
//
//
//#if 1
//time_t t = time(0);
//locTime = localtime(&t);
//
//unsigned char c;
//
//unsigned char values[256];
//for (i=0; i<256; i++){
//  unsigned char temp = i;
//  values[i] = temp;
//  //printf("VALUE: %d = %c \n", values[i], values[i]);
//}
//
//
//handle = persComDbOpen("/tmp/chars-localdb2.db", 0x1); //create test.db if not present
//x_fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);
//
//
////write to cache
//for(i=0; i< 256; i++)
//{
//   snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
//   ret = persComDbWriteKey(handle, key, &values[i], 1);
//   x_fail_unless(ret == 1, "Wrong write size while inserting in cache");
//}
//
////read from cache
//for(i=0; i< 256; i++)
//{
//   snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
//   ret = persComDbReadKey(handle, key, (char*) read, 1);
//   x_fail_unless(ret == 1 , "Wrong read size while reading from cache: %d", ret);
//   x_fail_unless( memcmp(read, &values[i], 1) == 0, "Reading Data from Cache failed: Buffer not correctly read");
//}
//
//
//
//ret = persComDbWriteKey(handle, "DEGREE", "°", 1);
//ret = persComDbReadKey(handle, "DEGREE", (char*) read, 1);
//printf("degree: %s\n",read);
//
//
////persist data in cache to file
//ret = persComDbClose(handle);
//if (ret != 0)
//   printf("persComDbClose() failed: [%d] \n", ret);
//x_fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);
//
//
//handle = persComDbOpen("/tmp/chars-localdb2.db", 0x1); //create test.db if not present
//x_fail_unless(handle >= 0, "Failed to reopen existing lDB: retval: [%d]", ret);
//
//
////read from database file
//for(i=0; i< 256; i++)
//{
//   snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
//   ret = persComDbReadKey(handle, key, (char*) write2, 1);
//   x_fail_unless(ret == 1, "Wrong read size: %d",ret);
//   //printf("READ from file: %d = %c \n", write2[0], write2[0]);
//   x_fail_unless( memcmp(write2, &values[i], 1) == 0, "Reading Data from File failed: Buffer not correctly read");
//}
//
//
//
//ret = persComDbWriteKey(handle, "DEGREE", "qwertz°abcd", 11);
//ret = persComDbReadKey(handle, "DEGREE", (char*) write2, 1);
//printf("degree: %s\n",write2);
//
//ret = persComDbClose(handle);
//if (ret != 0)
//   printf("persComDbClose() failed: [%d] \n", ret);
//x_fail_unless(ret == 0, "Failed to close database file: retval: [%d]", ret);
//
//#endif
//}
//END_TEST




START_TEST(test_BadParameters)
{
   X_TEST_REPORT_TEST_NAME("persistence_common_object_test");
   X_TEST_REPORT_COMP_NAME("libpersistence_common_object_library");
   X_TEST_REPORT_REFERENCE("NONE");
   X_TEST_REPORT_DESCRIPTION("Test of bad parameters behavior");
   X_TEST_REPORT_TYPE(GOOD);

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
   PersistenceConfigurationKey_s psConfig;

   ret = persComDbOpen("6l3HrKvT9TmXdTbqF4mc3N38llpbKkn2qMhdiLOlwXnY7H09ZewvQG80uyLr8sg0by0oD9UXNOm9OHvXl8zf7vKosu0M90Aau6WFqELDJl6OYr3xPYPH59o7AvixDMQXlNrPUUTdluU24TEFEiTVhcRcWJDoxlL6LHg1u9p3pNURI9GKmAsXDHovXXrvwP3qSjDYB0gMhvEvfpDI5oy8vb3Frz81zZmKuHsx9GQi0xWTB5n6grRH9TvcJW7F1yu7", 0x0); //Pathname exceeds 255 chars
   x_fail_unless(ret < 0, "Open local db with too long path length works, but should fail: retval: [%d]", ret);

   ret = persComDbOpen(path, 0x1); //create test.db if not present
   x_fail_unless(ret < 0, "Open local db with bad pathname works, but should fail: retval: [%d]", ret);

   ret = persComDbClose(-1);
   x_fail_unless(ret < 0, "Closing the local database with negative handle works, but should fail: retval: [%d]", ret);

   ret = persComRctOpen("6l3HrKvT9TmXdTbqF4mc3N38llpbKkn2qMhdiLOlwXnY7H09ZewvQG80uyLr8sg0by0oD9UXNOm9OHvXl8zf7vKosu0M90Aau6WFqELDJl6OYr3xPYPH59o7AvixDMQXlNrPUUTdluU24TEFEiTVhcRcWJDoxlL6LHg1u9p3pNURI9GKmAsXDHovXXrvwP3qSjDYB0gMhvEvfpDI5oy8vb3Frz81zZmKuHsx9GQi0xWTB5n6grRH9TvcJW7F1yu7", 0x0); //Pathname exceeds 255 chars
   x_fail_unless(ret < 0, "Open RCT db with too long path length works, but should fail: retval: [%d]", ret);

   ret = persComRctOpen(path, 0x1); //create test.db if not present
   x_fail_unless(ret < 0, "Open RCT db with bad pathname works, but should fail: retval: [%d]", ret);

   ret = persComRctClose(-1);
   x_fail_unless(ret < 0, "Closing the RCT database with negative handle works, but should fail: retval: [%d]", ret);



   ret = persComDbWriteKey(1, "CteKh3FTonalS4AlOaEruzUbgAP9fryYJLCykq5tTPQkPrHEcV9p6akxa6TuF9gqnJu5iCEyxMUu17QhTP7sYgFwFKU1qqNMcCmps8WcpWDR2oCnjqdaBtATL2A36q6QV", "data", 4); //key too long
   x_fail_unless(ret < 0 , "Writing with wrong keylength works, but should fail");

   ret = persComDbWriteKey(-1, "key", "data", 4); //negative handle
   x_fail_unless(ret < 0 , "Writing with negative handle works, but should fail");

   ret = persComDbWriteKey(1, key, "data", 4); //key is NULL
   x_fail_unless(ret < 0 , "Writing key that is NULL works, but should fail");

   ret = persComDbWriteKey(1, "key", data, 4); //data is NULL
   x_fail_unless(ret < 0 , "Writing data that is NULL works, but should fail");

   ret = persComDbWriteKey(1, "key", "data", -1254); //datasize is negative
   x_fail_unless(ret < 0 , "Writing with negative datasize works, but should fail");

   ret = persComDbWriteKey(1, "key", "data", 0); //datasize is zero
   x_fail_unless(ret < 0 , "Writing with zero datasize works, but should fail");

   ret = persComDbWriteKey(1, "key", "data", 16385); //datasize too big
   x_fail_unless(ret < 0 , "Writing with too big datasize works, but should fail");



   ret = persComRctWrite(1, "CteKh3FTonalS4AlOaEruzUbgAP9fryYJLCykq5tTPQkPrHEcV9p6akxa6TuF9gqnJu5iCEyxMUu17QhTP7sYgFwFKU1qqNMcCmps8WcpWDR2oCnjqdaBtATL2A36q6QV", &psConfig); //key too long
   x_fail_unless(ret < 0 , "Writing RCT with wrong keylength works, but should fail");

   ret = persComRctWrite(-1, "key", &psConfig); //negative handle
   x_fail_unless(ret < 0 , "Writing RCT with negative handle works, but should fail");

   ret = persComRctWrite(1, key, &psConfig); //key is NULL
   x_fail_unless(ret < 0 , "Writing RCT key that is NULL works, but should fail");

   ret = persComRctWrite(1, "key", BadPsConfig); //data is NULL
   x_fail_unless(ret < 0 , "Writing RCT data that is NULL works, but should fail");



   ret = persComDbReadKey(1, "CteKh3FTonalS4AlOaEruzUbgAP9fryYJLCykq5tTPQkPrHEcV9p6akxa6TuF9gqnJu5iCEyxMUu17QhTP7sYgFwFKU1qqNMcCmps8WcpWDR2oCnjqdaBtATL2A36q6QV", "data", 4); //key too long
   x_fail_unless(ret < 0 , "Reading with wrong keylength works, but should fail");

   ret = persComDbReadKey(-1, "key", buffer, 4); //negative handle
   x_fail_unless(ret < 0 , "Reading with negative handle works, but should fail");

   ret = persComDbReadKey(1, key, buffer, 4); //key is NULL
   x_fail_unless(ret < 0 , "Reading key that is NULL works, but should fail");

   ret = persComDbReadKey(1, "key", Badbuffer, 4); //data is NULL
   x_fail_unless(ret < 0 , "Reading data to buffer that is NULL works, but should fail");

   ret = persComDbReadKey(1, "key", buffer, -1254); //data buffer size is negative
   x_fail_unless(ret < 0 , "Reading with negative data buffer size works, but should fail");

   ret = persComDbReadKey(1, "key", buffer, 0); //data buffer size is zero
   x_fail_unless(ret < 0 , "Reading with zero data buffer size works, but should fail");



   ret = persComRctRead(1, "CteKh3FTonalS4AlOaEruzUbgAP9fryYJLCykq5tTPQkPrHEcV9p6akxa6TuF9gqnJu5iCEyxMUu17QhTP7sYgFwFKU1qqNMcCmps8WcpWDR2oCnjqdaBtATL2A36q6QV", &psConfig); //key too long
   x_fail_unless(ret < 0 , "Reading RCT with wrong keylength works, but should fail");

   ret = persComRctRead(-1, "key", &psConfig); //negative handle
   x_fail_unless(ret < 0 , "Reading RCT with negative handle works, but should fail");

   ret = persComRctRead(1, key, &psConfig); //key is NULL
   x_fail_unless(ret < 0 , "Reading RCT key that is NULL works, but should fail");

   ret = persComRctRead(1, "key", BadPsConfig); //data is NULL
   x_fail_unless(ret < 0 , "Reading RCT data to buffer that is NULL works, but should fail");



   ret = persComDbGetSizeKeysList(-1);
   x_fail_unless(ret < 0 , "Reading keylist size with negative handle works, but should fail");

   ret = persComRctGetSizeResourcesList(-1);
   x_fail_unless(ret < 0 , "Reading RCT resourcelist size with negative handle works, but should fail");



   ret = persComDbGetKeySize(1, "CteKh3FTonalS4AlOaEruzUbgAP9fryYJLCykq5tTPQkPrHEcV9p6akxa6TuF9gqnJu5iCEyxMUu17QhTP7sYgFwFKU1qqNMcCmps8WcpWDR2oCnjqdaBtATL2A36q6QV"); //key too long
   x_fail_unless(ret < 0 , "Reading Size with wrong keylength works, but should fail");

   ret = persComDbGetKeySize(-1, "key"); //negative handle
   x_fail_unless(ret < 0 , "Reading Size with negative handle works, but should fail");

   ret = persComDbGetKeySize(1, key); //key is NULL
   x_fail_unless(ret < 0 , "Reading Size from key that is NULL works, but should fail");




   ret = persComDbGetKeysList(-1, buffer, 10); //
   x_fail_unless(ret < 0 , "Reading key list with negative handle works, but should fail");

   ret = persComDbGetKeysList(1, Badbuffer, 10); //
   x_fail_unless(ret < 0 , "Reading key list with readbuffer that is NULL works, but should fail");

   ret = persComDbGetKeysList(1, buffer, -1); //
   x_fail_unless(ret < 0 , "Reading key list with negative buffer size works, but should fail");

   ret = persComDbGetKeysList(1, buffer, 0); //
   x_fail_unless(ret < 0 , "Reading key list with zero buffer size works, but should fail");





   ret = persComRctGetResourcesList(-1, buffer, 10); //
   x_fail_unless(ret < 0 , "Reading RCT key list with negative handle works, but should fail");

   ret = persComRctGetResourcesList(1, Badbuffer, 10); //
   x_fail_unless(ret < 0 , "Reading RCT key list with readbuffer that is NULL works, but should fail");

   ret = persComRctGetResourcesList(1, buffer, -1); //
   x_fail_unless(ret < 0 , "Reading RCT key list with negative buffer size works, but should fail");

   ret = persComRctGetResourcesList(1, buffer, 0); //
   x_fail_unless(ret < 0 , "Reading RCT key list with zero buffer size works, but should fail");


   ret = persComDbDeleteKey(1, "CteKh3FTonalS4AlOaEruzUbgAP9fryYJLCykq5tTPQkPrHEcV9p6akxa6TuF9gqnJu5iCEyxMUu17QhTP7sYgFwFKU1qqNMcCmps8WcpWDR2oCnjqdaBtATL2A36q6QV");
   x_fail_unless(ret < 0 , "Deleting key with wrong keylength works, but should fail");

   ret = persComDbDeleteKey(1, key);
   x_fail_unless(ret < 0 , "Deleting key with key that is NULL works, but should fail");

   ret = persComDbDeleteKey(-1, "key");
   x_fail_unless(ret < 0 , "Deleting with negative handle works, but should fail");


   ret = persComRctDelete(1, "CteKh3FTonalS4AlOaEruzUbgAP9fryYJLCykq5tTPQkPrHEcV9p6akxa6TuF9gqnJu5iCEyxMUu17QhTP7sYgFwFKU1qqNMcCmps8WcpWDR2oCnjqdaBtATL2A36q6QV");
   x_fail_unless(ret < 0 , "Deleting RCT key with wrong keylength works, but should fail");

   ret = persComRctDelete(1, key);
   x_fail_unless(ret < 0 , "Deleting RCT key with key that is NULL works, but should fail");

   ret = persComRctDelete(-1, "key");
   x_fail_unless(ret < 0 , "Deleting RCT key with negative handle works, but should fail");


}
END_TEST





START_TEST(test_Backups)
{
   X_TEST_REPORT_TEST_NAME("persistence_common_object_test");
   X_TEST_REPORT_COMP_NAME("libpers_common");
   X_TEST_REPORT_REFERENCE("NONE");
   X_TEST_REPORT_DESCRIPTION("Test of backup functionality");
   X_TEST_REPORT_TYPE(GOOD);

   int ret = 0;
   int handle = 0;
         char write2[READ_SIZE] = { 0 };
   char read[READ_SIZE] = { 0 };
   char key[128] = { 0 };
   int i =0;

//   //Cleaning up testdata folder
   if (remove("/tmp/backups-localdb.db") == 0)
      printf("File %s  deleted.\n", "/tmp/backups-localdb.db");
   else
      fprintf(stderr, "Warning:  Could not delete file [%s].\n", "/tmp/backups-localdb.db");


#if 1
snprintf(write2, 176 , "%s", "/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/");


handle = persComDbOpen("/tmp/backups-localdb.db", 0x1); //create test.db if not present
x_fail_unless(handle >= 0, "Failed to create non existent lDB: retval: [%d]", ret);
//write to cache
for(i=0; i< 2; i++)
{
   snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
   ret = persComDbWriteKey(handle, key, (char*) write2, strlen(write2));
   x_fail_unless(ret == strlen(write2) , "Wrong write size while inserting in cache");
}

//read from cache
for(i=0; i< 1; i++)
{
   snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);
   ret = persComDbReadKey(handle, key, (char*) read, strlen(write2));
   x_fail_unless(ret == strlen(write2), "Wrong read size while reading from cache");
   x_fail_unless(memcmp(read, write2, sizeof(write2)) == 0, "Reading Data from Cache failed: Buffer not correctly read");
}

//persist data in cache to file
ret = persComDbClose(handle);
if (ret != 0)
   printf("persComDbClose() failed: [%d] \n", ret);
x_fail_unless(ret == 0, "Failed to close cached database: retval: [%d]", ret);


// IF DATABASE HEADER STRUCTURES OR KEY VALUE PAIR STORAGE CHANGES, the seek to offset part must be updated
//open database and make data corrupt
int fd;
FILE* f;
fd = open("/tmp/backups-localdb.db", O_RDWR , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH  ); //gets closed when f is closed
f = fdopen(fd, "w+b");

//seek to data of offsetA for Key_in_loop_0_0
fseeko(f,7357, SEEK_SET);
fputc('b',f); //make data corrupt

//seek to data of offsetA for Key_in_loop_1_1
fseeko(f,40389, SEEK_SET);
fputc('b',f); //make data corrupt

//seek to backupdata of offsetB for Key_in_loop_1_1
fseeko(f,56905, SEEK_SET);
fputc('b',f); //make backup data corrupt
fclose(f);

handle = persComDbOpen("/tmp/backups-localdb.db", 0x1); //create test.db if not present
x_fail_unless(handle >= 0, "Failed to reopen existing lDB: retval: [%d]", ret);
memset(read, 0 , sizeof(read));
//read from database file must return backup data
for(i=0; i< 1; i++)
{
   snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i);  //Key_in_loop_0_0
   ret = persComDbReadKey(handle, key, (char*) read, strlen(write2));
   //printf("read returnval: %d Data: %s \n", ret, read);
   memset(read, 0 , sizeof(read));
   x_fail_unless(ret == strlen(write2), "Wrong read size");
}

//test when backupdata is also corrupted -> no data should be returned
for(i=1; i< 2; i++)
{
   snprintf(key, 128, "Key_in_loop_%d_%d",i,i*i); //Key_in_loop_1_1
   ret = persComDbReadKey(handle, key, (char*) read, strlen(write2));
   //printf("read returnval: %d Data: %s \n", ret, read);
   memset(read, 0 , sizeof(read));
   x_fail_unless(ret < 0 , "Reading for key with corrupt data and corrupt backup worked, but should fail");
}

ret = persComDbClose(handle);
if (ret != 0)
   printf("persComDbClose() failed: [%d] \n", ret);
x_fail_unless(ret == 0, "Failed to close database file: retval: [%d]", ret);

#endif
}
END_TEST






static Suite * persistenceCommonLib_suite()
{
   Suite * s = suite_create("Persistence common object");

   TCase * tc_persOpenLocalDB = tcase_create("OpenlocalDB");
   tcase_add_test(tc_persOpenLocalDB, test_OpenLocalDB);

   TCase * tc_persOpenRCT = tcase_create("OpenRCT");
   tcase_add_test(tc_persOpenRCT, test_OpenRCT);

   TCase * tc_persSetDataLocalDB = tcase_create("SetDataLocalDB");
   tcase_add_test(tc_persSetDataLocalDB, test_SetDataLocalDB);
   tcase_set_timeout(tc_persSetDataLocalDB, 20);

   TCase * tc_persGetDataLocalDB = tcase_create("GetDataLocalDB");
   tcase_add_test(tc_persGetDataLocalDB, test_GetDataLocalDB);
   tcase_set_timeout(tc_persGetDataLocalDB, 20);

   TCase * tc_persSetDataRCT = tcase_create("SetDataRCT");
   tcase_add_test(tc_persSetDataRCT, test_SetDataRCT);

   TCase * tc_persGetDataRCT = tcase_create("GetDataRCT");
   tcase_add_test(tc_persGetDataRCT, test_GetDataRCT);

   TCase * tc_persGetDataSize = tcase_create("GetDataSize");
   tcase_add_test(tc_persGetDataSize, test_GetDataSize);

   TCase * tc_persDeleteDataLocalDB = tcase_create("DeleteDataLocalDB");
   tcase_add_test(tc_persDeleteDataLocalDB, test_DeleteDataLocalDB);

   TCase * tc_persDeleteDataRct = tcase_create("DeleteDataRct");
   tcase_add_test(tc_persDeleteDataRct, test_DeleteDataRct);

   TCase * tc_persGetKeyListSizeLocalDB = tcase_create("GetKeyListSizeLocalDb");
   tcase_add_test(tc_persGetKeyListSizeLocalDB, test_GetKeyListSizeLocalDB);

   TCase * tc_persGetKeyListLocalDB = tcase_create("GetKeyListLocalDb");
   tcase_add_test(tc_persGetKeyListLocalDB, test_GetKeyListLocalDB);

   TCase * tc_persGetResourceListSizeRct = tcase_create("GetResourceListSizeRct");
   tcase_add_test(tc_persGetResourceListSizeRct, test_GetResourceListSizeRct);

   TCase * tc_persGetResourceListRct = tcase_create("GetResourceListRct");
   tcase_add_test(tc_persGetResourceListRct, test_GetResourceListRct);

   TCase * tc_persCacheSize = tcase_create("CacheSize");
   tcase_add_test(tc_persCacheSize, test_CacheSize);
   tcase_set_timeout(tc_persCacheSize, 20);

   TCase * tc_persCachedConcurrentAccess = tcase_create("CachedConcurrentAccess");
   tcase_add_test(tc_persCachedConcurrentAccess, test_CachedConcurrentAccess);
   tcase_set_timeout(tc_persCachedConcurrentAccess, 20);

   //TCase * tc_persSharedHeaderResize = tcase_create("SharedHeaderResize");
   //tcase_add_test(tc_persSharedHeaderResize, test_SharedHeaderResize);
   //tcase_set_timeout(tc_persSharedHeaderResize, 20);


   //TCase * tc_RemapHashtableHeader = tcase_create("RemapHashtableHeader");
   //tcase_add_test(tc_RemapHashtableHeader, test_RemapHashtableHeader);
   //tcase_set_timeout(tc_RemapHashtableHeader, 20);

//   TCase * tc_SupportedChars = tcase_create("SupportedChars");
//   tcase_add_test(tc_SupportedChars, test_SupportedChars);
//   tcase_set_timeout(tc_SupportedChars, 20);

   TCase * tc_BadParameters = tcase_create("BadParameters");
   tcase_add_test(tc_BadParameters, test_BadParameters);
   tcase_set_timeout(tc_BadParameters, 5);

   TCase * tc_Backups = tcase_create("Backups");
   tcase_add_test(tc_Backups, test_Backups);
   tcase_set_timeout(tc_Backups, 5);



   suite_add_tcase(s, tc_persOpenLocalDB);
   suite_add_tcase(s, tc_persOpenRCT);
   suite_add_tcase(s, tc_persSetDataLocalDB);
   suite_add_tcase(s, tc_persGetDataLocalDB);
   suite_add_tcase(s, tc_persSetDataRCT);
   suite_add_tcase(s, tc_persGetDataRCT);
   suite_add_tcase(s, tc_persGetDataSize);
   suite_add_tcase(s, tc_persDeleteDataLocalDB);
   suite_add_tcase(s, tc_persDeleteDataRct);
   suite_add_tcase(s, tc_persGetKeyListSizeLocalDB);
   suite_add_tcase(s, tc_persGetKeyListLocalDB);
   suite_add_tcase(s, tc_persGetResourceListSizeRct);
   suite_add_tcase(s, tc_persGetResourceListRct);
   suite_add_tcase(s, tc_persCacheSize);
   suite_add_tcase(s, tc_persCachedConcurrentAccess);
               //suite_add_tcase(s, tc_RemapHashtableHeader);
               //suite_add_tcase(s, tc_SupportedChars);
   suite_add_tcase(s, tc_BadParameters);
   //suite_add_tcase(s, tc_Backups);

   return s;
}

int main(int argc, char *argv[])
{
   int nr_failed = 0, nr_run = 0, i = 0;
   TestResult** tResult;

   // assign application name
   strncpy(gTheAppId, "lt-persistence_common_object_test", MaxAppNameLen);
   gTheAppId[MaxAppNameLen - 1] = '\0';

   /// debug log and trace (DLT) setup
   DLT_REGISTER_APP("PCOt", "tests the persistence common object library");

#if 1
   Suite * s = persistenceCommonLib_suite();
   SRunner * sr = srunner_create(s);
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

   // unregister debug log and trace
   DLT_UNREGISTER_APP()
   ;

   dlt_free();
   return (0 == nr_failed) ? EXIT_SUCCESS : EXIT_FAILURE;
}

