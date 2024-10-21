/* 
 * tclParser.c --
 *
 *	This is a Tcl language parser as a Tcl dynamically loadable
 *	extension.
 *
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * Copyright (c) 2000 Ajuba Solutions
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclParser.c,v 1.6 2001/10/17 18:07:41 andreas_kupries Exp $
 */

/*
 * tclInt.h is used for TclFindElement
 */

#include <tclInt.h>


#if TCL_MAJOR_VERSION < 9
#ifdef Tcl_Size
#undef Tcl_Size
#endif
typedef int Tcl_Size;
#define Tcl_GetSizeIntFromObj Tcl_GetIntFromObj
#endif

/*
 * TCL_TOKEN_EXPAND_WORD is new in 8.5, but it's safe to define it
 * when building in earlier releases, and a version of tclparser
 * built that way continues to work in 8.5.
 */

#ifndef TCL_TOKEN_EXPAND_WORD
#define TCL_TOKEN_EXPAND_WORD 256
#endif

/*
 * The max number of characters needed to sprintf
 * an integer, a space and a double.
 */

#define MAX_RANGE_SIZE 100

/*
 * name and version of this package
 */

static char packageName[] = "parser";
static char packageVersion[] = PACKAGE_VERSION;

/*
 * Declarations for functions defined in this file.
 */

#ifdef BUILD_tclparser
#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT
#endif

EXTERN int Tclparser_Init (Tcl_Interp *interp);

static int	ParseMakeTokenList (char *script,
		    Tcl_Parse *parsePtr, int index, Tcl_Obj **resultPtr);
static Tcl_Obj *ParseMakeRange (char *script, const char *start,
		    int end);
