/*
Copyright (c) 2011, Jonas Tarnstrom and ESN Social Software AB
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. All advertising materials mentioning features or use of this software
   must display the following acknowledgement:
   This product includes software developed by ESN Social Software AB (www.esn.me).
4. Neither the name of the ESN Social Software AB nor the
   names of its contributors may be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY ESN SOCIAL SOFTWARE AB ''AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ESN SOCIAL SOFTWARE AB BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Portions of code from:
MODP_ASCII - Ascii transformations (upper/lower, etc)
http://code.google.com/p/stringencoders/
Copyright (c) 2007  Nick Galbreath -- nickg [at] modp [dot] com. All rights reserved.

*/

#include "ultrajson.h"
#include <math.h>
#include <assert.h>
#include <string.h>

struct DecoderState
{
	char *start;
	char *end;
	char *escStart;
	char *escEnd;
	int escHeap;
	int lastType;
	JSONObjectDecoder *dec;
};

JSOBJ decode_any( struct DecoderState *ds);

typedef JSOBJ (*PFN_DECODER)( struct DecoderState *ds);
PFN_DECODER g_identTable[256] = { NULL }; 

static const double g_pow10[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};


/*
#define RETURN_JSOBJ_NULLCHECK(_expr)\
{ \
	JSOBJ obj__ = (_expr); \
	if (obj__ == NULL) \
		fprintf (stderr, "Function at %s:%d returned NULL", __FUNCTION__, __LINE__); \
	return obj__; \
} \
*/

#define RETURN_JSOBJ_NULLCHECK(_expr) return(_expr);

INLINEFUNCTION double createDouble(double intNeg, double intValue, double frcValue, int frcDecimalCount)
{
	return (intValue + (frcValue / g_pow10[frcDecimalCount])) * intNeg;
}

JSOBJ SetError( struct DecoderState *ds, int offset, const char *message)
{
	ds->dec->errorOffset = ds->start + offset;
	ds->dec->errorStr = message;
	return NULL;
}


JSOBJ decode_numeric ( struct DecoderState *ds)
{
	int intNeg = 1;
	int expNeg = 1;
	int chr;
	int decimalCount = 0;
	JSLONG intValue;
	double frcValue = 0.0;
	double expValue;
	
	//return ds->dec->newInteger(31337);

	ds->lastType = JT_INVALID;

	// Go one back since the scanner ate one
	ds->start --;

	if (*(ds->start) == '-')
	{
		ds->start ++;
		intNeg = -1;
	}

	// Scan integer part
	intValue = 0;

	while (1)
	{
		chr = (int) (unsigned char) *(ds->start);

		switch (chr)
		{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			//FIXME: Check for arithemtic overflow here
			intValue = intValue * 10 + (JSLONG) (chr - 48);
			ds->start ++;
			break;

		case '.':
			ds->start ++;
			goto DECODE_FRACTION;
			break;

		case 'e':
		case 'E':
			ds->start ++;
			goto DECODE_EXPONENT;
			break;

		default:
			goto BREAK_INT_LOOP;
			break;
		}
	}

BREAK_INT_LOOP:

	ds->lastType = JT_INTEGER;

	/*
	If input string is LONGLONG_MIN here the value is already negative so we should not flip it */
	if (intValue < 0)
	{
		intNeg = 1;
	}
	RETURN_JSOBJ_NULLCHECK(ds->dec->newInteger(intValue * intNeg));

DECODE_FRACTION:

	// Scan fraction part
	frcValue = 0.0;
	while (1)
	{
		chr = (int) (unsigned char) *(ds->start);

		switch (chr)
		{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			if (decimalCount < JSON_DOUBLE_MAX_DECIMALS)
			{
				frcValue = frcValue * 10.0 + (double) (chr - 48);
				decimalCount ++;
			}
			ds->start ++;
			break;

		case 'e':
		case 'E':
			ds->start ++;
			goto DECODE_EXPONENT;
			break;

		default:
			goto BREAK_FRC_LOOP;
		}
	}

BREAK_FRC_LOOP:

	if (intValue < 0)
	{
		intNeg = 1;
	}

	//FIXME: Check for arithemtic overflow here
	ds->lastType = JT_DOUBLE;
	RETURN_JSOBJ_NULLCHECK(ds->dec->newDouble (createDouble( (double) intNeg, (double) intValue, frcValue, decimalCount)));

DECODE_EXPONENT:
	if (*(ds->start) == '-')
	{
		expNeg = -1;
		ds->start ++;
	}
	else
	if (*(ds->start) == '+')
	{
		expNeg = +1;
		ds->start ++;
	}

	expValue = 0.0;

	while (1)
	{
		chr = (int) (unsigned char) *(ds->start);

		switch ((chr = (int) (unsigned char) *(ds->start++)))
		{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			expValue = expValue * 10.0 + (double) (chr - 48);
			ds->start ++;
			break;

		default:
			goto BREAK_EXP_LOOP;

		}
	}

BREAK_EXP_LOOP:

	if (intValue < 0)
	{
		intNeg = 1;
	}
	
	//FIXME: Check for arithemtic overflow here
	ds->lastType = JT_DOUBLE;
	RETURN_JSOBJ_NULLCHECK(ds->dec->newDouble (createDouble( (double) intNeg, (double) intValue , frcValue, decimalCount) * pow(10.0, expValue)));
}

