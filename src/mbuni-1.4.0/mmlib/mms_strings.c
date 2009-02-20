/*
 * Mbuni - Open  Source MMS Gateway 
 * 
 * MMS strings module
 * 
 * Copyright (C) 2003 - 2008, Digital Solutions Ltd. - http://www.dsmagic.com
 *
 * Paul Bagyenda <bagyenda@dsmagic.com>
 * 
 * This program is free software, distributed under the terms of
 * the GNU General Public License, with a few exceptions granted (see LICENSE)
 */

#include "gwlib/gwlib.h"
#include "mms_strings.h"


#define TABLE_SIZE(table) ((long)(sizeof(table) / sizeof(table[0])))

static int initialized;

/* The arrays in a table structure are all of equal length, and their
 * elements correspond.  The number for string 0 is in numbers[0], etc.
 * Table structures are initialized dynamically.
 */
struct table
{
    long size;          /* Nr of entries in each of the tables below */
    Octstr **strings;   /* Immutable octstrs */
    long *numbers;      /* Assigned numbers, or NULL for linear tables */
    int *versions;      /* WSP Encoding-versions, or NULL if non-versioned */
    int linear;	        /* True for tables defined as LINEAR */
};

struct numbered_element
{
    unsigned char *str;
    long number;
    int version;    
};

struct linear_element
{
    unsigned char *str;
    int version;    
};

/* Local functions */
static Octstr *number_to_string(long number, struct table *table);
static unsigned char *number_to_cstr(long number, struct table *table);
static long string_to_number(Octstr *ostr, struct table *table);
static long string_to_versioned_number(Octstr *ostr, struct table *table, int version);


/* Declare the data.  For each table "foo", create a foo_strings array
 * to hold the data, and a (still empty) foo_table structure. */
#define LINEAR(name, strings) \
    static const struct linear_element name##_strings[] = { strings }; \
    static struct table name##_table;
#define STRING(string) { (unsigned char *)string, 0 },
#define VSTRING(version, string) { (unsigned char *)string, version }, 
#define NUMBERED(name, strings) \
    static const struct numbered_element name##_strings[] = { strings }; \
    static struct table name##_table;
#define ASSIGN(string, number) { (unsigned char *)string, number, 0 },
#define VASSIGN(version, string, number) { (unsigned char *)string, number, version },
#include "mms_strings.def"

/* Define the functions for translating number to Octstr */
#define LINEAR(name, strings) \
Octstr *mms_##name##_to_string(long number) { \
    return number_to_string(number, &name##_table); \
}
#include "mms_strings.def"

/* Define the functions for translating number to constant string */
#define LINEAR(name, strings) \
unsigned char *mms_##name##_to_cstr(long number) { \
    return number_to_cstr(number, &name##_table); \
}
#include "mms_strings.def"

#define LINEAR(name, strings) \
long mms_string_to_##name(Octstr *ostr) { \
     return string_to_number(ostr, &name##_table); \
}
#include "mms_strings.def"

#define LINEAR(name, strings) \
long mms_string_to_versioned_##name(Octstr *ostr, int version) { \
     return string_to_versioned_number(ostr, &name##_table, version); \
}
#include "mms_strings.def"

static Octstr *number_to_string(long number, struct table *table)
{
    long i;

    gw_assert(initialized);

    if (table->linear) {
        if (number >= 0 && number < table->size)
            return octstr_duplicate(table->strings[number]);
    } else {
        for (i = 0; i < table->size; i++) {
            if (table->numbers[i] == number)
                return octstr_duplicate(table->strings[i]);
        }
    }
    return octstr_imm("");
}

static unsigned char *number_to_cstr(long number, struct table *table)
{
    long i;

    gw_assert(initialized);

    if (table->linear) {
	if (number >= 0 && number < table->size)
	  return (unsigned char *)octstr_get_cstr(table->strings[number]);
    } else {
	for (i = 0; i < table->size; i++) {
   	     if (table->numbers[i] == number)
	       return (unsigned char *)octstr_get_cstr(table->strings[i]);
	}
    }
    return NULL;
}

/* Case-insensitive string lookup */
static long string_to_number(Octstr *ostr, struct table *table)
{
    long i;

    gw_assert(initialized);

    for (i = 0; i < table->size; i++) {
	if (octstr_case_compare(ostr, table->strings[i]) == 0) {
	    return table->linear ? i : table->numbers[i];
	}
    }

    return -1;
}

/* Case-insensitive string lookup according to passed WSP encoding version */
static long string_to_versioned_number(Octstr *ostr, struct table *table, 
                                       int version)
{
    unsigned int i, ret;

    gw_assert(initialized);

    /* walk the whole table and pick the highest versioned token */
    ret = -1;
    for (i = 0; i < table->size; i++) {
        if (octstr_case_compare(ostr, table->strings[i]) == 0 &&
            table->versions[i] <= version) {
                ret = table->linear ? i : table->numbers[i];
        }
    }

    debug("mms.strings",0,"MMS: Mapping string `%s', MMS version 1.%d to binary " 
          "representation `0x%04x'.", octstr_get_cstr(ostr), version, ret);

    return ret;
}

static void construct_linear_table(struct table *table, const struct linear_element *strings, 
                                   long size)
{
    long i;

    table->size = size;
    table->strings = gw_malloc(size * (sizeof table->strings[0]));
    table->numbers = NULL;
    table->versions = gw_malloc(size * (sizeof table->versions[0]));
    table->linear = 1;

    for (i = 0; i < size; i++) {
      table->strings[i] = octstr_imm((char *)strings[i].str);
        table->versions[i] = strings[i].version;
    }
}

static void construct_numbered_table(struct table *table, const struct numbered_element *strings, 
                                     long size)
{
    long i;

    table->size = size;
    table->strings = gw_malloc(size * (sizeof table->strings[0]));
    table->numbers = gw_malloc(size * (sizeof table->numbers[0]));
    table->versions = gw_malloc(size * (sizeof table->versions[0]));
    table->linear = 0;

    for (i = 0; i < size; i++) {
      table->strings[i] = octstr_imm((char *)strings[i].str);
        table->numbers[i] = strings[i].number;
        table->versions[i] = strings[i].version;
    }
}

static void destroy_table(struct table *table)
{
    /* No need to call octstr_destroy on immutable octstrs */

    gw_free(table->strings);
    gw_free(table->numbers);
    gw_free(table->versions);
}

void mms_strings_init(void)
{
  wsp_strings_init();
    if (initialized > 0) {
         initialized++;
         return;
    }

#define LINEAR(name, strings) \
    construct_linear_table(&name##_table, \
		name##_strings, TABLE_SIZE(name##_strings));
#define NUMBERED(name, strings) \
    construct_numbered_table(&name##_table, \
	        name##_strings, TABLE_SIZE(name##_strings));
#include "mms_strings.def"
    initialized++;
}

void mms_strings_shutdown(void)
{
     wsp_strings_shutdown();
    /* If we were initialized more than once, then wait for more than
     * one shutdown. */
    if (initialized > 1) {
        initialized--;
        return;
    }

#define LINEAR(name, strings) \
    destroy_table(&name##_table);
#include "mms_strings.def"

    initialized = 0;
}