static int	ParseObjCmd (ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static void	ParseSetErrorCode (Tcl_Interp *interp,
		    char *script, Tcl_Parse *parsePtr);
static int	ParseCommand (Tcl_Interp *interp, char *script,
		    int index, int length);
static int	ParseExpr (Tcl_Interp *interp, char *script,
		    int index, int length);
static int	ParseList (Tcl_Interp *interp, char *script,
		    int index, int length);
static int	ParseVarName (Tcl_Interp *interp, char *script,
		    int index, int length);
static int	ParseGetString (Tcl_Interp *interp, char *script,
		    int index, int length);
static int	ParseCharIndex (Tcl_Interp *interp, char *script,
		    int index, int length);
static int	ParseCharLength (Tcl_Interp *interp, char *script,
		    int index, int length);
static int	ParseCountNewline (Tcl_Interp *interp,
		    char *script, Tcl_Size scriptLength, Tcl_Obj *rangePtr1,
		    Tcl_Obj *rangePtr2);
static int	ParseGetIndexAndLength (Tcl_Interp *interp,
		    Tcl_Obj *rangePtr, Tcl_Size scriptLen, Tcl_Size *index,
		    Tcl_Size *length);

/*
 *----------------------------------------------------------------------
 *
 * Tclparser_Init --
 *
 *	This procedure initializes the parse command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tclparser_Init(Tcl_Interp *interp)
{
    if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL) {
	return TCL_ERROR;
    }
    Tcl_CreateObjCommand(interp, "parse", ParseObjCmd, NULL, NULL);
    return Tcl_PkgProvide(interp, packageName, packageVersion);
}

/*
 *----------------------------------------------------------------------
 *
 * ParseObjCmd --
 *
 *	This function implements the Tcl "parse" command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ParseObjCmd(
    ClientData dummy,		/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    int option;
    Tcl_Size index, length, scriptLength;
    char *script;

    static const char *options[] = {
	 "command", "expr", "varname", "list",
	 "getrange", "getstring", "charindex", "charlength",
	 "countnewline", NULL
    };
    enum options {
	PARSE_COMMAND, PARSE_EXPR, PARSE_VARNAME, PARSE_LIST, 
	PARSE_GET_RANGE, PARSE_GET_STR, PARSE_CHAR_INDEX, PARSE_CHAR_LEN, 
	PARSE_COUNT_NWLNE
    };

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "option arg ?arg ...?");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], options, "option", 0, &option)
	    != TCL_OK) {
    	return TCL_ERROR;
    }

    /*
     * Check the number arguments passed to the command and
     * extract information (script, index, length) depending
     * upon the option selected.
     */

    script = Tcl_GetStringFromObj(objv[2], &scriptLength);
    switch ((enum options) option) {
	case PARSE_GET_RANGE:
	    if (objc == 3) {
		index = 0;
		length = scriptLength;
	    } else if (objc == 5) {
		if (Tcl_GetSizeIntFromObj(interp, objv[3], &index) != TCL_OK) {
		    return TCL_ERROR;
		}
		if (Tcl_GetSizeIntFromObj(interp, objv[4], &length) != TCL_OK) {
		    return TCL_ERROR;
		}
		if (index < 0) {
		    index = 0;
		} else if (index >= scriptLength) {
		    index = scriptLength - 1;
		}
		if (length < 0) {
		    length = 0;
		} else if (length > (scriptLength - index)) {
		    length = scriptLength - index;
		}
	    } else {
		Tcl_WrongNumArgs(interp, 2, objv, "string ?index length?");
		return TCL_ERROR;
	    }
	    Tcl_SetObjResult(interp, ParseMakeRange(script, script + index,
		    length));
	    return TCL_OK;

	case PARSE_COMMAND:
	case PARSE_EXPR:
	case PARSE_VARNAME:
	case PARSE_LIST:
	case PARSE_GET_STR: 
	case PARSE_CHAR_INDEX:
	case PARSE_CHAR_LEN: {
	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 2, objv, "string range");
		return TCL_ERROR;
	    }
	    if (ParseGetIndexAndLength(interp, objv[3], scriptLength,  
		&index, &length) != TCL_OK) {
		return TCL_ERROR;	
	    }	    
	    switch ((enum options) option) {
		case PARSE_COMMAND:
		    return ParseCommand(interp, script, index, length);
		case PARSE_EXPR:
		    return ParseExpr(interp, script, index, length);
		case PARSE_VARNAME:
		    return ParseVarName(interp, script, index, length);
		case PARSE_LIST:
		    return ParseList(interp, script, index, length);
		case PARSE_GET_STR:
		    return ParseGetString(interp, script, index, length);
		case PARSE_CHAR_INDEX:
		    return ParseCharIndex(interp, script, index, length);
		case PARSE_CHAR_LEN:
		    return ParseCharLength(interp, script, index, length);
		case PARSE_GET_RANGE:
		case PARSE_COUNT_NWLNE:
		    /* No Op - This will suppress compiler warnings */
		    break;
	    }
	    break;
	}
	case PARSE_COUNT_NWLNE: {
	    Tcl_Obj *range2;
	    if (objc == 5) {
		range2 = objv[4];
	    } else if (objc == 4) {
		range2 = NULL;
	    } else {
		Tcl_WrongNumArgs(interp, 2, objv, "string range ?range?");
		return TCL_ERROR;
	    }
	    return ParseCountNewline(interp, script, scriptLength,
		    objv[3], range2);
	}
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseCommand --
 *
 *	This function parses a script into Tcl commands by calling the
 *	Tcl_ParseCommand function.  This routine returns a list of the
 *	following form: <commentRange> <commandRange> <restRange> <parseTree>
 *	The first range refers to any leading comments before the command.
 *	The second range refers to the command itself.  The third range
 *	contains the remainder of the original range that appears after
 *	the command range.  The parseTree is a list representation
 *	of the parse tree where each node is a list in the form:
 *	<type> <range> <subTree>.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int 
