/** @file

  A brief file description

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

/***************************************/
#include "ink_platform.h"
#include "Tokenizer.h"
#include "ink_assert.h"
#include "ink_memory.h"

/****************************************************************************
 *
 *  Tokenizer.cc - A string tokenzier
 *
 *
 *
 ****************************************************************************/        /* MAGIC_EDITING_TAG */

Tokenizer::Tokenizer(const char *StrOfDelimiters)
{
  int length;

  if (StrOfDelimiters == NULL) {
    strOfDelimit = NULL;
  } else {
    length = (int) (strlen(StrOfDelimiters) + 1);
    strOfDelimit = new char[length];
    memcpy(strOfDelimit, StrOfDelimiters, length);
  }

  memset(&start_node, 0, sizeof(tok_node));

  numValidTokens = 0;
  maxTokens = -1;
  options = 0;

  add_node = &start_node;
  add_index = 0;
}

Tokenizer::~Tokenizer()
{
  bool root = true;
  tok_node *cur = &start_node;;
  tok_node *next = NULL;

  if (strOfDelimit != NULL) {
    delete[]strOfDelimit;
  }

  while (cur != NULL) {

    if (options & COPY_TOKS) {
      for (int i = 0; i < TOK_NODE_ELEMENTS; i++)
        ats_free(cur->el[i]);
    }

    next = cur->next;
    if (root == false) {
      ats_free(cur);
    } else {
      root = false;
    }
    cur = next;
  }
}

int
Tokenizer::Initialize(const char *str)
{
  return Initialize((char *) str, COPY_TOKS);
}

inline int
Tokenizer::isDelimiter(char c)
{
  int i = 0;

  while (strOfDelimit[i] != '\0') {
    if (c == strOfDelimit[i]) {
      return 1;
    }
    i++;
  }

  return 0;
}

int
Tokenizer::Initialize(char *str, int opt)
{
  char *strStart;
  int priorCharWasDelimit = 1;
  char *tokStart = NULL;
  int tok_count = 0;
  bool max_limit_hit = false;

  // We can't depend on ReUse() being called so just do it
  //   if the object needs it
  if (numValidTokens > 0) {
    ReUse();
  }

  strStart = str;

  if (!(opt & (COPY_TOKS | SHARE_TOKS))) {
    opt = opt | COPY_TOKS;
  }
  options = opt;

  // Make sure that both options are not set
  ink_assert(!((opt & COPY_TOKS) && (opt & SHARE_TOKS)));


  str = strStart;
  priorCharWasDelimit = 1;

  tok_count = 0;
  tokStart = str;

  while (*str != '\0') {

    // Check to see if we've run to maxToken limit
    if (tok_count + 1 == maxTokens) {
      max_limit_hit = true;
      break;
    }
    // There two modes for collecting tokens
    //
    //  Mode 1: Every delimiter creates a token
    //          even if some of those tokens
    //          are zero length
    //
    //  Mode2:  Every token has some data
    //          in it which means we
    //          to skip past repeated delimiters
    if (options & ALLOW_EMPTY_TOKS) {
      if (isDelimiter(*str)) {
        addToken(tokStart, (int) (str - tokStart));
        tok_count++;
        tokStart = str + 1;
        priorCharWasDelimit = 1;
      } else {
        priorCharWasDelimit = 0;
      }
      str++;
    } else {
      if (isDelimiter(*str)) {
        if (priorCharWasDelimit == 0) {
          // This is a word end, so add it
          addToken(tokStart, (int) (str - tokStart));
          tok_count++;
        }
        priorCharWasDelimit = 1;
      } else {
        if (priorCharWasDelimit == 1) {
          // This is the start of a new token
          tokStart = str;
        }
        priorCharWasDelimit = 0;
      }
      str++;
    }
  }

  // Check to see if we stoped due to a maxToken limit
  if (max_limit_hit == true) {

    if (options & ALLOW_EMPTY_TOKS) {

      // Go till either we hit a delimiter or we've
      //   come to the end of the string, then
      //   set for copying
      for (; *str != '\0' && !isDelimiter(*str); str++);
      priorCharWasDelimit = 0;

    } else {

      // First, skip the delimiters
      for (; *str != '\0' && isDelimiter(*str); str++);

      // If there are only delimiters remaining, bail and set
      //   so that we do not add the empty token
      if (*str == '\0') {
        priorCharWasDelimit = 1;
      } else {
        // There is stuff to copy for the last token
        tokStart = str;
        priorCharWasDelimit = 0;

        // Advance until the end of the string
        for (; *str != '\0'; str++);

        // Now back off all trailing delimiters
        for (; isDelimiter(*(str - 1)); str--);
      }
    }
  }
  // Check to see if we got the last token.  We will
  //  only have gotten it if the string ended with a delimiter
  if (priorCharWasDelimit == 0) {
    // We did not get it
    addToken(tokStart, (int) (str - tokStart));
    tok_count++;
  }

  numValidTokens = tok_count;
  return tok_count;
}


