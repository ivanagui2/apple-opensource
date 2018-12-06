/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma	ident	"@(#)tst.printcont.d	1.1	06/08/28 SMI"

/*
 * ASSERTION:
 * Test 'uint' continously; like kinda stress.
 *
 * SECTION: Output Formatting/printf()
 *
 */

#pragma D option quiet

uint64_t ts;

BEGIN
{
	ts = 53114233149441;
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	printf("%u\n", ts);
	exit(0);
}