ParseCommand(
    Tcl_Interp *interp,		/* Current interpreter. */
    char *script,		/* Script to parse. */
    int index,			/* Index to the starting point of the 
				 * script. */
    int length)			/* Byte length of script be parsed. */
{
    Tcl_Obj *resultPtr, *listPtr, *tokenPtr;
    Tcl_Parse parse;
    int  i;
    const char *start, *end;

    start = script + index;

    if (Tcl_ParseCommand(interp, start, length, 0, &parse)
	    != TCL_OK) {
	ParseSetErrorCode(interp, script, &parse);
	return TCL_ERROR;
    }

    resultPtr = Tcl_GetObjResult(interp);
    i = 0;
    if (parse.commentStart) {
	Tcl_ListObjAppendElement(interp, resultPtr,
		ParseMakeRange(script, parse.commentStart, parse.commentSize));
    } else {
	Tcl_ListObjAppendElement(interp, resultPtr,
		ParseMakeRange(script, script, 0));
    }
    Tcl_ListObjAppendElement(interp, resultPtr,
	    ParseMakeRange(script, parse.commandStart, parse.commandSize));
    end = parse.commandStart + parse.commandSize;
    Tcl_ListObjAppendElement(interp, resultPtr, 
	    ParseMakeRange(script, end, length - (int) (end - start)));
    listPtr = Tcl_NewListObj(0, NULL);
    while (i < parse.numTokens) {
	i = ParseMakeTokenList(script, &parse, i, &tokenPtr);
	Tcl_ListObjAppendElement(NULL, listPtr, tokenPtr);
    }
    Tcl_ListObjAppendElement(interp, resultPtr, listPtr);
    Tcl_SetObjResult(interp, resultPtr);
    Tcl_FreeParse(&parse);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseExpr --
 *
 *	This function parses a Tcl expression into a tree representation.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ParseExpr(
    Tcl_Interp *interp,		/* Current interpreter. */
    char *script,		/* Script to parse. */
    int index,			/* Index to the starting point of the
				 * script. */
    int length)			/* Byte length of script be parsed. */
{
    Tcl_Obj *resultPtr;
    Tcl_Parse parse;

    resultPtr = Tcl_GetObjResult(interp);

    if (Tcl_ParseExpr(interp, script + index, length, &parse)
	    != TCL_OK) {
	ParseSetErrorCode(interp, script, &parse);
	return TCL_ERROR;
    }

    /*
     * There is only one top level token, so just return it.
     */

    ParseMakeTokenList(script, &parse, 0, &resultPtr);
    Tcl_SetObjResult(interp, resultPtr);
    Tcl_FreeParse(&parse);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseList --
 *
 *	This function parses a Tcl list into a list of ranges.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ParseList(
    Tcl_Interp *interp,		/* Current interpreter. */
    char *script,		/* Script to parse. */
    int index,			/* Index to the starting point of the 
				 * script. */
    int length)			/* Byte length of script be parsed. */
{
    Tcl_Obj *resultPtr;
    Tcl_Size size;
    char c;
    const char *list, *element, *prevList, *last;

    resultPtr = Tcl_NewListObj(0, NULL);
    list = script + index;
    last = list + length;

    for (;;) {
	prevList = list;
	if (TclFindElement(interp, list, length, &element, &list,
		&size, NULL) != TCL_OK) {
	    Tcl_Obj *objv[4];
	    Tcl_DecrRefCount(resultPtr);
	    objv[0] = Tcl_NewStringObj("PARSE", 5);
	    objv[1] = Tcl_NewStringObj("list", -1);
	    objv[2] = Tcl_NewIntObj(list - script);
	    objv[3] = Tcl_GetObjResult(interp);
	    Tcl_SetObjErrorCode(interp, Tcl_NewListObj(4, objv));
	    return TCL_ERROR;
	}
	length -= (list - prevList);
	if (element >= last) {
	    break;
	}

	/*
	 * Check to see if this element was in quotes or braces.
	 * If it is, ensure that the range includes the quotes/braces
	 * so the parser can make decisions based on this fact.
	 */

	if (element > (script + index)) {
	    c = *(element - 1);
	} else {
	    c = 0;
	}
	if (c == '{' || c == '"') {
	    element--;
	    size += 2;
	}
	Tcl_ListObjAppendElement(interp, resultPtr,
		ParseMakeRange(script, (char *)element, size));
    }

    Tcl_SetObjResult(interp, resultPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseVarName --
 *
 *	This function parses a Tcl braced word into a tree representation.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ParseVarName(
    Tcl_Interp *interp,		/* Current interpreter. */
    char *script,		/* Script to parse. */
    int index,			/* Index to the starting point of the 
				 * script. */
    int length)			/* Byte length of script be parsed. */
{
    Tcl_Obj *resultPtr;
    Tcl_Parse parse;

    resultPtr = Tcl_GetObjResult(interp);
    
    if (Tcl_ParseVarName(interp, script + index, length, &parse, 0)
	    != TCL_OK) {
	ParseSetErrorCode(interp, script, &parse);
	return TCL_ERROR;
    }

    /*
     * There is only one top level token, so just return it.
     */

    ParseMakeTokenList(script, &parse, 0, &resultPtr);
    Tcl_SetObjResult(interp, resultPtr);
    Tcl_FreeParse(&parse);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseSetErrorCode --
 *
 *	Set the errorCode variable the standard parser error form.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
ParseSetErrorCode(
    Tcl_Interp *interp,		/* Current interpreter. */
    char *script,		/* Script to parse. */
    Tcl_Parse *parsePtr)	/* Parse state. */
{
    Tcl_Obj *objv[4];
    char *type;

    switch (parsePtr->errorType) {
	case TCL_PARSE_QUOTE_EXTRA:
	    type = "quoteExtra";
	    break;
	case TCL_PARSE_BRACE_EXTRA:
	    type = "braceExtra";
	    break;
	case TCL_PARSE_MISSING_BRACE:
	    type = "missingBrace";
	    break;
	case TCL_PARSE_MISSING_BRACKET:
	    type = "missingBracket";
	    break;
	case TCL_PARSE_MISSING_PAREN:
	    type = "missingParen";
	    break;
	case TCL_PARSE_MISSING_QUOTE:
	    type = "missingQuote";
	    break;
	case TCL_PARSE_MISSING_VAR_BRACE:
	    type = "missingVarBrace";
	    break;
	case TCL_PARSE_SYNTAX:
	    type = "syntax";
	    break;
	case TCL_PARSE_BAD_NUMBER:
	    type = "badNumber";
	    break;
	default:
	    type = "unknown";
	    break;
    }
    objv[0] = Tcl_NewStringObj("PARSE", 5);
    objv[1] = Tcl_NewStringObj(type, -1);
    if (parsePtr->term) {
	objv[2] = Tcl_NewIntObj(parsePtr->term - script);
    } else {
	objv[2] = Tcl_NewIntObj(0);
    }
    objv[3] = Tcl_GetObjResult(interp);
    Tcl_SetObjErrorCode(interp, Tcl_NewListObj(4, objv));
}

/*
 *----------------------------------------------------------------------
 *
 * ParseMakeTokenList --
 *
 *	Make the list representation of a token.  Each token is represented
 *	as a list where the first element is a token type, the second
 *	element is a range, and the third element is a list of
 *	subtokens.
 *
 * Results:
 *	Returns the next token offset and stores a newly allocated
 *	list object in the location referred to by resultPtrPtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ParseMakeTokenList(
    char *script,		/* Pointer to start of script being parsed. */
    Tcl_Parse *parsePtr,	/* Parse information. */
    int index,			/* Index of token to append. */
    Tcl_Obj **resultPtrPtr)	/* Pointer to location where resulting list
				 * object is to be stored. */
{
    Tcl_Token *tokenPtr = parsePtr->tokenPtr + index;
    Tcl_Obj *objv[3];
    int start;
    char *type;

    switch (tokenPtr->type) {
	case TCL_TOKEN_WORD:
	    type = "word";
	    break;
        case TCL_TOKEN_EXPAND_WORD:
            type = "expand";
	    break;
	case TCL_TOKEN_SIMPLE_WORD:
	    type = "simple";
	    break;
	case TCL_TOKEN_TEXT:
	    type = "text";
	    break;
	case TCL_TOKEN_BS:
	    type = "backslash";
	    break;
	case TCL_TOKEN_COMMAND:
	    type = "command";
	    break;
	case TCL_TOKEN_VARIABLE:
	    type = "variable";
	    break;
	case TCL_TOKEN_SUB_EXPR:
	    type = "subexpr";
	    break;
	case TCL_TOKEN_OPERATOR:
	    type = "operator";
	    break;
        default:
	    type = "unknown";
	    break;
    }
    objv[0] = Tcl_NewStringObj(type, -1);
    objv[1] = ParseMakeRange(script, tokenPtr->start, tokenPtr->size);
    objv[2] = Tcl_NewListObj(0, NULL);
    start = index;
    index++;
    while (index <= start + tokenPtr->numComponents) {
	index = ParseMakeTokenList(script, parsePtr, index, resultPtrPtr);
	Tcl_ListObjAppendElement(NULL, objv[2], *resultPtrPtr);
    }

    *resultPtrPtr = Tcl_NewListObj(3, objv);
    return index;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseMakeRange --
 *
 *	Construct a new range object.
 *
 * Results:
 *	Returns a newly allocated Tcl object.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
ParseMakeRange(
    char *script,		/* Pointer to the first byte of the script. */
    const char *start,		/* Pointer to the start of the range. */
    int length)			/* The length of the range. */
{
    Tcl_Obj *objv[2];

    objv[0] = Tcl_NewIntObj(start-script);
    objv[1] = Tcl_NewIntObj(length);
    return Tcl_NewListObj(2, objv);
}

/*
 *----------------------------------------------------------------------
 *
 * ParseGetString --
 *
 *	Extract the string fron the script within the boundaries of
 *	byte oriented index and length.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The interp's result is set.
 *
 *----------------------------------------------------------------------
 */

static int
ParseGetString(
    Tcl_Interp *interp,	    /* Current interpreter. */
    char *script,	    /* Script to parse. */
    int index,		    /* Index to the starting point of the
			     * script. */
    int length)	    /* Byte length of script be parsed. */
{
    Tcl_Obj *resultPtr;

    resultPtr = Tcl_GetObjResult(interp);
    Tcl_SetStringObj(resultPtr, script + index, length);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseCharIndex --
 *
 *	Converts byte oriented index values into character oriented
 *	index values.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The interp's result is set.
 *
 *----------------------------------------------------------------------
 */

static int 
ParseCharIndex(
    Tcl_Interp *interp,	    /* Current interpreter. */
    char *script,	    /* Script to parse. */
    int index,		    /* Index to the starting point of the 
			     * script. */
    int length)	    /* Byte length of script be parsed. */ 
{
    Tcl_Obj *resultPtr;

    resultPtr = Tcl_GetObjResult(interp);
    resultPtr = Tcl_NewLongObj(Tcl_NumUtfChars(script, index));
    Tcl_SetObjResult(interp, resultPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseCharLength --
 *
 *	Converts the given byte length into a character count.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The interp's result is set.
 *
 *----------------------------------------------------------------------
 */

static int
ParseCharLength(
    Tcl_Interp *interp,	    /* Current interpreter. */
    char *script,	    /* Script to parse. */
    int index,		    /* Index to the starting point of the 
			     * script. */
    int length)	    /* Byte length of script be parsed. */ 
{
    Tcl_Obj *resultPtr;

    resultPtr = Tcl_GetObjResult(interp);
    resultPtr = Tcl_NewLongObj(Tcl_NumUtfChars(script+index, length));
    Tcl_SetObjResult(interp, resultPtr);
    return TCL_OK;
;}

/*
 *----------------------------------------------------------------------
 *
 * ParseCountNewline --
 *
 *	Count the number of newlines between a range of characters
 *	in a script.  If two ranges are passed to this function, 
 *	calculate the number of newlines from the beginning index of
 *	the first range up to, but not including, the beginning of 
 *	the second range.  If one range is passed in, count the 
 *	number of newlines from the beginning of the first range 
 *	through the last character in the range.
 *
 *	It is assumed that the indices and lengths are within the
 *	boundaries of the script.  No error checking is done to
 *	verify this.  Use the ParseGetIndexAndRange to validate
 *	the data.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The interp's result is set to the number of newlines counted.
 *
 *----------------------------------------------------------------------
 */

static int
ParseCountNewline(
    Tcl_Interp *interp,	    /* Current interpreter. */
    char *script,	    /* Script to parse. */
    Tcl_Size scriptLength,
    Tcl_Obj *rangePtr1,	    /* Begin counting newlines with this range. */
    Tcl_Obj *rangePtr2)	    /* Possibly NULL, otherwise used to terminate
			     * newline counting */
{
    Tcl_Obj *resultPtr;
    char *subStr;
    char *endStr;
    Tcl_Size offset, index1, index2;
    Tcl_Size  length, length1, length2;
    Tcl_Size  listLen1, listLen2;
    int  numNewline;

    if (Tcl_ListObjLength(interp, rangePtr1, &listLen1) != TCL_OK) {
	return TCL_ERROR;
    }
    if (ParseGetIndexAndLength(interp, rangePtr1, scriptLength,  
	    &index1, &length1) != TCL_OK) {
        return TCL_ERROR;	
    }
    if (rangePtr2 != NULL) {
	if (Tcl_ListObjLength(interp, rangePtr2, &listLen2) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (ParseGetIndexAndLength(interp, rangePtr2, scriptLength,  
	        &index2, &length2) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	listLen2 = 0;
    }

    if ((listLen1 == 0) && (listLen2 == 2)) {
	/*
	 * Counting from the beginning of the file to 
	 * the beginning of the second range.
	 *
	 * example: parse count script {} r2
	 */
	
	offset = 0;
	length = index2;
    } else if ((listLen1 == 2) && (listLen2 == 2)) {
	/*
	 * Counting from the beginning of the first  
	 * range to the beginning of the second range.
	 *
	 * example: parse count script r1 r2
	 */
	
	offset = index1;
	length = (index2 - offset);
    } else {
	/*
	 * Counting from the beginning of the first  
	 * range to the end of the first range.  If
	 * the arg passed was an empty string it 
	 * will count the whole script.
	 *
	 * example: parse count script {}
	 *          parse count script r1
	 */

	offset = index1;
	length = length1;
    }

    subStr = (script + offset);
    endStr = (subStr + length);
    numNewline = 0;
    while (subStr < endStr) {
	if (*subStr == '\n') {
	    numNewline++;
	}
	subStr++;
    }
    
    resultPtr = Tcl_GetObjResult(interp);
    resultPtr = Tcl_NewIntObj(numNewline);
    Tcl_SetObjResult(interp, resultPtr);
 
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseGetIndexAndLength --
 *
 *	Extract the index and length from a Tcl Object.  If the 
 *	Tcl Object does not contain data, return the beginning
 *	of the script as the index and the length of the script
 *	for the length.  If the data in the script is out of the
 *	scripts range (e.g. < 0 or > scriptLength,) and scriptLen 
 *      is >= 0, set the value to the closest point.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The values are written to the index and length arguments.
 *	If scriptLen is >= 0, the values will be normalized based
 *	on the length of the script.
 *
 *----------------------------------------------------------------------
 */

static int 
ParseGetIndexAndLength(
    Tcl_Interp *interp,	    /* Current interpreter. */
    Tcl_Obj    *rangePtr,
    Tcl_Size scriptLen,	    /* Length of script.  If >= 0, then try 
			     * to normalize index and length based
			     * on the length of the script. */
    Tcl_Size *indexPtr,	    /* Index to the starting point of the 
			     * script. */
    Tcl_Size *lengthPtr)    /* Byte length of script be parsed. */ 
{
    Tcl_Obj *itemPtr;
    Tcl_Size listLen;

    if (Tcl_ListObjLength(interp, rangePtr, &listLen) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((listLen != 0) && (listLen != 2)) {
	Tcl_SetResult(interp, "invalid range input: incorrect list size", TCL_STATIC);
	return TCL_ERROR;	
    }
    if ((listLen == 0) && (scriptLen < 0)) {
	Tcl_SetResult(interp, "empty range: no index or length values", 
		TCL_STATIC);
	return TCL_ERROR;	
    }
    
    /*
     * If the range argument is null, then set 'index' to zero
     * and 'length' to the string length of the script.  Otherwise
     * extract 'index' and 'length' from the list.  If index or length
     * is < 0 then set it to 0, if index or length is > then the scripts
     * length, set it to the end of the script.
     */

    if (listLen == 0) {
	*indexPtr = 0;
	*lengthPtr = scriptLen;
    } else {
	Tcl_Size len;
	char *bytes;
	if (Tcl_ListObjIndex(interp, rangePtr, 0, &itemPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_GetSizeIntFromObj(interp, itemPtr, indexPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_ListObjIndex(interp, rangePtr, 1, &itemPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	bytes = Tcl_GetStringFromObj(itemPtr, &len);
	if ((*bytes == 'e')
		&& (strncmp(bytes, "end", (unsigned) len) == 0)) {
	    *lengthPtr = scriptLen;
	} else if (Tcl_GetSizeIntFromObj(interp, itemPtr, lengthPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (scriptLen >= 0) {
	    if (*indexPtr < 0) {
		*indexPtr = 0;
	    }
	    if (*lengthPtr < 0) {
		*lengthPtr = 0;
	    }
	    if (*indexPtr >= scriptLen) {
		*indexPtr = scriptLen;
	    }
	    if (*indexPtr + *lengthPtr >= scriptLen) {
		*lengthPtr = scriptLen - *indexPtr;
	    }
	}
    }
    return TCL_OK;
}