JSOBJ decode_true ( struct DecoderState *ds)
{
	ds->lastType = JT_INVALID;

	if (*(ds->start++) != 'r')
		goto SETERROR;
	if (*(ds->start++) != 'u')
		goto SETERROR;
	if (*(ds->start++) != 'e')
		goto SETERROR;

	ds->lastType = JT_TRUE;
	RETURN_JSOBJ_NULLCHECK(ds->dec->newTrue());

SETERROR:
	return SetError(ds, -1, "Unexpected character found when decoding 'true'");
}

JSOBJ decode_false ( struct DecoderState *ds)
{
	ds->lastType = JT_INVALID;

	if (*(ds->start++) != 'a')
		goto SETERROR;
	if (*(ds->start++) != 'l')
		goto SETERROR;
	if (*(ds->start++) != 's')
		goto SETERROR;
	if (*(ds->start++) != 'e')
		goto SETERROR;

	ds->lastType = JT_FALSE;
	RETURN_JSOBJ_NULLCHECK(ds->dec->newFalse());

SETERROR:
	return SetError(ds, -1, "Unexpected character found when decoding 'false'");

}


JSOBJ decode_null ( struct DecoderState *ds)
{
	ds->lastType = JT_INVALID;

	if (*(ds->start++) != 'u')
		goto SETERROR;
	if (*(ds->start++) != 'l')
		goto SETERROR;
	if (*(ds->start++) != 'l')
		goto SETERROR;

	ds->lastType = JT_NULL;
	RETURN_JSOBJ_NULLCHECK(ds->dec->newNull());

SETERROR:
	return SetError(ds, -1, "Unexpected character found when decoding 'null'");
}


static char g_unescapeLookup[] = {
	/*				 0x00   0x01   0x02    0x03   0x04   0x05    0x06  0x07   0x08   0x09   0x0a   0x0b   0x0c   0x0d   0x0e   0x0f */
	/* 0x00 */ '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',
	/* 0x10 */ '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',
	/* 0x20 */ '\0',  '\0',  '\"',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',
	/* 0x30 */ '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',
	/* 0x40 */ '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',
	/* 0x50 */ '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',
	/* 0x60 */ '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',
	/* 0x70 */ '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',
	/* 0x80 */ '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',
	/* 0x90 */ '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',
	/* 0xa0 */ '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',
	/* 0xb0 */ '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',
	/* 0xc0 */ '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',
	/* 0xd0 */ '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',
	/* 0xe0 */ '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',
	/* 0xf0 */ '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',  '\0',
};

JSOBJ decode_string ( struct DecoderState *ds)
{
	char *escOffset;
	char chr;
	size_t escLen = (ds->escEnd - ds->escStart);
	ds->lastType = JT_INVALID;

	if ( (ds->end - ds->start) > escLen)
	{
		size_t newSize = escLen * 2;

		if (ds->escHeap)
		{
			ds->escStart = (char *) ds->dec->realloc (ds->escStart, escLen);
		}
		else
		{
			char *oldStart = ds->escStart;
			ds->escHeap = 1;
			ds->escStart = (char *) ds->dec->malloc (newSize);
			memcpy (ds->escStart, oldStart, escLen);
		}

		ds->escEnd = ds->escStart + newSize;
	}

	escOffset = ds->escStart;

	while (1)
	{
		chr = (*ds->start++);

		switch (chr)
		{
			case '\0':
				return SetError(ds, -1, "Unmatched ''\"' when when decoding 'string'");

			case '\"':
				ds->lastType = JT_UTF8;
				RETURN_JSOBJ_NULLCHECK(ds->dec->newString(ds->escStart, escOffset));

			case '\\':
				if (*ds->start == '\0')
				{
					return SetError(ds, -1, "Unterminated escape sequence when decoding 'string'");
				}

				chr = g_unescapeLookup[ (unsigned char) (*ds->start++)];

				if (chr == '\0')
				{
					return SetError(ds, -1, "Unrecognized escape sequence when decoding 'string'");
				}
				break;
		}

		*(escOffset++) = chr;
	}



}

JSOBJ decode_array( struct DecoderState *ds)
{
	JSOBJ itemValue;
	JSOBJ newObj = ds->dec->newArray();

	ds->lastType = JT_INVALID;

	while ((*ds->start) != '\0')
	{
		itemValue = decode_any(ds);

		if (itemValue == NULL)
		{
			ds->lastType = JT_ARRAY;
			return newObj;
		}

		ds->dec->arrayAddItem (newObj, itemValue);
	}

	return SetError(ds, -1, "Unmatched ']' when decoding 'array'");
}