void
Tokenizer::addToken(char *startAddr, int length)
{
  char *add_ptr;
  if (options & SHARE_TOKS) {
    startAddr[length] = '\0';
    add_ptr = startAddr;
  } else {
    add_ptr = (char *)ats_malloc(length + 1);
    memcpy(add_ptr, startAddr, length);
    add_ptr[length] = '\0';
  }

  add_node->el[add_index] = add_ptr;

  add_index++;

  // Check to see if we are out of elements after
  //   adding this one.  If we are change add_node
  //   to point to next tok_node, creating one
  //   if there is not a next one
  if (add_index >= TOK_NODE_ELEMENTS) {
    if (add_node->next == NULL) {
      add_node->next = (tok_node *)ats_malloc(sizeof(tok_node));
      memset(add_node->next, 0, sizeof(tok_node));
    }
    add_node = add_node->next;
    add_index = 0;
  }
}


const char *
Tokenizer::operator[] (int index)
{
  tok_node *
    cur_node = &start_node;
  int
    cur_start = 0;
  if (index >= numValidTokens) {
    return NULL;
  } else {
    while (cur_start + TOK_NODE_ELEMENTS <= index) {
      cur_node = cur_node->next;
      ink_assert(cur_node != NULL);
      cur_start += TOK_NODE_ELEMENTS;
    }
    return cur_node->el[index % TOK_NODE_ELEMENTS];
  }
}

int
Tokenizer::getNumber()
{
  return numValidTokens;
}

const char *
Tokenizer::iterFirst(tok_iter_state * state)
{
  state->node = &start_node;
  state->index = -1;
  return iterNext(state);
}

const char *
Tokenizer::iterNext(tok_iter_state * state)
{
  tok_node *node = state->node;;
  int index = state->index;

  index++;
  if (index >= TOK_NODE_ELEMENTS) {
    node = node->next;
    if (node == NULL) {
      return NULL;
    } else {
      index = 0;
    }
  }

  if (node->el[index] != NULL) {
    state->node = node;
    state->index = index;
    return node->el[index];
  } else {
    return NULL;
  }
}



void
Tokenizer::Print()
{
  tok_node *cur_node = &start_node;
  int node_index = 0;
  int count = 0;

  while (cur_node != NULL) {

    if (cur_node->el[node_index] != NULL) {
      printf("Token %d : |%s|\n", count, cur_node->el[node_index]);
      count++;
    } else {
      return;
    }

    node_index++;
    if (node_index >= TOK_NODE_ELEMENTS) {
      cur_node = cur_node->next;
      node_index = 0;
    }
  }
}

void
Tokenizer::ReUse()
{
  tok_node *cur_node = &start_node;

  while (cur_node != NULL) {
    if (options & COPY_TOKS) {
      for (int i = 0; i < TOK_NODE_ELEMENTS; i++)
        ats_free(cur_node->el[i]);
    }
    memset(cur_node->el, 0, sizeof(char *) * TOK_NODE_ELEMENTS);
    cur_node = cur_node->next;
  }

  numValidTokens = 0;
  add_node = &start_node;
  add_index = 0;
}
