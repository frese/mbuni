/* ==================================================================== 
 * The Kannel Software License, Version 1.0 
 * 
 * Copyright (c) 2001-2008 Kannel Group  
 * Copyright (c) 1998-2001 WapIT Ltd.   
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 * 
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in 
 *    the documentation and/or other materials provided with the 
 *    distribution. 
 * 
 * 3. The end-user documentation included with the redistribution, 
 *    if any, must include the following acknowledgment: 
 *       "This product includes software developed by the 
 *        Kannel Group (http://www.kannel.org/)." 
 *    Alternately, this acknowledgment may appear in the software itself, 
 *    if and wherever such third-party acknowledgments normally appear. 
 * 
 * 4. The names "Kannel" and "Kannel Group" must not be used to 
 *    endorse or promote products derived from this software without 
 *    prior written permission. For written permission, please  
 *    contact org@kannel.org. 
 * 
 * 5. Products derived from this software may not be called "Kannel", 
 *    nor may "Kannel" appear in their name, without prior written 
 *    permission of the Kannel Group. 
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED.  IN NO EVENT SHALL THE KANNEL GROUP OR ITS CONTRIBUTORS 
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,  
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT  
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR  
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,  
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE  
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,  
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 * ==================================================================== 
 * 
 * This software consists of voluntary contributions made by many 
 * individuals on behalf of the Kannel Group.  For more information on  
 * the Kannel Group, please see <http://www.kannel.org/>. 
 * 
 * Portions of this software are based upon software originally written at  
 * WapIT Ltd., Helsinki, Finland for the Kannel project.  
 */ 

/*
 * test_normalize_number.c - simple testing of normalize_number()
 */

#include "gwlib/gwlib.h"

int test_normalize_number(char* prefix, Octstr* number, Octstr* expected) {
	Octstr* orig_number = octstr_duplicate(number);
	normalize_number(prefix, &number);
	if (0 != octstr_compare(number,expected))
		panic(0, "normalize(%s,%s) => [%s] : [%s] expected",
			prefix,
			octstr_get_cstr(orig_number),
			octstr_get_cstr(number),
			octstr_get_cstr(expected));

	octstr_destroy(orig_number);
}

int main(void) {
	char *prefix;
	Octstr *number;
	Octstr *test;

	gwlib_init();
	
	// Testcase 1: Simple replace
	number = octstr_create("212212therest");
	test_normalize_number("xxx,212212", number, octstr_imm("xxxtherest"));
	
	// Testcase 2: No replace!
	number = octstr_create("234092343");
	test_normalize_number("blah,1234", number, number);

	// Testcase 3: unified-prefix part of prefix 
	number = octstr_create("21221261244845");
	test_normalize_number("212,212212", number, octstr_imm("21261244845"));
	
	// Testcase 4: prefix part of unified prefix 
	number = octstr_create("21221261244845");
	test_normalize_number("212212,212", number, octstr_imm("21221221261244845"));

	// Testcase 5: multiple replacements, replace in first
	number = octstr_create("+21221261244845");
	test_normalize_number("00,+;00212,00212212", number, octstr_imm("0021221261244845"));
	
	// Testcase 6: multiple replacements, replace in last
	number = octstr_create("+21221261244845");
	test_normalize_number("00212,00212212;00,+", number, octstr_imm("0021221261244845"));

	// Testcase 7: documentation exampl, e.g.
	// Normalize 'number', like change number "040500" to "0035840500" if
	// the dial-prefix is like "0035840,040;0035850,050"
	number = octstr_create("040500");
	test_normalize_number("0035840,040;0035850,050", number, octstr_imm("0035840500"));


	printf("Success\n");


	gwlib_shutdown();
	
	return 0;
}