JSOBJ decode_object( struct DecoderState *ds)
{
	JSOBJ itemName;
	JSOBJ itemValue;
	JSOBJ newObj = ds->dec->newObject();
	ds->lastType = JT_INVALID;

	while ((*ds->start) != '\0')
	{
		itemName = decode_any(ds);

		if (itemName == NULL)
		{
			ds->lastType = JT_OBJECT;
			return newObj;
		}

		if (itemName)
		{
			if (ds->lastType != JT_UTF8)
			{
				return SetError(ds, -1, "Key name of object must be 'string' when decoding 'object'");
			}

			// Expect ':'
			while ((*ds->start) != '\0')
			{
				if (*(ds->start++) == ':')
					break;
			}

			if (ds->start == ds->end)
			{
				return SetError(ds, -1, "No ':' found when decoding object value");
			}

			itemValue = decode_any(ds);
			ds->dec->objectAddKey (newObj, itemName, itemValue);
		}
	}

	return SetError(ds, -1, "Unmatched '}' when decoding object");
}


JSOBJ decode_array_begin( struct DecoderState *ds)
{
	return decode_array(ds);
}

JSOBJ decode_array_end( struct DecoderState *ds)
{
	return NULL;
}

JSOBJ decode_object_begin( struct DecoderState *ds)
{
	return decode_object(ds);
}

JSOBJ decode_object_end( struct DecoderState *ds)
{
	return NULL;
}




JSOBJ decode_item_separator( struct DecoderState *ds)
{
	//FIXME: Validate that we are inside array or object here
	return decode_any(ds);
}


JSOBJ decode_any(struct DecoderState *ds)
{
	ds->lastType = JT_INVALID;

	while (1)
	{
		switch ((*ds->start++))
		{
			case '\"': return decode_string (ds);
			case '0': return decode_numeric (ds);
			case '1': return decode_numeric (ds);
			case '2': return decode_numeric (ds);
			case '3': return decode_numeric (ds);
			case '4': return decode_numeric (ds);
			case '5': return decode_numeric (ds);
			case '6': return decode_numeric (ds);
			case '7': return decode_numeric (ds);
			case '8': return decode_numeric (ds);
			case '9': return decode_numeric (ds);
			case '-': return decode_numeric (ds);
			case '[': return decode_array (ds);
			case '{': return decode_object (ds);
			case 't': return decode_true (ds);
			case 'f': return decode_false (ds);
			case 'n': return decode_null (ds);
			/*
			case '}': return decode_object_end (ds);
			case ']': return decode_array_end (ds);
			case ',': return decode_item_separator (ds);
			*/
			case '}':
			case ']':
				return NULL;

			case ',':
				// FIXME: Validate
				// Trailers
				break; 

			case ' ':
			case '\t':
			case '\r':
			case '\n':
				// White space
				break;

			default:
				return NULL;

		}

	}

	return NULL;
}


JSOBJ JSON_DecodeObject(JSONObjectDecoder *dec, const char *buffer, size_t cbBuffer)
{
	static int s_once = 1;

	struct DecoderState ds;
	int index;
	char escBuffer[65536];
	JSOBJ ret;
	

	ds.start = (char *) buffer;
	ds.end = ds.start + cbBuffer;
	ds.lastType = JT_INVALID;
	ds.escStart = escBuffer;
	ds.escEnd = ds.escStart + sizeof(escBuffer);
	ds.escHeap = 0;
	ds.dec = dec;
	ds.dec->errorStr = NULL;
	ds.dec->errorOffset = NULL;

	if (s_once)
	{
		//FIXME: Move to static initialization instead
		g_identTable['\"'] = decode_string;
		g_identTable['0'] = decode_numeric;
		g_identTable['1'] = decode_numeric;
		g_identTable['2'] = decode_numeric;
		g_identTable['3'] = decode_numeric;
		g_identTable['4'] = decode_numeric;
		g_identTable['5'] = decode_numeric;
		g_identTable['6'] = decode_numeric;
		g_identTable['7'] = decode_numeric;
		g_identTable['8'] = decode_numeric;
		g_identTable['9'] = decode_numeric;
		g_identTable['0'] = decode_numeric;
		g_identTable['-'] = decode_numeric;
		g_identTable['['] = decode_array;
		g_identTable['{'] = decode_object; 
		g_identTable['t'] = decode_true;
		g_identTable['f'] = decode_false;
		g_identTable['n'] = decode_null; 
		g_identTable['}'] = decode_object_end; 
		g_identTable[']'] = decode_array_end; 
		g_identTable[','] = decode_item_separator; 
		for (index = 0; index < 33; index ++)
		{
			g_identTable[index] = (PFN_DECODER) 1; 
		}

		for (index = 0; index < 256; index ++)
		{
			g_unescapeLookup[index] = '\0';
		}

		g_unescapeLookup['\\'] = '\\';
		g_unescapeLookup['\"'] = '\"';
		g_unescapeLookup['/'] = '/';
		g_unescapeLookup['b'] = '\b';
		g_unescapeLookup['f'] = '\f';
		g_unescapeLookup['n'] = '\n';
		g_unescapeLookup['r'] = '\r';
		g_unescapeLookup['t'] = '\t';

		//FIXME: Implement unicode decode here
		s_once = 0;
	}

	ds.dec = dec;

	ret = decode_any (&ds);
	
	if (ds.escHeap)
	{
		dec->free(ds.escStart);
	}
	return ret;
}
