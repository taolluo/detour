/*
    globaliser -- programmatically replace globals in C source code
    Copyright (C) 2003-2006 Sam Jansen

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc., 59
    Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

// $Id$
#include <stdlib.h>
#include <stdio.h>
#include <sstream>
#include <assert.h>
using namespace std;

#include "parser.tab.hh"

// The lexer function generated by flex
extern int lexscan(void);
// The string corresponding to the last token scanned by the lexer
extern char *yytext;
// Our function to check typedef's
extern bool global_is_type(const string& identifier);

struct token_info
{
  int token;
  string token_text, attr_text, lex_text;
};

// Globals accessed in the AST code
string ws_text;
string token_text;
string attr_text;
string lex_text;
//

/* This function checks if `s' is a type name or an identifier.
 */
static int check_identifier(const char *s)
{
  if(global_is_type(s))
    return TYPEDEF_NAME;

  return IDENTIFIER;
}

/* Parses a gcc attribute. Expects that the ATTRIBUTE token has already been
 * consumed. Attributes are not a part of the C grammar, so need to be 
 * pre-parsed out of the input. */
static void parse_attr(std::ostringstream& oa)
{
  int depth = 1;
  int token;

  while ( (token = lexscan()) == WS )
    oa << yytext;
  oa << yytext;

  if (token != '(') {
    assert(0 && "invalid syntax for gcc attribute");
  }

  while (depth > 0) {
    token = lexscan();
    switch (token) {
      case '(': depth++; break;
      case ')': depth--; break;
    };
    oa << yytext;
  }
}

/* Our intermediate function which is called by Bison (or the code generated by
 * Bison anyway).
 *
 * This function handles consuming whitespace and gcc attributes. It saved
 * information about these in global variables which are accessed by AST code.
 * A token is associated with the whitespace and attribute info that occured
 * after that token. Only the first non-whitespace non-attribute token
 * encountered is exempt from this rule. To support this, we scan until we find
 * a non-whitespace non-attribute token, save this token, then return the
 * information scanned.
 */
int yylex(void)
{
  std::ostringstream os, oa, ol;
  int token;
  string yy;

  static struct token_info last_token = { -1, "", "", "" };

  while( ((token = lexscan()) == WS) || token == ATTRIBUTE ) {
    assert(yytext);
    yy = yytext;

    if(token == WS) {
      os << yy;
    } else if(token == ATTRIBUTE) {
      oa << yy;
      parse_attr(oa);
      oa << " ";
      continue;
    }

    ol << yy;
  }

  // If this is the first token, save the information and recurse.
  if (last_token.token == -1) {
    last_token.token = token;
    last_token.token_text = yytext;
    last_token.attr_text = oa.str();
    last_token.lex_text = oa.str() + yytext;

    // Special case here: emit whitespace now. This simplifies handling of ws
    // in the parser.
    fputs(os.str().c_str(), stdout);

    return yylex();
  }

  // Attributes are a hack. Normally, we assume they come *after* what they refer to. In some
  // cases they come before, and this hack deals with that.
  bool attr_hack = (last_token.token == ';') || (last_token.token == '}');

  // Use the saved information to set the global variables
  int tmp_token = last_token.token;
  token_text = last_token.token_text;
  ws_text = os.str();
  attr_text = (attr_hack ? string() : last_token.attr_text) + oa.str();
  lex_text = last_token.lex_text + ol.str() + (attr_hack ? string() : oa.str());

  // Save the currently scanned token for next time
  last_token.token = token;
  last_token.token_text = yytext;
  last_token.attr_text = (attr_hack ? last_token.attr_text + oa.str() : "");
  last_token.lex_text = (attr_hack ? oa.str() : string()) + yytext;

  // Check if the identifier has been typedef'd
  if (tmp_token == IDENTIFIER)
    return check_identifier(token_text.c_str());


  // Return the saved token
  return tmp_token;
}
